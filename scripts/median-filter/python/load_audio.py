"""
Audio/prompt orchestration placeholder.

Step 1: manage the initial OpenAI search prompt storage (no API calls yet).
"""

from prompt_store import load_prompt, ensure_files  # local module


def load_audio(path: str):
    """Stub for audio loading."""
    raise NotImplementedError("Audio loader not implemented yet.")


def get_search_prompt() -> str:
    """Return the base prompt used to ask OpenAI for search queries."""
    return load_prompt()


# Ensure prompt file exists at import time for convenience.
ensure_files()
