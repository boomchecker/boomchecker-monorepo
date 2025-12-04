from pathlib import Path
from datetime import datetime
import hashlib
import json
import os
from openai import OpenAI
from pydantic import BaseModel, ValidationError

ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = ROOT / "data"
PREVIOUS_QUERIES_FILE = DATA_DIR / "previous_queries.txt"
RESPONSES_LOG = DATA_DIR / "openai_responses.jsonl"
DEFAULT_PROMPT_ID = "pmpt_6931a981604c8193bb4dece7b4bb6e850f65a6ac4f7424e8"


def parse_queries(raw_text: str) -> list[str]:
    """Split raw OpenAI output into individual, non-empty queries."""
    if type(raw_text) != str:
        print("Warning: raw_text is not a string: %s" % type(raw_text))
        return []
    return generate_queries()[0].split(",")

def ensure_files():
    """Ensure prompt and data directories exist with a default prompt."""
    DATA_DIR.mkdir(parents=True, exist_ok=True)

    if not PREVIOUS_QUERIES_FILE.exists():
        PREVIOUS_QUERIES_FILE.write_text("", encoding="utf-8")
    
    if not RESPONSES_LOG.exists():
        RESPONSES_LOG.write_text("", encoding="utf-8")

def get_recent_queries(limit: int = 200) -> list[str]:
    """
    Return up to `limit` most recent distinct queries from the responses log,
    preserving recency (latest first).
    """
    ensure_files()

    seen = set()
    recent: list[str] = []
    try:
        lines = RESPONSES_LOG.read_text(encoding="utf-8").splitlines()
    except OSError:
        return []

    for line in reversed(lines):
        try:
            obj = json.loads(line)
        except json.JSONDecodeError:
            continue
        for q in reversed(obj.get("queries", []) or []):
            if q not in seen:
                seen.add(q)
                recent.append(q)
                if len(recent) >= limit:
                    return recent
    return recent


def log_openai_response(queries: list[str], model: str, prompt_id: str) -> dict:
    """
    Store OpenAI response to JSONL with queries.
    Returns the stored record.
    """
    ensure_files()
    raw_text = "\n".join(queries)
    record = {
        "ts": datetime.utcnow().isoformat(timespec="seconds") + "Z",
        "model": model,
        "prompt_id": prompt_id,
        "raw": raw_text,
        "queries": queries,
        "queries_sha256": _hash(raw_text) if queries else None,
    }
    with RESPONSES_LOG.open("a", encoding="utf-8") as f:
        f.write(json.dumps(record, ensure_ascii=False) + "\n")
    return record


def generate_queries(
    model: str = "gpt-4.1-mini",
    include_previous: bool = False,
) -> list[str]:
    """
    Ultra-simple helper that just sends the stored prompt to the OpenAI Responses
    API and returns the raw text (split by lines). No structured parsing or file
    uploads so it is easier to iterate during development.
    """
    ensure_files()
    api_key = os.environ.get("OPENAI_API_KEY")
    if not api_key:
        raise ValueError(
            "OPENAI_API_KEY environment variable is not set. "
            "Please set it with your OpenAI API key."
        )

    # Use hosted custom prompt if available, otherwise fall back to local prompt text.
    prompt_id = (
        os.environ.get("OPENAI_PROMPT_ID") or DEFAULT_PROMPT_ID or ""
    ).strip()
    client = OpenAI(api_key=api_key)

    request: dict = {"model": model}
    if prompt_id:
        request["prompt"] = {"id": prompt_id}

    prompt = load_prompt(include_recent=include_previous)
    request["input"] = [
        {
            "role": "user",
            "content": [
                {
                    "type": "input_text",
                    "text": prompt,
                }
            ],
        }
    ]

    response = client.responses.create(**request)

    output_text = (getattr(response, "output_text", None) or "").strip()
    if not output_text:
        raise RuntimeError("OpenAI returned empty output")

    # Log the response
    log_openai_response(parse_queries(output_text), model=model, prompt_id=prompt_id)

    return output_text.splitlines()


if __name__ == "__main__":
    print(generate_queries())

