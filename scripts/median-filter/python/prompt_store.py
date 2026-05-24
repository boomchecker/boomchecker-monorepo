from pathlib import Path
from datetime import datetime
import hashlib
import json
import os
from openai import OpenAI
from pydantic import ValidationError

ROOT = Path(__file__).resolve().parent.parent
PYTHON_DIR = Path(__file__).resolve().parent

DOTENV_PATH = PYTHON_DIR / ".env"

DATA_DIR = ROOT / "data"
PREVIOUS_QUERIES_FILE = DATA_DIR / "previous_queries.txt"
RESPONSES_LOG = DATA_DIR / "openai_responses.jsonl"

_ENV_LOADED = False


def load_env() -> None:
    """
    Best-effort loader for the local `.env` file so the script can be run
    standalone without exporting variables manually.
    """
    global _ENV_LOADED
    if _ENV_LOADED:
        return

    try:
        text = DOTENV_PATH.read_text(encoding="utf-8")
    except FileNotFoundError:
        _ENV_LOADED = True
        return
    except OSError:
        _ENV_LOADED = True
        return

    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        if not key or key in os.environ:
            continue
        value = value.strip().strip("\"'")
        os.environ[key] = value

    _ENV_LOADED = True


def parse_queries(raw_text: str) -> list[str]:
    """  
    Split raw OpenAI output into individual, non-empty queries.  

    Tries to parse as a JSON array first. If that fails, splits by newlines.  
    If queries are comma-separated, this will not handle queries containing commas.  
    Prefer newline-separated or JSON array output for robustness.  
    """  
    if not isinstance(raw_text, str):  
        print("Warning: raw_text is not a string: %s" % type(raw_text))  
        return []  
    # Try to parse as JSON array  
    try:  
        parsed = json.loads(raw_text)  
        if isinstance(parsed, list):  
            return [q for q in parsed if isinstance(q, str) and q.strip()]  
    except Exception:  
        pass  
    # Fallback: split by newlines  
    queries = [q.strip() for q in raw_text.splitlines() if q.strip()]  
    if queries:  
        return queries  
    # Last resort: original comma splitting (deprecated, not robust)  
    return [q for q in raw_text.replace(", ", ",").split(",") if q.strip()]  

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


def load_prompt(include_recent: bool = False) -> str:
    """
    Return the stored base prompt, optionally appending recent queries so the
    model can avoid repeating them.
    """
    ensure_files()

    base_prompt = ""

    if include_recent:
        recent = get_recent_queries(limit=200)
        if recent:
            previous = "\n".join(f"- {q}" for q in recent)
            base_prompt = (
                f"{base_prompt}\n\nPreviously suggested queries (do not repeat):\n{previous}"
            )
    return base_prompt


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
        "queries_sha256": hashlib.sha256(raw_text.encode("utf-8")).hexdigest() if queries else None,  
    }
    with RESPONSES_LOG.open("a", encoding="utf-8") as f:
        f.write(json.dumps(record, ensure_ascii=False) + "\n")
    return record


def generate_queries(
    model: str = "gpt-4.1-mini",
    include_previous: bool = False,
) -> list[str]:
    """
    Ultra-simple helper that just sends the stored prompt to the OpenAI Chat Completions  
    API and returns the raw text (split by lines). No structured parsing or file
    uploads so it is easier to iterate during development.
    """
    load_env()
    ensure_files()
    api_key = os.environ.get("OPENAI_API_KEY")
    if not api_key:
        raise ValueError(
            "OPENAI_API_KEY environment variable is not set. "
            "Please set it with your OpenAI API key."
        )

    # Use hosted custom prompt if available, otherwise fall back to local prompt text.
    prompt_id = os.environ.get("OPENAI_PROMPT_ID", "").strip()  
    prompt_ver = os.environ.get("OPENAI_PROMPT_VER", "").strip()  
    client = OpenAI(api_key=api_key)

    request: dict = {"model": model}
    if prompt_id:
        request["prompt"] = {"id": prompt_id, "version": prompt_ver}

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

    parsed_response = parse_queries(output_text)

    # Log the response
    log_openai_response(parsed_response, model=model, prompt_id=prompt_id)

    return parsed_response


if __name__ == "__main__":
    print(generate_queries(include_previous=True))

