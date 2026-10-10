"""Number formatting for the generated LaTeX files (Czech decimal comma)."""

from __future__ import annotations


def czech(x: float, digits: int = 1) -> str:
    """``x`` with ``digits`` decimals and a braced comma, e.g. ``0{,}4`` (no spacing after it)."""
    return f"{x:.{digits}f}".replace(".", "{,}")
