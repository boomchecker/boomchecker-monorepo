from __future__ import annotations

import argparse
import csv
from pathlib import Path

import numpy as np
import pandas as pd
from scipy.fftpack import dct

import utils


PROJECT_ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = PROJECT_ROOT / "datasets" / "recordings" / "manifest.csv"
AUDIO_ROOT = PROJECT_ROOT / "datasets" / "recordings"
OUTPUT_ROOT = PROJECT_ROOT / "generated" / "features"
EXPECTED_FRAMES = 58
DEFAULT_SNR_DB_LEVELS = [30, 20, 10, 5]


def fix_shape(mfcc_matrix: np.ndarray) -> np.ndarray:
    if mfcc_matrix.shape[0] < EXPECTED_FRAMES:
        pad_width = EXPECTED_FRAMES - mfcc_matrix.shape[0]
        return np.pad(mfcc_matrix, pad_width=((0, pad_width), (0, 0)), mode="constant")
    if mfcc_matrix.shape[0] > EXPECTED_FRAMES:
        return mfcc_matrix[:EXPECTED_FRAMES, :]
    return mfcc_matrix


def add_snr_noise(signal: np.ndarray, snr_db: float) -> np.ndarray:
    """Add waveform-domain Gaussian noise at the given acoustic SNR (dB), applied before MFCC extraction."""
    signal_power = np.mean(signal**2)
    noise_power = signal_power / (10 ** (snr_db / 10))
    noise = np.random.normal(0, np.sqrt(noise_power), signal.shape)
    return signal + noise


def compute_mfcc(signal: np.ndarray, sampling_rate: int) -> np.ndarray:
    peak_index = utils.find_peak(signal)
    windowed_signal = utils.extract_window(signal, sampling_rate, peak_index)
    frames = utils.framing(windowed_signal, sampling_rate)
    _, pow_frames = utils.spectrum(frames, NFFT=512)
    filter_banks = utils.mel_filterbank(pow_frames, sampling_rate, nfilt=40, NFFT=512)
    mfcc = dct(filter_banks, type=2, axis=1, norm="ortho")[:, :12]
    return fix_shape(mfcc)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=MANIFEST_PATH)
    parser.add_argument("--output", type=Path, default=OUTPUT_ROOT)
    parser.add_argument("--include-noisy", action="store_true")
    parser.add_argument(
        "--snr-db",
        type=float,
        nargs="+",
        default=DEFAULT_SNR_DB_LEVELS,
        help="Acoustic SNR levels (dB) for waveform-domain noise variants, used with --include-noisy.",
    )
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--seed", type=int, default=42, help="Random seed for noise generation (reproducibility).")
    args = parser.parse_args()

    np.random.seed(args.seed)
    manifest = pd.read_csv(args.manifest)
    if args.limit:
        manifest = manifest.head(args.limit)
    feature_rows: list[dict[str, str | int]] = []
    feature_root = args.output
    feature_root.mkdir(parents=True, exist_ok=True)

    for row in manifest.itertuples(index=False):
        audio_path = AUDIO_ROOT / row.audio_path
        try:
            signal, sampling_rate = utils.load_signal(str(audio_path))
        except Exception as exc:
            print(f"skip {row.recording_id}: cannot load {audio_path}: {exc}")
            continue

        variants = [("clean", signal)]
        if args.include_noisy:
            for snr_db in args.snr_db:
                variant_name = f"noise_snr{int(round(snr_db))}db"
                variants.append((variant_name, add_snr_noise(signal, snr_db)))

        for variant, variant_signal in variants:
            try:
                mfcc = compute_mfcc(variant_signal, sampling_rate)
            except Exception as exc:
                print(f"skip {row.recording_id}: cannot compute MFCC: {exc}")
                continue
            rel_path = Path(variant) / row.split / str(row.class_id) / f"{row.recording_id}_{variant}_mfcc.npy"
            output_path = feature_root / rel_path
            output_path.parent.mkdir(parents=True, exist_ok=True)
            np.save(output_path, mfcc)
            feature_rows.append(
                {
                    "recording_id": row.recording_id,
                    "feature_path": rel_path.as_posix(),
                    "variant": variant,
                    "split": row.split,
                    "label": row.label,
                    "class_id": int(row.class_id),
                }
            )

    index_path = feature_root / "features_manifest.csv"
    if not feature_rows:
        raise SystemExit("No feature matrices were generated.")
    with index_path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(feature_rows[0].keys()))
        writer.writeheader()
        writer.writerows(feature_rows)

    print(f"Wrote {len(feature_rows)} feature matrices to {feature_root}")
    print(f"Wrote {index_path}")


if __name__ == "__main__":
    main()
