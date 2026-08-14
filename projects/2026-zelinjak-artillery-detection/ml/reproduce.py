from __future__ import annotations

import argparse
import csv
import subprocess
import sys
from pathlib import Path

import pandas as pd

from evaluate_robustness import DEFAULT_SNR_DB_LEVELS, run_regime


PROJECT_ROOT = Path(__file__).resolve().parents[1]
RESULTS_ROOT = PROJECT_ROOT / "generated" / "results"
DEFAULT_SEEDS = [42, 43, 44, 45, 46]

MODEL_KERAS = PROJECT_ROOT / "archive" / "models" / "najlepsi_model.h5"
MODEL_TFLITE = PROJECT_ROOT / "archive" / "models" / "model.tflite"

MODE_LABELS = {"keras": "PC float32", "tflite": "PC int8"}
SCOPE_TO_SPLIT = {"full": "all", "test": "test"}


def variants_with_labels() -> list[tuple[str, str]]:
    """(label, variant) pairs matching evaluate_robustness.py's own --include-clean ordering,
    so aggregated output lines up with the paper's clean + 30/20/10/5 dB sweep."""
    pairs = [("clean", "clean")]
    for snr_db in DEFAULT_SNR_DB_LEVELS:
        pairs.append((f"{snr_db:g} dB", f"noise_snr{int(round(snr_db))}db"))
    return pairs


def prepare_features_for_seed(seed: int, force: bool) -> Path:
    feature_dir = PROJECT_ROOT / "generated" / f"features_seed{seed}"
    feature_index = feature_dir / "features_manifest.csv"
    if feature_index.exists() and not force:
        print(f"seed {seed}: features already exist at {feature_dir}, skipping (--force to regenerate)")
        return feature_index
    cmd = [
        sys.executable,
        str(PROJECT_ROOT / "ml" / "prepare_features.py"),
        "--include-noisy",
        "--snr-db",
        *[str(s) for s in DEFAULT_SNR_DB_LEVELS],
        "--seed",
        str(seed),
        "--output",
        str(feature_dir),
    ]
    print(f"seed {seed}: generating features...")
    subprocess.run(cmd, check=True, cwd=PROJECT_ROOT)
    return feature_index


def evaluate_seed(seed: int, feature_index: Path) -> tuple[list[dict], list[dict], list[dict]]:
    """Run both canonical models over both scopes and all variants for one seed.

    Returns (legacy_full_rows, aggregate_rows, per_sample_rows) where legacy_full_rows matches
    the pre-M3 metrics_seedN.csv schema (mode, snr_db, accuracy, ..., samples) for a direct
    byte-level cross-check against generated/results_ref_20260721/, and aggregate_rows matches
    the mode/variant/seed/scope/n/... schema already used by the 2026-07-21 per_seed_metrics.csv.
    """
    legacy_full_rows: list[dict] = []
    aggregate_rows: list[dict] = []
    per_sample_rows: list[dict] = []

    for scope, split in SCOPE_TO_SPLIT.items():
        for label, variant in variants_with_labels():
            for model_format, model_path in (("keras", MODEL_KERAS), ("tflite", MODEL_TFLITE)):
                metrics, per_sample = run_regime(model_path, model_format, variant, feature_index, split)
                mode = MODE_LABELS[model_format]

                if scope == "full":
                    legacy_full_rows.append({"mode": mode, "snr_db": label, **metrics})

                aggregate_rows.append(
                    {
                        "mode": mode,
                        "variant": variant,
                        "seed": seed,
                        "scope": scope,
                        "n": metrics["samples"],
                        "accuracy": metrics["accuracy"],
                        "precision": metrics["precision"],
                        "recall": metrics["recall"],
                        "f1": metrics["f1"],
                        "mcc": metrics["mcc"],
                    }
                )
                for recording_id, sample_split, class_id, score, pred in per_sample:
                    per_sample_rows.append(
                        {
                            "mode": mode,
                            "variant": variant,
                            "seed": seed,
                            "scope": scope,
                            "recording_id": recording_id,
                            "split": sample_split,
                            "class_id": class_id,
                            "score": score,
                            "pred": pred,
                        }
                    )

    return legacy_full_rows, aggregate_rows, per_sample_rows


def write_csv(rows: list[dict], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def summarize(aggregate_rows: list[dict]) -> pd.DataFrame:
    df = pd.DataFrame(aggregate_rows)
    grouped = df.groupby(["mode", "variant", "scope"], sort=False)[["accuracy", "precision", "recall", "f1", "mcc"]]
    summary = grouped.agg(["mean", "std"])
    summary.columns = ["_".join(col) for col in summary.columns]
    summary["n_seeds"] = df.groupby(["mode", "variant", "scope"], sort=False).size().values
    return summary.reset_index()


def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "M3: multi-seed robustness reproduction. Runs the canonical float32 (najlepsi_model.h5) "
            "and int8 (archive/model.tflite) models over clean + 30/20/10/5 dB noise, full-corpus and "
            "held-out test split, across --seeds, and aggregates to mean +/- std."
        )
    )
    parser.add_argument("--seeds", type=int, nargs="+", default=DEFAULT_SEEDS)
    parser.add_argument(
        "--force-features", action="store_true", help="Regenerate features even if the seed directory already exists."
    )
    args = parser.parse_args()

    all_aggregate_rows: list[dict] = []
    all_per_sample_rows: list[dict] = []

    for seed in args.seeds:
        feature_index = prepare_features_for_seed(seed, args.force_features)
        print(f"seed {seed}: evaluating (full-corpus + held-out, float32 + int8)...")
        legacy_full_rows, aggregate_rows, per_sample_rows = evaluate_seed(seed, feature_index)

        write_csv(legacy_full_rows, RESULTS_ROOT / f"metrics_seed{seed}.csv")
        write_csv(per_sample_rows, RESULTS_ROOT / f"per_sample_seed{seed}.csv")

        all_aggregate_rows.extend(aggregate_rows)
        all_per_sample_rows.extend(per_sample_rows)
        print(f"seed {seed}: done ({len(aggregate_rows)} metric rows, {len(per_sample_rows)} per-sample rows)")

    write_csv(all_aggregate_rows, RESULTS_ROOT / "per_seed_metrics.csv")
    summary = summarize(all_aggregate_rows)
    summary_path = RESULTS_ROOT / "summary_mean_std.csv"
    summary.to_csv(summary_path, index=False)

    print(f"\nWrote {RESULTS_ROOT / 'per_seed_metrics.csv'} ({len(all_aggregate_rows)} rows)")
    print(f"Wrote {summary_path}")
    print(summary.to_string(index=False))


if __name__ == "__main__":
    main()
