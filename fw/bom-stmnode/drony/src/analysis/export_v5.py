"""Export the chosen v5 experiment candidates to firmware MLP headers.

v5  = E4_feats  (25->... 52-dim rich features, hidden 32): best quiet-drone
      sensitivity (tichy 27/33) - the default.
v5b = E5b_wide  (same rows, hidden 64): cleanest backgrounds + best mid-volume
      and helicopter rejection - the conservative alternative.

Feature layout (must match detector.c det_aggregate and experiment_v5.agg):
  [mean(13), std(13), dmean(13) = mean |frame-to-frame delta|, cmax(13) = max-mean]
Index 0 (mean-c0, the only level-dependent entry) is dropped at scoring time,
so MLP_NUM_INPUTS = 51 and the C code reads p_features[1..51].

Run from the drony root:  .venv/Scripts/python src/analysis/export_v5.py
"""

import sys
from pathlib import Path

import joblib
import numpy as np

sys.path.insert(0, str(Path(__file__).parent))

FW_INC = Path(__file__).parent.parent / "firmware" / "Inc"
EXPORTS = [
    ("E4_feats", "mlp_model_data_v5.h", "MLP_MODEL_DATA_V5_H",
     "v5 = experiment E4 (win14+mix train, rich features): quiet-drone 27/33"),
    ("E6_nomix", "mlp_model_data_v6.h", "MLP_MODEL_DATA_V6_H",
     "v6 = experiment E6 (win14, rich features, no mixing) - pick_champion.py "
     "winner @ thr 7.25: bebop/membo alarms 4/5, zero >=2-window FPs"),
    # E5b_wide deliberately NOT exported: the original evaluated instance was
    # not persisted, and a refit (refit_e5b.py, 2026-08-10) did not reproduce
    # its real-recording strengths (stredni 30/44 -> 0/44) - the playback
    # domain is out-of-distribution and seed-unstable. Fix the domain with
    # real recordings in training before shipping a "conservative" variant.
]


def write_header(path, guard, scaler, clf, note):
    mu = scaler.mean_.astype(np.float32)
    inv = (1.0 / (scaler.scale_ + 1e-8)).astype(np.float32)
    W1 = clf.coefs_[0].astype(np.float32).T          # (hidden, n_in)
    b1 = clf.intercepts_[0].astype(np.float32)
    W2 = clf.coefs_[1][:, 0].astype(np.float32)
    b2 = float(clf.intercepts_[1][0])
    n_in, hidden = W1.shape[1], W1.shape[0]

    def arr(a):
        return ", ".join(f"{v:.8e}f" for v in a)

    rows = ",\n    ".join("{" + arr(r) + "}" for r in W1)
    path.write_text(
        f"#ifndef {guard}\n#define {guard}\n\n"
        "// Tiny MLP drone classifier (see src/analysis/experiment_v5.py and\n"
        "// export_v5.py). Feature layout from detector.c det_aggregate:\n"
        "// [mean(13), std(13), dmean(13), cmax(13)]; inputs = features[1..51]\n"
        "// (mean-c0 excluded -> level-invariant). Decision = raw logit;\n"
        "// threshold 0 == p 0.5.\n"
        f"// {note}\n\n"
        f"#define MLP_NUM_INPUTS {n_in}\n#define MLP_HIDDEN {hidden}\n"
        f"#define MLP_B2 {b2:.8e}f\n\n"
        f"static const float mlp_scaler_mean[MLP_NUM_INPUTS] = {{\n    {arr(mu)}\n}};\n\n"
        f"static const float mlp_scaler_inv_std[MLP_NUM_INPUTS] = {{\n    {arr(inv)}\n}};\n\n"
        f"static const float mlp_w1[MLP_HIDDEN][MLP_NUM_INPUTS] = {{\n    {rows}\n}};\n\n"
        f"static const float mlp_b1[MLP_HIDDEN] = {{\n    {arr(b1)}\n}};\n\n"
        f"static const float mlp_w2[MLP_HIDDEN] = {{\n    {arr(W2)}\n}};\n\n"
        f"#endif // {guard}\n",
        encoding="utf-8",
    )


def parity_check(scaler, clf):
    """The header numbers must reproduce sklearn's probabilities."""
    rng = np.random.default_rng(0)
    n_in = scaler.mean_.shape[0]
    X = rng.standard_normal((256, n_in)).astype(np.float32) * 3.0
    mu = scaler.mean_.astype(np.float32)
    inv = (1.0 / (scaler.scale_ + 1e-8)).astype(np.float32)
    Z = (X - mu) * inv
    H = np.maximum(Z @ clf.coefs_[0].astype(np.float32)
                   + clf.intercepts_[0].astype(np.float32), 0.0)
    logit = H @ clf.coefs_[1][:, 0].astype(np.float32) + float(clf.intercepts_[1][0])
    with np.errstate(over="ignore"):
        p_ours = 1.0 / (1.0 + np.exp(-logit.astype(np.float64)))
    p_skl = clf.predict_proba(scaler.transform(X))[:, 1]
    return float(np.max(np.abs(p_ours - p_skl)))


def main():
    for name, fname, guard, note in EXPORTS:
        clf = joblib.load(f"models/exp_v5/{name}_clf.pkl")
        scaler = joblib.load(f"models/exp_v5/{name}_scaler.pkl")
        diff = parity_check(scaler, clf)
        assert diff < 5e-4, f"{name}: parity failed ({diff:.2e})"
        out = FW_INC / fname
        write_header(out, guard, scaler, clf, note)
        n_floats = (scaler.mean_.size * 2 + clf.coefs_[0].size
                    + clf.intercepts_[0].size + clf.coefs_[1].size + 1)
        print(f"{name}: {out.name} written ({n_floats} floats = "
              f"{n_floats * 4 / 1024:.1f} kB, parity diff {diff:.1e})", flush=True)


if __name__ == "__main__":
    main()
