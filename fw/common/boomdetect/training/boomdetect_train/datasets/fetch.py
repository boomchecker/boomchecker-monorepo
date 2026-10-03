"""Download the HuggingFace shards, one at a time and resumably.

The set is 8.3 GB in 39 parquet shards, which is too much to hold in one
request and too much to start again from zero when a connection drops. So each
shard is written to a `.part` beside its destination and a restart continues it
with a Range request; a shard whose size already matches what the server
reports is left alone. That makes the command safe to run again at any point,
which is the only way an 8 GB download over a home line is going to finish.

The shards are not interchangeable. 00-03 carry the long negatives (ESC-50,
UrbanSound8K, TUT: 3.7 GB, average clip 7.3 s) and 04-38 the half-second drone
clips, so a partial download is a skewed dataset rather than a smaller one.
`bdtrain manifest` reports what is actually present.
"""

from __future__ import annotations

import time
import urllib.error
import urllib.request
from pathlib import Path

from boomdetect_train.paths import raw_dir

SHARD_COUNT = 39
HF_REPO = "geronimobasso/drone-audio-detection-samples"
HF_URL = f"https://huggingface.co/datasets/{HF_REPO}/resolve/main/data/"
USER_AGENT = "boomdetect-train/0.1 (+fw/common/boomdetect/training)"
CHUNK = 1 << 20
RETRIES = 5


def shard_name(i: int) -> str:
    return f"train-{i:05d}-of-{SHARD_COUNT:05d}.parquet"


def hf_dir() -> Path:
    return raw_dir() / "hf-drone-audio-detection-samples"


def _request(url: str, start: int = 0) -> urllib.request.Request:
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    if start:
        req.add_header("Range", f"bytes={start}-")
    return req


def remote_size(url: str) -> int | None:
    """Content-Length of the final resource, following the CDN redirect."""
    try:
        with urllib.request.urlopen(_request(url), timeout=60) as r:
            n = r.headers.get("Content-Length")
            return int(n) if n else None
    except urllib.error.URLError:
        return None


def fetch_one(name: str, dest_dir: Path, log=print) -> Path:
    """One shard, resuming a `.part` if there is one. Returns the finished file."""
    url = HF_URL + name
    dest, part = dest_dir / name, dest_dir / (name + ".part")
    total = remote_size(url)
    if dest.exists() and (total is None or dest.stat().st_size == total):
        log(f"{name}: present ({dest.stat().st_size / 1e6:.0f} MB), skipping")
        return dest

    for attempt in range(1, RETRIES + 1):
        have = part.stat().st_size if part.exists() else 0
        if total is not None and have >= total:
            break
        try:
            with urllib.request.urlopen(_request(url, have), timeout=120) as r:
                # A server that ignores Range answers 200 and restarts the body.
                if have and r.status != 206:
                    have, mode = 0, "wb"
                else:
                    mode = "ab" if have else "wb"
                t0, last = time.time(), have
                with open(part, mode) as f:
                    while True:
                        buf = r.read(CHUNK)
                        if not buf:
                            break
                        f.write(buf)
                        have += len(buf)
                        if time.time() - t0 > 20:
                            rate = (have - last) / (time.time() - t0) / 1e6
                            pct = f" {100.0 * have / total:.0f}%" if total else ""
                            log(f"{name}:{pct} {have / 1e6:.0f} MB, {rate:.1f} MB/s")
                            t0, last = time.time(), have
            break
        except (urllib.error.URLError, TimeoutError, ConnectionError, OSError) as e:
            if attempt == RETRIES:
                raise
            log(f"{name}: {type(e).__name__} {e}, retry {attempt}/{RETRIES} in 10 s")
            time.sleep(10)

    size = part.stat().st_size if part.exists() else 0
    if total is not None and size != total:
        raise OSError(f"{name}: got {size} bytes, expected {total}")
    part.replace(dest)
    log(f"{name}: done ({size / 1e6:.0f} MB)")
    return dest


def fetch_all(
    dest_dir: Path | None = None,
    shards: list[int] | None = None,
    log=print,
) -> list[Path]:
    """Every shard (or the listed ones) into the raw tree, skipping what is there."""
    d = dest_dir if dest_dir is not None else hf_dir()
    d.mkdir(parents=True, exist_ok=True)
    want = shards if shards is not None else list(range(SHARD_COUNT))
    out: list[Path] = []
    for i, n in enumerate(want, 1):
        log(f"[{i}/{len(want)}] {shard_name(n)}")
        out.append(fetch_one(shard_name(n), d, log=log))
    have = sum(p.stat().st_size for p in out)
    log(f"{len(out)} shards, {have / 1e9:.2f} GB under {d}")
    return out
