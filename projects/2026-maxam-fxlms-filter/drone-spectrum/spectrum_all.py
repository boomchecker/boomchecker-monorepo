#!/usr/bin/env python3
"""
spectrum_all.py — average PSD across every recording in the DADS dataset.

Processes all parquet shards in parallel. Each worker streams one shard and
accumulates a running PSD sum (mean = sum / n) plus a reservoir sample for
percentile bands. Checkpoint files are saved after each shard so the run can
be resumed after a crash or interruption.

Usage (from scripts/lms-filter):
  task spectrum-all
  task spectrum-all -- --workers 6 --start-shard 0 --end-shard 10
"""
from __future__ import annotations

import argparse
import io
import json
import math
import os
import pickle
import random
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import numpy as np
from scipy.signal import resample_poly, welch


DEFAULT_DATASET = "geronimobasso/drone-audio-detection-samples"
SCRIPT_DIR = Path(__file__).resolve().parent


# --------------------------------------------------------------------------- #
# Accumulators                                                                 #
# --------------------------------------------------------------------------- #


@dataclass
class LabelAcc:
    """Per-label running statistics for one or more shards."""

    n: int = 0
    sum_psd: np.ndarray | None = None
    sum_log_psd: np.ndarray | None = None  # for geometric mean (dB-domain mean)
    freqs: np.ndarray | None = None
    reservoir: list[np.ndarray] = field(default_factory=list)
    total_duration_s: float = 0.0
    skipped: int = 0
    # reservoir_capacity tracks how many items we want to keep total
    _reservoir_capacity: int = field(default=0, repr=False)

    def add(self, freqs: np.ndarray, psd: np.ndarray, duration_s: float) -> None:
        log_psd = np.log10(psd)
        if self.sum_psd is None:
            self.sum_psd = psd.copy()
            self.sum_log_psd = log_psd.copy()
            self.freqs = freqs.copy()
        else:
            self.sum_psd += psd
            self.sum_log_psd += log_psd
        self.n += 1
        self.total_duration_s += duration_s

        # Algorithm R reservoir sampling
        cap = self._reservoir_capacity
        if cap > 0:
            if len(self.reservoir) < cap:
                self.reservoir.append(psd.copy())
            else:
                j = random.randint(0, self.n - 1)
                if j < cap:
                    self.reservoir[j] = psd.copy()

    def merge(self, other: "LabelAcc") -> None:
        """Merge another accumulator into self (after parallel shard run)."""
        if other.n == 0:
            return
        if self.sum_psd is None:
            self.sum_psd = other.sum_psd.copy()
            self.sum_log_psd = other.sum_log_psd.copy()
            self.freqs = other.freqs.copy()
        else:
            self.sum_psd += other.sum_psd
            self.sum_log_psd += other.sum_log_psd
        self.n += other.n
        self.total_duration_s += other.total_duration_s
        self.skipped += other.skipped
        self.reservoir.extend(other.reservoir)

    def mean_psd(self) -> np.ndarray:
        """Arithmetic mean in linear power — dominated by loud outliers."""
        if self.n == 0:
            raise ValueError("No samples")
        return self.sum_psd / self.n

    def mean_db(self) -> np.ndarray:
        """Geometric mean: average in log domain, consistent with dB percentiles."""
        if self.n == 0:
            raise ValueError("No samples")
        return 10.0 * (self.sum_log_psd / self.n)


@dataclass
class ShardResult:
    shard_idx: int
    per_label: dict[str, LabelAcc]
    rows_scanned: int
    wall_s: float
    error: str | None = None


# --------------------------------------------------------------------------- #
# Helpers                                                                      #
# --------------------------------------------------------------------------- #


def load_dotenv(path: Path) -> None:
    if not path.exists():
        return
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip().strip("\"'")
        if key and key not in os.environ:
            os.environ[key] = value


def normalize_label(value: Any) -> str:
    if isinstance(value, bytes):
        value = value.decode("utf-8", errors="replace")
    if isinstance(value, (np.integer, int)):
        return str(int(value))
    if isinstance(value, (np.floating, float)):
        n = float(value)
        if math.isfinite(n) and n.is_integer():
            return str(int(n))
        return str(n)
    return str(value)


def audio_to_array(audio_value: Any) -> tuple[np.ndarray, int]:
    import soundfile as sf

    if isinstance(audio_value, dict):
        if audio_value.get("array") is not None:
            return np.asarray(audio_value["array"], dtype=np.float32), int(audio_value["sampling_rate"])
        if audio_value.get("bytes") is not None:
            arr, sr = sf.read(io.BytesIO(audio_value["bytes"]), dtype="float32")
            return arr, sr
        if audio_value.get("path") is not None:
            return sf.read(audio_value["path"], dtype="float32")
    if isinstance(audio_value, (str, Path)):
        return sf.read(str(audio_value), dtype="float32")
    if isinstance(audio_value, (bytes, bytearray)):
        return sf.read(io.BytesIO(audio_value), dtype="float32")
    arr = getattr(audio_value, "array", None)
    sr = getattr(audio_value, "sampling_rate", None)
    if arr is None or sr is None:
        raise ValueError("Cannot extract audio from value")
    return np.asarray(arr, dtype=np.float32), int(sr)


def resample_if_needed(audio: np.ndarray, sr: int, target_sr: int) -> np.ndarray:
    if sr == target_sr:
        return audio
    gcd = math.gcd(sr, target_sr)
    return resample_poly(audio, target_sr // gcd, sr // gcd).astype(np.float32)


def trim_center(audio: np.ndarray, sr: int, max_s: float) -> np.ndarray:
    max_n = int(round(max_s * sr))
    if max_n <= 0 or audio.size <= max_n:
        return audio
    start = (audio.size - max_n) // 2
    return audio[start : start + max_n]


def compute_psd(audio: np.ndarray, sr: int, nperseg: int) -> tuple[np.ndarray, np.ndarray]:
    audio = audio - float(np.mean(audio))
    seg = min(nperseg, audio.size)
    freqs, psd = welch(audio, fs=sr, window="hann", nperseg=seg, noverlap=seg // 2,
                       detrend=False, scaling="density")
    return freqs.astype(np.float32), np.maximum(psd.astype(np.float64), 1e-20)


# --------------------------------------------------------------------------- #
# Worker (runs in subprocess via spawn)                                        #
# --------------------------------------------------------------------------- #


def _process_shard(kw: dict) -> ShardResult:
    shard_idx: int = kw["shard_idx"]
    shard_count: int = kw["shard_count"]
    t0 = time.monotonic()
    data_file = f"data/train-{shard_idx:05d}-of-{shard_count:05d}.parquet"

    accs: dict[str, LabelAcc] = {}
    rows_scanned = 0

    try:
        from datasets import Audio, load_dataset

        ds = load_dataset(
            kw["dataset"],
            data_files=[data_file],
            split=kw["split"],
            streaming=True,
            token=kw["token"],
        )
        try:
            ds = ds.cast_column(kw["audio_col"], Audio(decode=False))
        except Exception:
            pass

        target_labels: set[str] | None = kw["target_labels"]
        reservoir_cap: int = kw["reservoir_per_shard"]
        target_sr: int = kw["target_sr"]
        nperseg: int = kw["nperseg"]
        max_s: float = kw["max_duration_s"]
        min_s: float = kw["min_duration_s"]
        audio_col: str = kw["audio_col"]
        label_col: str = kw["label_col"]

        for row in ds:
            rows_scanned += 1
            label = normalize_label(row[label_col])
            if target_labels is not None and label not in target_labels:
                continue
            if label not in accs:
                acc = LabelAcc()
                acc._reservoir_capacity = reservoir_cap
                accs[label] = acc
            acc = accs[label]
            try:
                audio, sr = audio_to_array(row[audio_col])
                if audio.ndim > 1:
                    audio = np.mean(audio, axis=-1, dtype=np.float32)
                audio = resample_if_needed(audio, sr, target_sr)
                audio = trim_center(audio, target_sr, max_s)
                dur = audio.size / float(target_sr)
                if dur < min_s:
                    acc.skipped += 1
                    continue
                freqs, psd = compute_psd(audio, target_sr, nperseg)
                acc.add(freqs, psd, dur)
            except Exception:
                acc.skipped += 1

    except Exception as exc:
        return ShardResult(
            shard_idx=shard_idx,
            per_label=accs,
            rows_scanned=rows_scanned,
            wall_s=time.monotonic() - t0,
            error=str(exc),
        )

    return ShardResult(
        shard_idx=shard_idx,
        per_label=accs,
        rows_scanned=rows_scanned,
        wall_s=time.monotonic() - t0,
    )


# --------------------------------------------------------------------------- #
# Checkpoint helpers                                                           #
# --------------------------------------------------------------------------- #


def _ckpt_path(ckpt_dir: Path, shard_idx: int) -> Path:
    return ckpt_dir / f"shard_{shard_idx:05d}.pkl"


def save_checkpoint(ckpt_dir: Path, result: ShardResult) -> None:
    ckpt_dir.mkdir(parents=True, exist_ok=True)
    with open(_ckpt_path(ckpt_dir, result.shard_idx), "wb") as f:
        pickle.dump(result, f, protocol=pickle.HIGHEST_PROTOCOL)


def load_checkpoint(ckpt_dir: Path, shard_idx: int) -> ShardResult | None:
    p = _ckpt_path(ckpt_dir, shard_idx)
    if not p.exists():
        return None
    with open(p, "rb") as f:
        result: ShardResult = pickle.load(f)
    # Reject stale checkpoints missing fields added in later versions.
    for acc in result.per_label.values():
        if getattr(acc, "sum_log_psd", None) is None and acc.n > 0:
            return None
    return result


# --------------------------------------------------------------------------- #
# Plotting                                                                     #
# --------------------------------------------------------------------------- #


def plot_results(
    global_accs: dict[str, LabelAcc],
    args: argparse.Namespace,
    metadata: dict[str, Any],
) -> None:
    args.output_dir.mkdir(parents=True, exist_ok=True)
    os.environ.setdefault("MPLCONFIGDIR", str(args.output_dir / ".mplconfig"))
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    colors = {"0": "#3b82f6", "1": "#ef4444"}
    names = {"0": "no-drone", "1": "drone"}

    fig, ax = plt.subplots(figsize=(12, 6))
    for label, acc in sorted(global_accs.items()):
        if acc.n == 0:
            continue
        freqs = acc.freqs
        mean_db = acc.mean_db()  # geometric mean — consistent with dB percentiles
        color = colors.get(label, "#6b7280")
        name = names.get(label, f"label={label}")

        if len(acc.reservoir) >= 4:
            stack = np.vstack(acc.reservoir)
            db_stack = 10.0 * np.log10(np.maximum(stack, 1e-20))
            low_db = np.percentile(db_stack, args.percentile_low, axis=0)
            high_db = np.percentile(db_stack, args.percentile_high, axis=0)
            ax.fill_between(freqs, low_db, high_db, color=color, alpha=0.18, linewidth=0,
                            label=f"{name} {args.percentile_low:.0f}–{args.percentile_high:.0f}th pct")

        ax.plot(freqs, mean_db, color=color, linewidth=1.6,
                label=f"{name} mean (n={acc.n:,}, {acc.total_duration_s / 3600:.1f} h)")

    ax.set_xlim(0, args.target_sr / 2)
    ax.set_xlabel("Frequency [Hz]", fontsize=20)
    ax.set_ylabel("PSD [dB/Hz]", fontsize=20)
    ax.tick_params(axis="both", labelsize=20)
    ax.grid(True, alpha=0.25)
    ax.set_ylim([-100,-30])
    ax.legend(loc="best", fontsize=20)
    fig.tight_layout()

    fig_path = args.output_dir / f"{args.output_name}.png"
    fig.savefig(fig_path, dpi=180)
    plt.close(fig)
    print(f"Plot saved: {fig_path}")

    metrics_path = args.output_dir / f"{args.output_name}_metrics.json"
    metrics_path.write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    print(f"Metrics saved: {metrics_path}")


# --------------------------------------------------------------------------- #
# CLI                                                                          #
# --------------------------------------------------------------------------- #


def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--dataset", default=DEFAULT_DATASET)
    p.add_argument("--split", default="train")
    p.add_argument("--shard-count", type=int, default=39)
    p.add_argument("--start-shard", type=int, default=0)
    p.add_argument("--end-shard", type=int, default=None,
                   help="Exclusive end shard. Defaults to --shard-count.")
    p.add_argument("--workers", type=int, default=4,
                   help="Parallel shard workers. Each opens its own HF stream.")
    p.add_argument("--audio-column", default="audio")
    p.add_argument("--label-column", default="label")
    p.add_argument("--labels", default="0,1",
                   help="Comma-separated labels to include. Default: '0,1'.")
    p.add_argument("--target-sr", type=int, default=16_000)
    p.add_argument("--nperseg", type=int, default=2048)
    p.add_argument("--max-duration-s", type=float, default=5.0)
    p.add_argument("--min-duration-s", type=float, default=0.25)
    p.add_argument("--reservoir-per-shard", type=int, default=200,
                   help="PSDs kept per shard per label for percentile bands. "
                        "Total reservoir ≈ reservoir-per-shard × num_shards.")
    p.add_argument("--percentile-low", type=float, default=25.0)
    p.add_argument("--percentile-high", type=float, default=75.0)
    p.add_argument("--anc-max-hz", type=float, default=1000.0)
    p.add_argument("--checkpoint-dir", type=Path, default=SCRIPT_DIR / "checkpoints")
    p.add_argument("--no-checkpoint", action="store_true",
                   help="Disable checkpoint save/load (recompute every shard).")
    p.add_argument("--output-dir", type=Path, default=SCRIPT_DIR / "out")
    p.add_argument("--output-name", default="dads_average_spectrum")
    return p.parse_args()


def main() -> None:
    load_dotenv(SCRIPT_DIR / ".env")
    args = parse_args()

    end_shard = args.end_shard if args.end_shard is not None else args.shard_count
    shard_range = list(range(args.start_shard, end_shard))
    target_labels: set[str] | None = (
        set(x.strip() for x in args.labels.split(",") if x.strip()) or None
    )
    token = os.environ.get("HF_TOKEN") or None

    print(f"Dataset : {args.dataset}", flush=True)
    print(f"Shards  : {args.start_shard}–{end_shard - 1}  ({len(shard_range)} total)", flush=True)
    print(f"Workers : {args.workers}", flush=True)
    print(f"Labels  : {sorted(target_labels) if target_labels else 'all'}", flush=True)
    print(flush=True)

    # Split shards into cached vs pending
    cached: dict[int, ShardResult] = {}
    pending: list[int] = []
    for idx in shard_range:
        if not args.no_checkpoint:
            result = load_checkpoint(args.checkpoint_dir, idx)
            if result is not None:
                n_proc = sum(a.n for a in result.per_label.values())
                print(f"  shard {idx:3d}: checkpoint — {result.rows_scanned} rows, {n_proc} processed")
                cached[idx] = result
                continue
        pending.append(idx)

    print(f"\nPending: {len(pending)}, cached: {len(cached)}", flush=True)

    worker_inputs = [
        {
            "shard_idx": idx,
            "shard_count": args.shard_count,
            "dataset": args.dataset,
            "split": args.split,
            "token": token,
            "audio_col": args.audio_column,
            "label_col": args.label_column,
            "target_sr": args.target_sr,
            "nperseg": args.nperseg,
            "max_duration_s": args.max_duration_s,
            "min_duration_s": args.min_duration_s,
            "reservoir_per_shard": args.reservoir_per_shard,
            "target_labels": target_labels,
        }
        for idx in pending
    ]

    all_results: dict[int, ShardResult] = dict(cached)

    if worker_inputs:
        import multiprocessing
        import sys

        print(f"Spawning {args.workers} worker processes (spawn context, first start may take ~30 s)…", flush=True)
        ctx = multiprocessing.get_context("spawn")
        with ProcessPoolExecutor(max_workers=args.workers, mp_context=ctx) as pool:
            print(f"Submitting {len(worker_inputs)} shards…", flush=True)
            futures = {}
            for kw in worker_inputs:
                fut = pool.submit(_process_shard, kw)
                futures[fut] = kw["shard_idx"]
                print(f"  submitted shard {kw['shard_idx']:3d}", flush=True)

            print(f"\nWaiting for results…", flush=True)
            done = 0
            for future in as_completed(futures):
                idx = futures[future]
                done += 1
                try:
                    result = future.result()
                except Exception as exc:
                    print(f"  shard {idx:3d}: EXCEPTION — {exc}", flush=True)
                    continue

                n_proc = sum(a.n for a in result.per_label.values())
                status = f"ERROR({result.error})" if result.error else "ok"
                print(
                    f"  shard {idx:3d}: {status} — "
                    f"{result.rows_scanned} rows, {n_proc} processed, "
                    f"{result.wall_s:.1f}s  [{done}/{len(pending)}]",
                    flush=True,
                )

                if not args.no_checkpoint:
                    save_checkpoint(args.checkpoint_dir, result)
                all_results[idx] = result

    # Merge all shards
    print("\nMerging…")
    global_accs: dict[str, LabelAcc] = {}
    total_rows = 0

    for idx in shard_range:
        result = all_results.get(idx)
        if result is None:
            print(f"  shard {idx:3d}: missing, skipped")
            continue
        total_rows += result.rows_scanned
        for label, acc in result.per_label.items():
            if label not in global_accs:
                global_accs[label] = LabelAcc()
            global_accs[label].merge(acc)

    print(f"\nTotal rows scanned : {total_rows:,}")
    for label, acc in sorted(global_accs.items()):
        name = {"0": "no-drone", "1": "drone"}.get(label, label)
        print(
            f"  {name:10s} (label={label}): "
            f"{acc.n:7,} files, "
            f"{acc.total_duration_s / 3600:5.1f} h, "
            f"{acc.skipped} skipped, "
            f"{len(acc.reservoir)} reservoir PSDs"
        )

    if not any(acc.n > 0 for acc in global_accs.values()):
        raise SystemExit("No audio was processed — check --labels and dataset structure.")

    metadata: dict[str, Any] = {
        "dataset": args.dataset,
        "split": args.split,
        "shard_range": [args.start_shard, end_shard],
        "workers": args.workers,
        "total_rows_scanned": total_rows,
        "labels": {
            label: {
                "n": acc.n,
                "skipped": acc.skipped,
                "total_duration_h": acc.total_duration_s / 3600,
                "reservoir_size": len(acc.reservoir),
            }
            for label, acc in global_accs.items()
        },
        "target_sr": args.target_sr,
        "nperseg": args.nperseg,
        "max_duration_s_per_file": args.max_duration_s,
    }

    plot_results(global_accs, args, metadata)


if __name__ == "__main__":
    main()
