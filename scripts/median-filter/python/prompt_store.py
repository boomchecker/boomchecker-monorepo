from pathlib import Path
from datetime import datetime
import hashlib
import json

ROOT = Path(__file__).resolve().parent.parent
DATA_DIR = ROOT / "data"
PROMPT_PATH = DATA_DIR / "prompt.txt"
RESPONSES_LOG = DATA_DIR / "openai_responses.jsonl"

DEFAULT_PROMPT = (
    "You are an assistant that generates concise, high-quality YouTube search "
    "queries to find videos containing impulsive acoustic events suitable for "
    "audio analysis (e.g., gunshots, blank rounds, starter pistols, fireworks, "
    "metal impacts). Provide queries in English and optionally in other languages "
    "to broaden the search. Focus on real recordings, avoid generic sound effects. "
    "OUTPUT FORMAT: Respond with search queries ONLY — one per line, no explanations, "
    "no extra text, no commentary."
)


def ensure_files():
    """Ensure prompt and data directories exist with a default prompt."""
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    if not PROMPT_PATH.exists():
        PROMPT_PATH.write_text(DEFAULT_PROMPT, encoding="utf-8")
    # Responses log is append-only JSONL; created lazily when first written.


def load_prompt(include_recent: bool = False, recent_limit: int = 200) -> str:
    """
    Load the current prompt text. Optionally appends a section with the most recent
    distinct queries (up to recent_limit) to discourage repetition.
    """
    ensure_files()
    base = PROMPT_PATH.read_text(encoding="utf-8").strip()
    if not include_recent:
        return base

    recent = get_recent_queries(limit=recent_limit)
    if not recent:
        return base

    recent_block = "\n\nPreviously suggested queries (do not repeat):\n" + "\n".join(
        f"- {q}" for q in recent
    )
    return f"{base}{recent_block}"


def _hash(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def parse_queries(raw_text: str) -> list[str]:
    """Split raw OpenAI output into individual, non-empty queries."""
    return [line.strip() for line in raw_text.splitlines() if line.strip()]


def get_recent_queries(limit: int = 200) -> list[str]:
    """
    Return up to `limit` most recent distinct queries from the responses log,
    preserving recency (latest first).
    """
    ensure_files()
    if not RESPONSES_LOG.exists():
        return []

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


def log_openai_response(raw_text: str, prompt_text: str | None = None, model: str | None = None) -> dict:
    """
    Store OpenAI response to JSONL with both raw text and parsed queries.
    Returns the stored record.
    """
    ensure_files()
    prompt = prompt_text or load_prompt()
    queries = parse_queries(raw_text)
    record = {
        "ts": datetime.utcnow().isoformat(timespec="seconds") + "Z",
        "model": model,
        "prompt_sha256": _hash(prompt),
        "prompt": prompt,
        "raw": raw_text,
        "queries": queries,
        "queries_sha256": _hash("\n".join(queries)) if queries else None,
    }
    with RESPONSES_LOG.open("a", encoding="utf-8") as f:
        f.write(json.dumps(record, ensure_ascii=False) + "\n")
    return record


if __name__ == "__main__":
    print(load_prompt())
