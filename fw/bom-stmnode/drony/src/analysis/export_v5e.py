"""Export the E7 seed-ensemble (3x MLP 51->32->1, mean logit) as the v5e
firmware header. Field-deployment candidate from exp_round2.py: best on real
distant flyovers (Salford 72 % windows), seed-stable by construction; muted
on the speaker-playback domain (expected - see PROGRESS_REPORT section 11).

Each member carries its own StandardScaler (fit_mlp fits one per call), so
the header stores per-member scaler arrays and the C forward averages the
three member logits. Feature layout identical to v5 (52-dim det_aggregate,
inputs = features[1..51]).

Run from the drony root:  .venv/Scripts/python src/analysis/export_v5e.py
"""

import sys
from pathlib import Path

import joblib
import numpy as np

sys.path.insert(0, str(Path(__file__).parent))

FW_INC = Path(__file__).parent.parent / "firmware" / "Inc"
SEEDS = (42, 43, 44)


def member_logit(clf, scaler, X):
    Z = (X - scaler.mean_.astype(np.float32)) * \
        (1.0 / (scaler.scale_ + 1e-8)).astype(np.float32)
    H = np.maximum(Z @ clf.coefs_[0].astype(np.float32)
                   + clf.intercepts_[0].astype(np.float32), 0.0)
    return H @ clf.coefs_[1][:, 0].astype(np.float32) + float(clf.intercepts_[1][0])


def main():
    members = [(joblib.load(f"models/exp_v5/E7_member{s}_clf.pkl"),
                joblib.load(f"models/exp_v5/E7_member{s}_scaler.pkl"))
               for s in SEEDS]
    n_in = members[0][1].mean_.shape[0]
    hidden = members[0][0].coefs_[0].shape[1]
    for clf, sc in members:
        assert sc.mean_.shape[0] == n_in and clf.coefs_[0].shape[1] == hidden

    # parity: header math must reproduce sklearn probabilities per member
    rng = np.random.default_rng(0)
    X = (rng.standard_normal((256, n_in)) * 3.0).astype(np.float32)
    for i, (clf, sc) in enumerate(members):
        with np.errstate(over="ignore"):
            p_ours = 1.0 / (1.0 + np.exp(-member_logit(clf, sc, X).astype(np.float64)))
        p_skl = clf.predict_proba(sc.transform(X))[:, 1]
        diff = float(np.max(np.abs(p_ours - p_skl)))
        assert diff < 5e-4, f"member {SEEDS[i]}: parity failed ({diff:.2e})"

    def arr(a):
        return ", ".join(f"{v:.8e}f" for v in np.asarray(a, dtype=np.float32))

    def arr2(m):
        return ",\n     ".join("{" + arr(r) + "}" for r in m)

    mean_rows = ",\n    ".join("{" + arr(sc.mean_) + "}" for _, sc in members)
    inv_rows = ",\n    ".join("{" + arr(1.0 / (sc.scale_ + 1e-8)) + "}"
                              for _, sc in members)
    w1_rows = ",\n    ".join("{" + arr2(clf.coefs_[0].T) + "}" for clf, _ in members)
    b1_rows = ",\n    ".join("{" + arr(clf.intercepts_[0]) + "}" for clf, _ in members)
    w2_rows = ",\n    ".join("{" + arr(clf.coefs_[1][:, 0]) + "}" for clf, _ in members)
    b2_row = arr([float(clf.intercepts_[1][0]) for clf, _ in members])

    out = FW_INC / "mlp_model_data_v5e.h"
    out.write_text(
        "#ifndef MLP_MODEL_DATA_V5E_H\n#define MLP_MODEL_DATA_V5E_H\n\n"
        "// v5e = seed ensemble (exp_round2.py E7): mean logit of 3 MLPs\n"
        "// 51->32->1 trained on the v5 recipe with seeds 42/43/44. Field\n"
        "// candidate: best on real distant flyovers, conservative on speaker\n"
        "// playback. Feature layout identical to v5 (det_aggregate 52-dim,\n"
        "// inputs = features[1..51]); decision = mean logit, neutral thr 0.\n\n"
        f"#define MLP_NUM_INPUTS {n_in}\n#define MLP_HIDDEN {hidden}\n"
        f"#define MLP_ENSEMBLE {len(members)}\n\n"
        f"static const float mlp_scaler_mean[MLP_ENSEMBLE][MLP_NUM_INPUTS] = {{\n    {mean_rows}\n}};\n\n"
        f"static const float mlp_scaler_inv_std[MLP_ENSEMBLE][MLP_NUM_INPUTS] = {{\n    {inv_rows}\n}};\n\n"
        f"static const float mlp_w1[MLP_ENSEMBLE][MLP_HIDDEN][MLP_NUM_INPUTS] = {{\n    {w1_rows}\n}};\n\n"
        f"static const float mlp_b1[MLP_ENSEMBLE][MLP_HIDDEN] = {{\n    {b1_rows}\n}};\n\n"
        f"static const float mlp_w2[MLP_ENSEMBLE][MLP_HIDDEN] = {{\n    {w2_rows}\n}};\n\n"
        f"static const float mlp_b2[MLP_ENSEMBLE] = {{\n    {b2_row}\n}};\n\n"
        "#endif // MLP_MODEL_DATA_V5E_H\n",
        encoding="utf-8",
    )
    n_floats = len(members) * (2 * n_in + n_in * hidden + 2 * hidden + 1)
    print(f"{out.name} written ({n_floats} floats = {n_floats * 4 / 1024:.1f} kB, "
          f"parity OK for all {len(members)} members)", flush=True)


if __name__ == "__main__":
    main()
