from __future__ import annotations

import argparse
import json
from pathlib import Path

import pandas as pd


PROJECT_ROOT = Path(__file__).resolve().parents[1]
RESULTS_ROOT = PROJECT_ROOT / "generated" / "results"
REPORTS_ROOT = PROJECT_ROOT / "generated" / "reports"
QUANT_MULTISEED_ROOT = REPORTS_ROOT / "quantization_effect_multiseed"

VARIANT_ORDER = ["clean", "noise_snr30db", "noise_snr20db", "noise_snr10db", "noise_snr5db"]
VARIANT_SNR_LABEL = {
    "clean": "Clean",
    "noise_snr30db": "30",
    "noise_snr20db": "20",
    "noise_snr10db": "10",
    "noise_snr5db": "5",
}
SCOPE_ORDER = ["full", "test"]
SCOPE_LABEL = {"full": "Full corpus", "test": "Held-out test"}

# Facts established elsewhere in the roadmap (M1), not derivable from a CSV.
MODEL_SIZE_BYTES = 81008
PARAM_COUNT = 72193
TENSOR_ARENA_KIB = 80

PUBLISHED_TABLE2_FLOAT32 = {
    "noise_snr30db": {"acc": 84.41, "prec": 0.35, "rec": 1.00, "f1": 0.52, "mcc": 0.54},
    "noise_snr20db": {"acc": 82.86, "prec": 0.33, "rec": 1.00, "f1": 0.49, "mcc": 0.54},
    "noise_snr10db": {"acc": 81.12, "prec": 0.31, "rec": 0.99, "f1": 0.47, "mcc": 0.49},
    "noise_snr5db": {"acc": 80.44, "prec": 0.29, "rec": 0.93, "f1": 0.45, "mcc": 0.46},
}
PUBLISHED_TABLE3_PC_INT8 = {
    "noise_snr30db": {"acc": 98.66, "prec": 0.86, "rec": 1.00, "f1": 0.93, "mcc": 0.92},
    "noise_snr20db": {"acc": 96.77, "prec": 0.72, "rec": 1.00, "f1": 0.84, "mcc": 0.83},
    "noise_snr10db": {"acc": 92.74, "prec": 0.54, "rec": 0.97, "f1": 0.69, "mcc": 0.69},
    "noise_snr5db": {"acc": 92.41, "prec": 0.53, "rec": 0.90, "f1": 0.66, "mcc": 0.65},
}
PUBLISHED_TABLE3_ESP32_INT8 = {
    "noise_snr30db": {"acc": 99.86, "prec": 1.00, "rec": 0.98, "f1": 0.99, "mcc": 0.99},
    "noise_snr20db": {"acc": 99.73, "prec": 0.98, "rec": 0.98, "f1": 0.98, "mcc": 0.98},
    "noise_snr10db": {"acc": 97.78, "prec": 0.86, "rec": 0.87, "f1": 0.87, "mcc": 0.86},
    "noise_snr5db": {"acc": 97.18, "prec": 0.95, "rec": 0.70, "f1": 0.81, "mcc": 0.80},
}


def fmt_pm(mean: float, std: float, decimals: int = 3, pct: bool = False) -> str:
    if pct:
        mean, std = mean * 100, std * 100
    return f"{mean:.{decimals if not pct else 2}f} $\\pm$ {std:.{decimals if not pct else 2}f}"


def latex_table(df: pd.DataFrame, mode: str, label: str, caption: str) -> str:
    sub = df[df["mode"] == mode]
    lines = [
        "\\begin{table}[!tbh]",
        f"\\caption{{{caption}}}",
        f"\\label{{{label}}}",
        "\\centering",
        "\\resizebox{\\linewidth}{!}{%",
        "\\begin{tabular}{@{}lcccccc@{}}",
        "\\toprule",
        "Scope & SNR (dB) & Accuracy (\\%) & Precision & Recall & F1-score & MCC \\\\",
        "\\midrule",
    ]
    for scope in SCOPE_ORDER:
        first = True
        for variant in VARIANT_ORDER:
            row = sub[(sub["scope"] == scope) & (sub["variant"] == variant)]
            if row.empty:
                continue
            row = row.iloc[0]
            scope_cell = SCOPE_LABEL[scope] if first else ""
            first = False
            lines.append(
                f"{scope_cell} & {VARIANT_SNR_LABEL[variant]} & "
                f"{fmt_pm(row['accuracy_mean'], row['accuracy_std'], pct=True)} & "
                f"{fmt_pm(row['precision_mean'], row['precision_std'])} & "
                f"{fmt_pm(row['recall_mean'], row['recall_std'])} & "
                f"{fmt_pm(row['f1_mean'], row['f1_std'])} & "
                f"{fmt_pm(row['mcc_mean'], row['mcc_std'])} \\\\"
            )
        if scope != SCOPE_ORDER[-1]:
            lines.append("\\addlinespace")
    lines += ["\\bottomrule", "\\end{tabular}", "}", "\\end{table}"]
    return "\n".join(lines)


def load_quantization_effect_stats() -> dict:
    summary = pd.read_csv(QUANT_MULTISEED_ROOT / "summary_mean_std.csv")
    f32 = summary[summary["mode"] == "float32 (same model)"].set_index(["variant", "scope"])["mcc_mean"]
    int8 = summary[summary["mode"] == "int8 quantized (same model)"].set_index(["variant", "scope"])["mcc_mean"]
    diff = (int8 - f32).dropna()
    return {
        "n_combinations": int(len(diff)),
        "n_int8_better": int((diff > 0).sum()),
        "mean_mcc_diff": float(diff.mean()),
        "std_mcc_diff": float(diff.std()),
    }


def load_weight_provenance_stats() -> dict:
    archive_vs_h5 = pd.read_csv(REPORTS_ROOT / "weights_layers_archive_vs_h5.csv")
    reconverted_vs_h5 = pd.read_csv(REPORTS_ROOT / "weights_layers_reconverted_vs_h5.csv")
    per_sample = pd.read_csv(REPORTS_ROOT / "weights_provenance_per_sample.csv")
    return {
        "archive_vs_h5_corr_range": (
            float(archive_vs_h5["correlation"].min()),
            float(archive_vs_h5["correlation"].max()),
        ),
        "reconverted_vs_h5_corr_min": float(reconverted_vs_h5["correlation"].min()),
        "flip_rate": float(per_sample["flip"].mean()),
        "n_flips": int(per_sample["flip"].sum()),
        "n_samples": int(len(per_sample)),
    }


def load_legacy_bug_stats() -> dict:
    df = pd.read_csv(REPORTS_ROOT / "legacy_bug_metrics.csv")
    return {
        "max_recall": float(df["recall"].max()),
        "accuracy": float(df["accuracy"].iloc[0]),
    }


def make_reviewer_response(summary: pd.DataFrame) -> str:
    quant = load_quantization_effect_stats()
    weights = load_weight_provenance_stats()
    legacy = load_legacy_bug_stats()

    int8_full = summary[(summary["mode"] == "PC int8") & (summary["scope"] == "full")].set_index("variant")
    int8_test = summary[(summary["mode"] == "PC int8") & (summary["scope"] == "test")].set_index("variant")
    f32_full = summary[(summary["mode"] == "PC float32") & (summary["scope"] == "full")].set_index("variant")
    f32_test = summary[(summary["mode"] == "PC float32") & (summary["scope"] == "test")].set_index("variant")

    def mcc(df: pd.DataFrame, variant: str) -> str:
        row = df.loc[variant]
        return f"{row['mcc_mean']:.2f} ± {row['mcc_std']:.2f}"

    lines = []
    lines.append("# Reviewer Response — BEC2026 Paper 53 Camera-Ready")
    lines.append("")
    lines.append(
        "This document maps each major reviewer concern to the reproduced evidence "
        "(see `REPRODUCTION_ROADMAP.md` milestones M1-M4) and a proposed camera-ready wording. "
        "All numbers below are generated from `generated/results/` and `generated/reports/` CSVs "
        "by `ml/make_deliverables.py` — regenerate this file after any re-run instead of hand-editing numbers."
    )
    lines.append("")

    lines.append("## 1. PC-int8 vs. ESP32-S3-int8 discrepancy (R2#6, R3, R4 major concern 1, R4-Q1)")
    lines.append("")
    lines.append(
        "**Concern:** the same quantized model gives materially different results on PC-int8 vs. "
        "ESP32-S3-int8 (e.g. published 30 dB MCC 0.92 vs. 0.99; 5 dB precision/recall inverts "
        "0.53/0.90 vs. 0.95/0.70). Running one fixed int8 model on identical inputs should give "
        "near-identical numbers on both platforms."
    )
    lines.append("")
    lines.append("**Evidence gathered:**")
    lines.append(
        f"- **M1 (weight provenance):** the archived firmware model (`model_data.h`, "
        f"{MODEL_SIZE_BYTES} B) was **not** produced by quantizing the archived float32 model "
        f"(`najlepsi_model.h5`). Dequantized Conv2D/Dense kernels of the two artifacts correlate "
        f"with the float32 reference weights at {weights['archive_vs_h5_corr_range'][0]:.2f} to "
        f"{weights['archive_vs_h5_corr_range'][1]:.2f} (essentially uncorrelated), vs. "
        f"{weights['reconverted_vs_h5_corr_min']:.4f}+ for a freshly-requantized control pair from "
        f"the same float32 model. {weights['n_flips']}/{weights['n_samples']} "
        f"({weights['flip_rate']*100:.0f}%) of clean-sample predictions flip between the two int8 "
        f"artifacts. They are two different trained models of the same architecture, not float32/"
        f"int8 versions of one model."
    )
    lines.append(
        "- **M4 (bug provenance, ruled out):** a legacy PC-side evaluation script "
        "(`ml/eval_tflite_pc.py`) has an int8 output-dequantization overflow bug "
        "(narrow-int8 subtraction under numpy 2.x NEP 50 casting). For this model's output "
        f"quantization (`zero_point=-128`), the bug provably forces recall to exactly "
        f"{legacy['max_recall']:.2f} at every SNR level (confirmed empirically: accuracy pinned "
        f"at {legacy['accuracy']*100:.2f}%, the corpus's non-launch proportion). Published Table "
        "III reports recall 0.90-1.00, directly contradicting this — the bug is **not** the "
        "explanation for the published PC-int8 numbers."
    )
    lines.append("")
    lines.append(
        "**Proposed wording (Section V/VI):** state that the PC-int8 and ESP32-S3-int8 rows in the "
        "original Table III were produced from different trained checkpoints of the same "
        "architecture (traced to a missing `tf.random.set_seed()` in the pre-cleanup training "
        "script), not the same quantized model as originally described. The camera-ready unifies "
        "both a desktop float32 and desktop int8 evaluation on the single archived model "
        "(Tables 2-3 below); a definitive per-sample PC-vs-ESP32 comparison on identical inputs "
        "is deferred to future work pending hardware access (M6)."
    )
    lines.append("")

    lines.append("## 2. \"Quantization improves robustness\" not established (R3, R4 major concern 2, R4-Q2)")
    lines.append("")
    lines.append(
        "**Concern:** going from float32 to int8 raises performance substantially in the original "
        "tables, which is counterintuitive, and the reviewers ask to rule out preprocessing/scaling "
        "differences before presenting this as a robustness benefit."
    )
    lines.append("")
    lines.append("**Evidence gathered (M1, decisive experiment, 5 noise seeds):**")
    lines.append(
        "We evaluated float32 vs. a genuine int8 quantization of the **same** archived model "
        "(`najlepsi_model.h5` vs. a fresh requantization of it), across the same 5 noise seeds used "
        "elsewhere in this reproduction:"
    )
    lines.append(
        f"- Quantization improved MCC in only {quant['n_int8_better']} of {quant['n_combinations']} "
        f"variant x scope combinations; it was neutral-to-slightly-worse in the rest."
    )
    lines.append(
        f"- Mean effect across all combinations: MCC {quant['mean_mcc_diff']:+.4f} "
        f"(std {quant['std_mcc_diff']:.4f}) — quantization on the same model does not meaningfully "
        "change robustness in either direction, and if anything skews slightly negative."
    )
    lines.append(
        "This directly contradicts the original claim. The apparent float32-to-int8 jump in the "
        "original tables is explained by concern 1 above: the float32 and int8 rows come from two "
        "different trained models, not a quantization effect."
    )
    lines.append("")
    lines.append(
        "**Proposed wording:** retract the \"quantization improves robustness\" claim. State plainly "
        "that a controlled same-model comparison shows quantization has a negligible effect on "
        "robustness for this classifier, and that the originally observed improvement was an "
        "artifact of comparing two different trained models (Section VI, reframed per concern 1)."
    )
    lines.append("")

    lines.append("## 3. Evaluation partly on training data (R3, R4 major concern 3, R4-Q3)")
    lines.append("")
    lines.append(
        "**Concern:** the robustness stress test runs on the full corpus (most events seen during "
        "training); results on held-out data are needed for the headline claim."
    )
    lines.append("")
    lines.append("**Evidence gathered (M2 + M3):** a stratified 80/20 held-out test split "
                  "(`datasets/recordings/splits.csv`, seed 42) is now evaluated alongside the "
                  "full corpus, both averaged over 5 noise-realization seeds (Tables 2-3 below). "
                  "**Caveat:** the held-out split contains only 13 launch events (out of 171 total "
                  "test samples), so held-out metrics for the minority class carry wide, "
                  "noise-realization-sensitive intervals — this is disclosed explicitly, not "
                  "smoothed over.")
    lines.append("")
    lines.append(
        f"Held-out int8 MCC at 30 dB: {mcc(int8_test, 'noise_snr30db')} "
        f"(full-corpus: {mcc(int8_full, 'noise_snr30db')}); at 5 dB: "
        f"{mcc(int8_test, 'noise_snr5db')} (full-corpus: {mcc(int8_full, 'noise_snr5db')})."
    )
    lines.append("")
    lines.append(
        "**Proposed wording:** replace the headline full-corpus-only MCC figures with the held-out "
        "column as the primary reported result, keep full-corpus as a secondary/contextual column, "
        "and add the \"13 launch events\" sample-size caveat next to the held-out numbers."
    )
    lines.append("")

    lines.append("## 4. \"Acoustic SNR\" definition (R4-Q4, minor)")
    lines.append("")
    lines.append(
        "**Concern:** \"acoustic SNR\" is undefined — specify the per-event signal/noise power "
        "reference and scaling used to set each SNR level."
    )
    lines.append("")
    lines.append(
        "**Evidence:** the exact formula, `ml/prepare_features.py:31-36` "
        "(`add_snr_noise`), applied in the waveform domain before MFCC extraction:"
    )
    lines.append("")
    lines.append("```")
    lines.append("signal_power = mean(signal ** 2)")
    lines.append("noise_power = signal_power / 10 ** (snr_db / 10)")
    lines.append("noise = Normal(0, sqrt(noise_power))")
    lines.append("noisy_signal = signal + noise")
    lines.append("```")
    lines.append("")
    lines.append(
        "**Proposed wording (Experimental Setup):** \"Acoustic SNR (dB) is defined per event as "
        "$10\\log_{10}(P_\\text{signal}/P_\\text{noise})$, where $P_\\text{signal}$ is the mean squared "
        "amplitude of the clean waveform and additive zero-mean Gaussian noise with the corresponding "
        "power is injected before MFCC extraction.\""
    )
    lines.append("")

    lines.append("## 5. Model size, Flash, RAM, TFLM operator support (R1 Section III.C)")
    lines.append("")
    lines.append(
        f"**Available now (PC-side):** the deployed int8 model is {MODEL_SIZE_BYTES:,} bytes "
        f"({PARAM_COUNT:,} parameters), using an {TENSOR_ARENA_KIB} KiB `tensor_arena`."
    )
    lines.append(
        "**Not available (requires hardware, M6):** Flash usage, RAM usage, and confirmation that "
        "all operators are natively supported by TFLite Micro on ESP32-S3 all require a build/"
        "`idf.py size` pass on the actual target, which was not accessible during this reproduction "
        "pass. **Proposed wording:** report the model size and parameter count now; state Flash/RAM/"
        "operator-support as pending a hardware validation pass, rather than fabricating numbers."
    )
    lines.append("")

    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="M5: generate camera-ready LaTeX tables, reviewer response, and HTML-artifact data.")
    parser.add_argument("--summary-csv", type=Path, default=RESULTS_ROOT / "summary_mean_std.csv")
    parser.add_argument("--output-dir", type=Path, default=REPORTS_ROOT)
    args = parser.parse_args()

    summary = pd.read_csv(args.summary_csv)
    args.output_dir.mkdir(parents=True, exist_ok=True)

    table2 = latex_table(
        summary, "PC float32", "tab:table2_float32",
        "Desktop float32 robustness under waveform-domain additive noise (mean $\\pm$ std over 5 noise seeds; full-corpus and held-out test split).",
    )
    (args.output_dir / "table2_float32.tex").write_text(table2 + "\n", encoding="utf-8")

    table3 = latex_table(
        summary, "PC int8", "tab:table3_int8",
        "PC int8 (archived firmware model) robustness under waveform-domain additive noise (mean $\\pm$ std over 5 noise seeds; full-corpus and held-out test split).",
    )
    (args.output_dir / "table3_int8.tex").write_text(table3 + "\n", encoding="utf-8")

    reviewer_response = make_reviewer_response(summary)
    (args.output_dir / "reviewer_response.md").write_text(reviewer_response + "\n", encoding="utf-8")

    data = {
        "published": {
            "table2_float32": PUBLISHED_TABLE2_FLOAT32,
            "table3_pc_int8": PUBLISHED_TABLE3_PC_INT8,
            "table3_esp32_int8": PUBLISHED_TABLE3_ESP32_INT8,
        },
        "reproduced_summary": summary.to_dict(orient="records"),
        "quantization_effect_same_model": load_quantization_effect_stats(),
        "weight_provenance": load_weight_provenance_stats(),
        "legacy_bug": load_legacy_bug_stats(),
        "facts": {
            "model_size_bytes": MODEL_SIZE_BYTES,
            "param_count": PARAM_COUNT,
            "tensor_arena_kib": TENSOR_ARENA_KIB,
        },
    }
    (args.output_dir / "deliverables_data.json").write_text(json.dumps(data, indent=2), encoding="utf-8")

    print(f"Wrote {args.output_dir / 'table2_float32.tex'}")
    print(f"Wrote {args.output_dir / 'table3_int8.tex'}")
    print(f"Wrote {args.output_dir / 'reviewer_response.md'}")
    print(f"Wrote {args.output_dir / 'deliverables_data.json'}")


if __name__ == "__main__":
    main()
