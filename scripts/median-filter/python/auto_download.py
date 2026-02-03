from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime
from pathlib import Path
from typing import Iterable

from load_audio import download_audio_segment, search_youtube
from prompt_store import generate_queries


def _day_dir(base_dir: Path) -> Path:
    day = datetime.now().strftime("%Y-%m-%d")
    return base_dir / day


def _sanitize_filename(title: str, fallback: str = "audio_preview") -> str:
    keep = [
        c if (c.isalnum() or c in (" ", "-", "_")) else "_"
        for c in title.strip()
    ]
    stem = "".join(keep).strip(" _") or fallback
    return stem[:80]


def _collect_existing_ids(base_dir: Path) -> set[str]:
    ids: set[str] = set()
    if not base_dir.exists():
        return ids
    for path in base_dir.rglob("*.json"):
        ids.add(path.stem)
    for path in base_dir.rglob("*.wav"):
        ids.add(path.stem)
    return ids


def _iter_queries(
    include_previous: bool,
    history_limit: int,
) -> Iterable[str]:
    queries = generate_queries(
        include_previous=include_previous,
        history_limit=history_limit,
    )
    for query in queries:
        cleaned = query.strip()
        if cleaned:
            yield cleaned


def _download_query(
    query: str,
    output_dir: Path,
    existing_ids: set[str],
    max_length_s: int,
    top_n: int,
) -> None:
    results = search_youtube(query, limit=top_n, max_length_s=max_length_s)
    if not results:
        print(f"No results for query: {query}")
        return

    day_dir = _day_dir(output_dir)
    day_dir.mkdir(parents=True, exist_ok=True)

    for entry in results:
        entry_id = (entry.get("id") or "").strip()
        if entry_id and entry_id in existing_ids:
            print(f"Skipping duplicate: {entry_id} ({entry.get('title')})")
            continue

        url = entry.get("url") or ""
        if not url:
            print(f"Skipping entry without URL: {entry.get('title')}")
            continue

        try:
            audio_segment, metadata = download_audio_segment(url, output_dir=str(day_dir))
        except Exception as exc:
            print(f"Download failed: {entry.get('title')} - {exc}")
            continue

        video_id = metadata.get("id") or entry_id or _sanitize_filename(
            metadata.get("title") or entry.get("title") or "audio_preview"
        )

        wav_path = day_dir / f"{video_id}.wav"
        json_path = day_dir / f"{video_id}.json"

        audio_segment.export(wav_path, format="wav")
        with json_path.open("w", encoding="utf-8") as f:
            json.dump(metadata, f, indent=2, ensure_ascii=False)

        existing_ids.add(video_id)
        print(f"Saved: {wav_path.name} ({audio_segment.duration_seconds:.1f}s)")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Continuously fetch queries and download short YouTube audio clips."
    )
    parser.add_argument("--max-length-s", type=int, default=30)
    parser.add_argument("--top-n", type=int, default=10)
    parser.add_argument("--history-limit", type=int, default=300)
    parser.add_argument("--output-dir", type=Path, default=Path("./downloads"))
    parser.add_argument("--once", action="store_true", help="Run a single batch and exit.")
    parser.add_argument("--no-history", action="store_true", help="Do not include previous queries.")

    args = parser.parse_args(argv)
    if args.max_length_s <= 0:
        raise ValueError("--max-length-s must be positive")
    if args.top_n <= 0:
        raise ValueError("--top-n must be positive")
    if args.history_limit <= 0:
        raise ValueError("--history-limit must be positive")

    output_dir = args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)
    existing_ids = _collect_existing_ids(output_dir)

    include_previous = not args.no_history

    while True:
        for query in _iter_queries(include_previous, args.history_limit):
            _download_query(
                query=query,
                output_dir=output_dir,
                existing_ids=existing_ids,
                max_length_s=args.max_length_s,
                top_n=args.top_n,
            )
        if args.once:
            break

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
