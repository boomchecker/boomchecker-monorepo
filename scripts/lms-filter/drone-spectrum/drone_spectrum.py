#!/usr/bin/env python3
"""Plot the average drone-only spectrum from a Hugging Face audio dataset."""

from __future__ import annotations

import argparse
import io
import json
import math
import os
from pathlib import Path
from typing import Any, Iterable

import numpy as np
from scipy.signal import resample_poly, welch


DEFAULT_DATASET = "geronimobasso/drone-audio-detection-samples"
DEFAULT_OUTPUT_DIR = Path(__file__).resolve().parent / "out"
DEFAULT_TARGET_SR = 16_000


def load_dotenv(path: Path) -> None:
    if not path.exists():
        return
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip().strip("\"'")
        if key and key not in os.environ:
            os.environ[key] = value


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compute and plot average PSD for drone-labelled audio samples."
    )
    parser.add_argument("--dataset", default=DEFAULT_DATASET)
    parser.add_argument("--split", default="train")
    parser.add_argument(
        "--data-files",
        default="",
        help="Comma-separated dataset repo data files, e.g. data/train-00002-of-00039.parquet.",
    )
    parser.add_argument(
        "--start-shard",
        type=int,
        help="Start from this DADS parquet shard index instead of scanning from shard 0.",
    )
    parser.add_argument(
        "--end-shard",
        type=int,
        help="Exclusive DADS parquet shard end index. Defaults to --shard-count.",
    )
    parser.add_argument("--shard-count", type=int, default=39)
    parser.add_argument("--streaming", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument(
        "--allow-non-streaming-download",
        action="store_true",
        help="Allow --no-streaming. This may download multi-GB dataset shards.",
    )
    parser.add_argument("--audio-column", default="auto")
    parser.add_argument("--label-column", default="auto")
    parser.add_argument("--drone-label", default="1")
    parser.add_argument(
        "--inspect-labels",
        action="store_true",
        help="Only count label values in the selected rows/shards, without reading audio.",
    )
    parser.add_argument(
        "--find-drone-shard",
        action="store_true",
        help="Inspect DADS shards in order and stop at the first shard containing --drone-label.",
    )
    parser.add_argument("--max-samples", type=int, default=500)
    parser.add_argument(
        "--max-rows-scanned",
        type=int,
        default=0,
        help="Stop after scanning this many dataset rows; 0 means unlimited.",
    )
    parser.add_argument(
        "--skip-rows",
        type=int,
        default=0,
        help="Skip this many rows before looking for drone-labelled samples.",
    )
    parser.add_argument("--max-duration-s", type=float, default=5.0)
    parser.add_argument("--min-duration-s", type=float, default=0.25)
    parser.add_argument("--target-sr", type=int, default=DEFAULT_TARGET_SR)
    parser.add_argument("--nperseg", type=int, default=2048)
    parser.add_argument("--anc-max-hz", type=float, default=1000.0)
    parser.add_argument("--percentile-low", type=float, default=25.0)
    parser.add_argument("--percentile-high", type=float, default=75.0)
    parser.add_argument("--progress-every", type=int, default=1000)
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT_DIR)
    parser.add_argument("--output-name", default="drone_average_spectrum")
    return parser.parse_args()


def selected_data_files(args: argparse.Namespace) -> list[str] | None:
    if args.data_files.strip():
        return [item.strip() for item in args.data_files.split(",") if item.strip()]
    start_shard = args.start_shard
    if start_shard is None:
        return None
    end_shard = args.end_shard if args.end_shard is not None else args.shard_count
    if start_shard < 0 or end_shard <= start_shard:
        raise SystemExit("--start-shard/--end-shard must define a non-empty shard range.")
    if end_shard > args.shard_count:
        raise SystemExit("--end-shard cannot be larger than --shard-count.")
    return [
        f"data/train-{idx:05d}-of-{args.shard_count:05d}.parquet"
        for idx in range(start_shard, end_shard)
    ]


def dads_shard_file(idx: int, shard_count: int) -> str:
    return f"data/train-{idx:05d}-of-{shard_count:05d}.parquet"


def inspect_parquet_labels(args: argparse.Namespace, data_files: list[str]) -> dict[str, Any]:
    try:
        import fsspec
        import pyarrow.parquet as pq
        from huggingface_hub import hf_hub_url
    except ModuleNotFoundError as exc:
        raise SystemExit(
            "Fast shard inspection needs fsspec, pyarrow, and huggingface_hub. Run `task setup` first."
        ) from exc

    label_column = "label" if args.label_column == "auto" else args.label_column
    wanted_label = normalize_label(args.drone_label)
    counts: dict[str, int] = {}
    per_file: list[dict[str, Any]] = []
    rows_scanned = 0
    storage_options: dict[str, Any] = {}
    token = os.environ.get("HF_TOKEN")
    if token:
        storage_options["headers"] = {"authorization": f"Bearer {token}"}

    for data_file in data_files:
        file_counts: dict[str, int] = {}
        url = hf_hub_url(args.dataset, data_file, repo_type="dataset")
        with fsspec.open(url, "rb", **storage_options) as handle:
            table = pq.read_table(handle, columns=[label_column])
        for value in table.column(label_column).to_pylist():
            label = normalize_label(value)
            counts[label] = counts.get(label, 0) + 1
            file_counts[label] = file_counts.get(label, 0) + 1
        rows_scanned += table.num_rows
        file_result = {
            "data_file": data_file,
            "rows_scanned": table.num_rows,
            "counts": file_counts,
            "contains_drone_label": file_counts.get(wanted_label, 0) > 0,
        }
        per_file.append(file_result)
        print(json.dumps(file_result, indent=2))
        if args.progress_every > 0:
            print(f"Scanned {rows_scanned} label rows, counts so far: {counts}")
        if args.max_rows_scanned > 0 and rows_scanned >= args.max_rows_scanned:
            break

    return {
        "dataset": args.dataset,
        "split": args.split,
        "data_files": data_files,
        "label_column": label_column,
        "rows_scanned": rows_scanned,
        "counts": counts,
        "per_file": per_file,
    }


def normalize_label(value: Any) -> str:
    if isinstance(value, bytes):
        value = value.decode("utf-8", errors="replace")
    if isinstance(value, (np.integer, int)):
        return str(int(value))
    if isinstance(value, (np.floating, float)):
        number = float(value)
        if math.isfinite(number) and number.is_integer():
            return str(int(number))
        return str(number)
    return str(value)


def infer_column(columns: Iterable[str], candidates: list[str], kind: str) -> str:
    names = list(columns)
    for candidate in candidates:
        if candidate in names:
            return candidate
    raise ValueError(
        f"Could not infer {kind} column. Available columns: {', '.join(names)}. "
        f"Pass --{kind}-column explicitly."
    )


def dataset_columns(ds: Any) -> list[str]:
    if getattr(ds, "column_names", None):
        return list(ds.column_names)
    first = next(iter(ds))
    return list(first.keys())


def cast_audio_if_possible(ds: Any, audio_column: str, target_sr: int) -> Any:
    del target_sr

    from datasets import Audio, IterableDataset

    if isinstance(ds, IterableDataset):
        try:
            return ds.cast_column(audio_column, Audio(decode=False))
        except (TypeError, ValueError):
            return ds
    return ds.cast_column(audio_column, Audio(decode=False))


def iter_dataset(args: argparse.Namespace) -> tuple[Any, str, str, list[str] | None]:
    try:
        from datasets import load_dataset
    except ModuleNotFoundError as exc:
        raise SystemExit(
            "Missing dependency 'datasets'. Run `task setup` from scripts/lms-filter "
            "to install Hugging Face dataset support."
        ) from exc

    if not args.streaming and not args.allow_non_streaming_download:
        raise SystemExit(
            "--no-streaming can download multi-GB parquet shards for this dataset, "
            "even for narrow split slices. Use the default streaming mode, or pass "
            "--allow-non-streaming-download if you intentionally want a local full/shard download."
        )

    token = os.environ.get("HF_TOKEN") or None
    if args.skip_rows > 0:
        print(
            "Warning: --skip-rows still reads earlier parquet shards in streaming mode. "
            "Prefer --start-shard/--end-shard for DADS."
        )
    data_files = selected_data_files(args)
    ds = load_dataset(
        args.dataset,
        data_files=data_files,
        split=args.split,
        streaming=args.streaming,
        token=token,
    )
    columns = dataset_columns(ds)
    audio_column = (
        infer_column(columns, ["audio", "file", "wav", "sound"], "audio")
        if args.audio_column == "auto"
        else args.audio_column
    )
    label_column = (
        infer_column(columns, ["label", "labels", "class", "target", "category"], "label")
        if args.label_column == "auto"
        else args.label_column
    )
    return ds, audio_column, label_column, data_files


def inspect_labels(args: argparse.Namespace) -> None:
    data_files = selected_data_files(args)
    if data_files is not None and args.skip_rows == 0:
        print(json.dumps(inspect_parquet_labels(args, data_files), indent=2))
        return

    ds, audio_column, label_column, data_files = iter_dataset(args)
    ds = cast_audio_if_possible(ds, audio_column, args.target_sr)
    counts: dict[str, int] = {}
    rows_scanned = 0
    for example in ds:
        rows_scanned += 1
        if args.skip_rows > 0 and rows_scanned <= args.skip_rows:
            continue
        if args.max_rows_scanned > 0 and rows_scanned > args.max_rows_scanned:
            break
        label = normalize_label(example[label_column])
        counts[label] = counts.get(label, 0) + 1
        if args.progress_every > 0 and rows_scanned % args.progress_every == 0:
            print(f"Scanned {rows_scanned} rows, label counts so far: {counts}")
    print(
        json.dumps(
            {
                "dataset": args.dataset,
                "split": args.split,
                "data_files": data_files,
                "label_column": label_column,
                "rows_scanned": rows_scanned,
                "counts": counts,
            },
            indent=2,
        )
    )


def find_drone_shard(args: argparse.Namespace) -> None:
    wanted_label = normalize_label(args.drone_label)
    start = args.start_shard if args.start_shard is not None else 0
    end = args.end_shard if args.end_shard is not None else args.shard_count
    if start < 0 or end <= start or end > args.shard_count:
        raise SystemExit("--start-shard/--end-shard must define a valid DADS shard range.")

    inspected: list[dict[str, Any]] = []
    found: dict[str, Any] | None = None
    for idx in range(start, end):
        data_file = dads_shard_file(idx, args.shard_count)
        result = inspect_parquet_labels(args, [data_file])
        shard_result = result["per_file"][0]
        counts = shard_result["counts"]
        inspected.append(shard_result)
        if counts.get(wanted_label, 0) > 0:
            found = {
                "shard": idx,
                "start_shard": idx,
                "end_shard": idx + 1,
                "data_file": data_file,
                "drone_label": wanted_label,
                "drone_rows": counts[wanted_label],
            }
            break

    print(
        json.dumps(
            {
                "dataset": args.dataset,
                "searched_start_shard": start,
                "searched_end_shard": end,
                "found": found,
                "inspected": inspected,
            },
            indent=2,
        )
    )


def audio_to_array(audio_value: Any) -> tuple[np.ndarray, int]:
    import soundfile as sf

    if isinstance(audio_value, dict):
        if audio_value.get("array") is not None and audio_value.get("sampling_rate") is not None:
            array = audio_value["array"]
            sampling_rate = audio_value["sampling_rate"]
        elif audio_value.get("bytes") is not None:
            array, sampling_rate = sf.read(io.BytesIO(audio_value["bytes"]), dtype="float32")
        elif audio_value.get("path") is not None:
            array, sampling_rate = sf.read(audio_value["path"], dtype="float32")
        else:
            raise ValueError("Audio dictionary has no array, bytes, or path payload.")
    elif isinstance(audio_value, (str, Path)):
        array, sampling_rate = sf.read(audio_value, dtype="float32")
    elif isinstance(audio_value, (bytes, bytearray)):
        array, sampling_rate = sf.read(io.BytesIO(audio_value), dtype="float32")
    else:
        array = getattr(audio_value, "array", None)
        sampling_rate = getattr(audio_value, "sampling_rate", None)
        if array is None or sampling_rate is None:
            raise ValueError("Audio value does not contain array/sampling_rate, path, or bytes.")
    data = np.asarray(array, dtype=np.float32)
    if data.ndim > 1:
        data = np.mean(data, axis=-1, dtype=np.float32)
    return data, int(sampling_rate)


def resample_if_needed(audio: np.ndarray, sample_rate: int, target_sr: int) -> np.ndarray:
    if sample_rate == target_sr:
        return audio
    gcd = math.gcd(sample_rate, target_sr)
    up = target_sr // gcd
    down = sample_rate // gcd
    return resample_poly(audio, up, down).astype(np.float32)


def trimmed_audio(audio: np.ndarray, sample_rate: int, max_duration_s: float) -> np.ndarray:
    if max_duration_s <= 0:
        return audio
    max_samples = int(round(max_duration_s * sample_rate))
    if max_samples <= 0 or audio.size <= max_samples:
        return audio
    start = (audio.size - max_samples) // 2
    return audio[start : start + max_samples]


def compute_psd(audio: np.ndarray, sample_rate: int, nperseg: int) -> tuple[np.ndarray, np.ndarray]:
    if audio.size < 2:
        raise ValueError("Audio sample is too short for PSD.")
    audio = audio.astype(np.float32, copy=False)
    audio = audio - float(np.mean(audio))
    segment = min(nperseg, audio.size)
    freqs, psd = welch(
        audio,
        fs=sample_rate,
        window="hann",
        nperseg=segment,
        noverlap=segment // 2,
        detrend=False,
        scaling="density",
    )
    return freqs.astype(np.float32), np.maximum(psd.astype(np.float64), 1e-20)


def iter_drone_psds(args: argparse.Namespace) -> tuple[np.ndarray, list[np.ndarray], dict[str, Any]]:
    ds, audio_column, label_column, data_files = iter_dataset(args)
    ds = cast_audio_if_possible(ds, audio_column, args.target_sr)

    wanted_label = normalize_label(args.drone_label)
    psds: list[np.ndarray] = []
    freqs_ref: np.ndarray | None = None
    seen = 0
    skipped = 0
    rows_scanned = 0
    used_duration_s = 0.0

    for example in ds:
        rows_scanned += 1
        if args.skip_rows > 0 and rows_scanned <= args.skip_rows:
            continue
        if args.max_rows_scanned > 0 and rows_scanned > args.max_rows_scanned:
            break
        if args.progress_every > 0 and rows_scanned % args.progress_every == 0:
            print(
                f"Scanned {rows_scanned} rows, matched {seen} drone rows, "
                f"processed {len(psds)} samples..."
            )
        if normalize_label(example[label_column]) != wanted_label:
            continue
        seen += 1
        try:
            audio, sample_rate = audio_to_array(example[audio_column])
            audio = resample_if_needed(audio, sample_rate, args.target_sr)
            audio = trimmed_audio(audio, args.target_sr, args.max_duration_s)
            duration_s = audio.size / float(args.target_sr)
            if duration_s < args.min_duration_s:
                skipped += 1
                continue
            freqs, psd = compute_psd(audio, args.target_sr, args.nperseg)
        except Exception as exc:  # noqa: BLE001 - keep batch processing alive.
            skipped += 1
            print(f"Skipping sample {seen}: {exc}")
            continue
        if freqs_ref is None:
            freqs_ref = freqs
        elif freqs.shape != freqs_ref.shape or not np.allclose(freqs, freqs_ref):
            skipped += 1
            print(f"Skipping sample {seen}: PSD frequency grid mismatch")
            continue
        psds.append(psd)
        used_duration_s += duration_s
        if len(psds) >= args.max_samples:
            break

    if freqs_ref is None or not psds:
        raise SystemExit(
            "No drone-labelled audio samples were processed. "
            f"Scanned {rows_scanned} rows and matched {seen} drone rows. "
            "Check --label-column/--drone-label, or inspect shard labels with "
            "`--inspect-labels --start-shard N --end-shard N+1`."
        )

    metadata = {
        "dataset": args.dataset,
        "split": args.split,
        "streaming": args.streaming,
        "data_files": data_files,
        "audio_column": audio_column,
        "label_column": label_column,
        "drone_label": wanted_label,
        "matched_drone_rows_seen": seen,
        "rows_scanned": rows_scanned,
        "skip_rows": args.skip_rows,
        "processed_samples": len(psds),
        "skipped_samples": skipped,
        "used_duration_s": used_duration_s,
        "target_sr": args.target_sr,
        "nperseg": args.nperseg,
        "max_duration_s_per_file": args.max_duration_s,
    }
    return freqs_ref, psds, metadata


def bandpower(freqs: np.ndarray, psd: np.ndarray, low_hz: float, high_hz: float) -> float:
    mask = (freqs >= low_hz) & (freqs <= high_hz)
    if np.count_nonzero(mask) < 2:
        return 0.0
    return float(np.trapz(psd[mask], freqs[mask]))


def plot_spectrum(
    freqs: np.ndarray,
    psds: list[np.ndarray],
    args: argparse.Namespace,
    metadata: dict[str, Any],
) -> dict[str, Any]:
    args.output_dir.mkdir(parents=True, exist_ok=True)
    os.environ.setdefault("MPLCONFIGDIR", str(args.output_dir / ".mplconfig"))
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    stack = np.vstack(psds)
    mean_psd = np.mean(stack, axis=0)
    db_stack = 10.0 * np.log10(stack)
    mean_db = 10.0 * np.log10(mean_psd)
    low_db = np.percentile(db_stack, args.percentile_low, axis=0)
    high_db = np.percentile(db_stack, args.percentile_high, axis=0)

    total_power = bandpower(freqs, mean_psd, 0.0, args.target_sr / 2.0)
    anc_power = bandpower(freqs, mean_psd, 0.0, args.anc_max_hz)
    metadata["anc_band_hz"] = [0.0, args.anc_max_hz]
    metadata["mean_total_power"] = total_power
    metadata["mean_anc_band_power"] = anc_power
    metadata["mean_anc_band_power_ratio"] = anc_power / total_power if total_power > 0 else 0.0

    fig_path = args.output_dir / f"{args.output_name}.png"

    fig, ax = plt.subplots(figsize=(11, 6))
    ax.axvspan(0, args.anc_max_hz, color="#dbeafe", alpha=0.55, label="low-frequency ANC region")
    ax.fill_between(
        freqs,
        low_db,
        high_db,
        color="#9ca3af",
        alpha=0.35,
        linewidth=0,
        label=f"{args.percentile_low:.0f}-{args.percentile_high:.0f} percentile",
    )
    ax.plot(freqs, mean_db, color="#111827", linewidth=1.6, label="mean drone PSD")
    ax.set_xlim(0, args.target_sr / 2)
    ax.set_xlabel("Frequency [Hz]")
    ax.set_ylabel("PSD [dB/Hz]")
    ax.set_title("Average drone spectrum from DADS drone-labelled samples")
    ax.grid(True, alpha=0.25)
    ax.legend(loc="best")
    ax.text(
        0.99,
        0.02,
        f"n={metadata['processed_samples']} files, "
        f"ANC-band power ratio={metadata['mean_anc_band_power_ratio']:.2%}",
        transform=ax.transAxes,
        ha="right",
        va="bottom",
        fontsize=9,
        bbox={"boxstyle": "round,pad=0.25", "facecolor": "white", "alpha": 0.8, "edgecolor": "#d1d5db"},
    )
    fig.tight_layout()
    fig.savefig(fig_path, dpi=180)
    plt.close(fig)

    metadata["figure_path"] = str(fig_path)
    metrics_path = args.output_dir / f"{args.output_name}_metrics.json"
    metrics_path.write_text(json.dumps(metadata, indent=2), encoding="utf-8")
    metadata["metrics_path"] = str(metrics_path)
    return metadata


def main() -> None:
    load_dotenv(Path(__file__).resolve().parent / ".env")
    args = parse_args()
    if args.find_drone_shard:
        find_drone_shard(args)
        return
    if args.inspect_labels:
        inspect_labels(args)
        return
    freqs, psds, metadata = iter_drone_psds(args)
    result = plot_spectrum(freqs, psds, args, metadata)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
