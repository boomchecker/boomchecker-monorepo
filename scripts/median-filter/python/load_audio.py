"""
Audio/prompt orchestration placeholder.

Step 1: manage the initial OpenAI search prompt storage (no API calls yet).
Step 2: prototype YouTube scraping helpers (search, download, etc.).
"""

from __future__ import annotations

import tempfile
from pathlib import Path
from typing import Dict, List, Optional, Tuple

try:
    from pytube import Search
except ImportError:  # pragma: no cover - helpful message if dependency missing
    Search = None

try:
    from yt_dlp import YoutubeDL
except ImportError:  # pragma: no cover
    YoutubeDL = None

try:
    from pydub import AudioSegment
except ImportError:  # pragma: no cover
    AudioSegment = None

from prompt_store import load_prompt, ensure_files  # local module


def load_audio(path: str):
    """Stub for audio loading."""
    raise NotImplementedError("Audio loader not implemented yet.")


def get_search_prompt() -> str:
    """Return the base prompt used to ask OpenAI for search queries."""
    return load_prompt()


def search_youtube(query: str, limit: int = 20) -> List[Dict[str, str]]:
    """
    Use pytube's lightweight search helper to pull back metadata for the first
    `limit` results. Returns dictionaries with title and watch_url that can be
    consumed by later scraping/downloading stages.
    """
    if not query:
        return []
    if Search is None:
        raise RuntimeError(
            "pytube is not installed. Run `pip install pytube` inside the devcontainer."
        )
    if limit <= 0:
        return []

    search = Search(query)
    results: List[Dict[str, str]] = []
    consumed = 0

    while len(results) < limit:
        # Ensure we have fresh results before consuming.
        if consumed >= len(search.results):
            if getattr(search, "has_more_results", False):
                search.get_next_results()
            else:
                break

        for video in search.results[consumed:]:
            consumed += 1
            title = getattr(video, "title", "").strip() or "Untitled video"
            url = getattr(video, "watch_url", None) or getattr(video, "watch_url", "")
            if not url:
                video_id = getattr(video, "video_id", "")
                if video_id:
                    url = f"https://www.youtube.com/watch?v={video_id}"
            results.append(
                {
                    "title": title,
                    "url": url,
                }
            )
            if len(results) >= limit:
                break

    return results


def download_audio_segment(video_url: str) -> AudioSegment:
    """
    Download the best available audio/best format using yt-dlp and return it as
    a pydub AudioSegment for further processing.
    """
    if not video_url:
        raise ValueError("video_url must not be empty")
    if YoutubeDL is None or AudioSegment is None:
        raise RuntimeError(
            "Missing dependency. Install `yt-dlp` and `pydub` (plus ffmpeg) to download audio."
        )

    with tempfile.TemporaryDirectory(prefix="yt-audio-") as tmpdir:
        tmpdir_path = Path(tmpdir)
        outtmpl = str(tmpdir_path / "%(id)s.%(ext)s")
        ydl_opts = {
            "format": "bestaudio/best",
            "outtmpl": outtmpl,
            "noplaylist": True,
            "quiet": True,
            "no_warnings": True,
            "cachedir": False,
            # Add user agent and other headers to avoid 403
            "http_headers": {
                "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
                "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
                "Accept-Language": "en-us,en;q=0.5",
                "Sec-Fetch-Mode": "navigate",
            },
            # Use extractor args for YouTube
            "extractor_args": {
                "youtube": {
                    "player_client": ["android", "web"],
                    "player_skip": ["webpage", "configs"],
                }
            },
        }
        try:
            with YoutubeDL(ydl_opts) as ydl:
                info = ydl.extract_info(video_url, download=True)
                downloaded_file = Path(ydl.prepare_filename(info))
        except Exception as exc:  # pragma: no cover - network dependent
            raise RuntimeError(f"yt-dlp failed to download audio: {exc}") from exc

        if not downloaded_file.exists():
            raise RuntimeError("yt-dlp reported success but file not found")

        return AudioSegment.from_file(downloaded_file)


def _sanitize_filename(title: str, fallback: str = "audio_preview") -> str:
    """Return filesystem-friendly stem derived from `title`."""
    keep = [
        c if (c.isalnum() or c in (" ", "-", "_")) else "_"
        for c in title.strip()
    ]
    stem = "".join(keep).strip(" _") or fallback
    return stem[:80]


def _demo_search_and_download():
    """Simple helper to exercise search_youtube + download_audio_segment."""
    sample_query = "clash of steel hammer impact sound"
    videos = search_youtube(sample_query, limit=20)
    print(f"Top {len(videos)} results for '{sample_query}':")
    for idx, video in enumerate(videos, 1):
        print(f"{idx:02d}. {video['title']}\n    {video['url']}")

    if not videos:
        print("No videos found; aborting audio download demo.")
        return

    audio_segment: Optional[AudioSegment] = None
    chosen_video: Optional[Dict[str, str]] = None
    last_error: Optional[Exception] = None

    for video in videos:
        print(f"\nAttempting download: {video['title']}")
        try:
            audio_segment = download_audio_segment(video["url"])
            chosen_video = video
            print(f"  Downloaded '{video['title']}' successfully; stopping search.")
        except Exception as exc:  # pragma: no cover - network dependent
            last_error = exc
            print(f"  Failed to download '{video['title']}': {exc}")

        if audio_segment is None or chosen_video is None:
            raise RuntimeError(
                "Unable to download audio for any search result"
            ) from last_error

        safe_stem = _sanitize_filename(chosen_video["title"])
        output_path = Path(__file__).with_name(f"{safe_stem}.wav")
        audio_segment.export(output_path, format="wav")
        duration = audio_segment.duration_seconds
        print(
            f"Saved WAV preview to {output_path} ({duration:.1f}s) "
            f"from '{chosen_video['title']}'."
        )


if __name__ == "__main__":
    _demo_search_and_download()


# Ensure prompt file exists at import time for convenience.
ensure_files()
