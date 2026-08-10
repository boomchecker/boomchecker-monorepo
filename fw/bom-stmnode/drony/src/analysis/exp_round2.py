"""Experiment round 2: the two untried combinations.

  E6_nomix  rich features + window training WITHOUT SNR mixing - isolates
            whether mixing (which cost specificity in E2/E3) is net-positive
            once the rich features are in.
  E7_ens3   seed ensemble: mean logit of 3 MLPs (seeds 42/43/44) on the full
            v5 recipe - directly attacks the seed instability found when
            refitting E5b (playback domain flipped 30/44 -> 0/44).

Reference = the exact deployed v5 weights (models/exp_v5/E4_feats_*.pkl).
Same three suites as experiment_v5. Candidates saved to models/exp_v5/.

Run from the drony root:  .venv/Scripts/python src/analysis/exp_round2.py
"""

import sys
import time
from pathlib import Path

import joblib
import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import experiment_v5 as e
import train_svm_level_robust as base


def ref_scorer(clf, scaler):
    mu = scaler.mean_.astype(np.float64)
    inv = (1.0 / (scaler.scale_ + 1e-8)).astype(np.float64)
    Ws = [w.astype(np.float64) for w in clf.coefs_]
    bs = [b.astype(np.float64) for b in clf.intercepts_]

    def score(feats):
        Z = (np.atleast_2d(np.asarray(feats, np.float64))[:, 1:] - mu) * inv
        for W, b in zip(Ws[:-1], bs[:-1]):
            Z = np.maximum(Z @ W + b, 0.0)
        out = Z @ Ws[-1][:, 0] + bs[-1][0]
        return out if np.ndim(feats) > 1 else float(out[0])
    return score


def main():
    t0 = time.time()
    log = lambda s: print(s, flush=True)
    print("Loading clips...", flush=True)
    clips, y = base.load_clips()
    from sklearn.model_selection import train_test_split
    idx_train, idx_test = train_test_split(
        np.arange(len(clips)), test_size=0.2, random_state=e.SEED, stratify=y)

    scorers = {
        "v5_ref": (e.FEAT_RICH, ref_scorer(joblib.load("models/exp_v5/E4_feats_clf.pkl"),
                                           joblib.load("models/exp_v5/E4_feats_scaler.pkl"))),
    }

    print("\n[E6_nomix] building rich win14 rows (no mixing) ...", flush=True)
    X6, y6 = e.build_rows(clips, y, idx_train, e.FEAT_RICH, True, False, log)
    print(f"  {X6.shape[0]} rows, fitting (32,) ...", flush=True)
    clf6, sc6, score6, _ = e.fit_mlp(X6, y6, (32,), e.FEAT_RICH)
    joblib.dump(clf6, "models/exp_v5/E6_nomix_clf.pkl")
    joblib.dump(sc6, "models/exp_v5/E6_nomix_scaler.pkl")
    scorers["E6_nomix"] = (e.FEAT_RICH, score6)

    print("\n[E7_ens3] building v5-recipe rows (win14+mix) ...", flush=True)
    X7, y7 = e.build_rows(clips, y, idx_train, e.FEAT_RICH, True, True, log)
    members = []
    for s in (42, 43, 44):
        print(f"  fitting member seed {s} ...", flush=True)
        clf, sc, score, _ = e.fit_mlp(X7, y7, (32,), e.FEAT_RICH, seed=s)
        joblib.dump(clf, f"models/exp_v5/E7_member{s}_clf.pkl")
        joblib.dump(sc, f"models/exp_v5/E7_member{s}_scaler.pkl")
        members.append(score)

    def ens(feats):
        outs = [m(feats) for m in members]
        if np.ndim(feats) > 1:
            return np.mean(np.stack(outs), axis=0)
        return float(np.mean(outs))

    scorers["E7_ens3"] = (e.FEAT_RICH, ens)

    print(f"\n=== Held-out CLIP level (max-window rule, {len(idx_test)} clips) ===",
          flush=True)
    print("  model    | native rec/spec/acc |  -15 dB rec/spec/acc", flush=True)
    for name, (nat, g15) in e.eval_heldout(clips, y, idx_test, scorers).items():
        print(f"  {name:8s} | {nat[0]:.3f}/{nat[1]:.3f}/{nat[2]:.3f}   "
              f"|  {g15[0]:.3f}/{g15[1]:.3f}/{g15[2]:.3f}", flush=True)

    print("\n=== Unseen validation halves (squelch 0.005, thr 0.0) ===", flush=True)
    for pattern, (lab, cells) in e.eval_unseen(scorers, 0.005, 0.0).items():
        nm = pattern.split("/")[-1].replace("_*.wav", "")[:11]
        line = "  ".join(f"{n}: {c[0]}/{c[1]} soub {c[2]:3.0f}%" for n, c in cells.items())
        print(f"  {nm:11s} (label {lab})  {line}", flush=True)

    print("\n=== Real recordings (squelch 0.003, thr 0.0) ===", flush=True)
    for fname, cells in e.eval_real(scorers, 0.003, 0.0).items():
        line = "  ".join(
            f"{n}:{c[0]}/{c[1]}" + (f"({c[2]:+.1f})" if c[2] is not None else "")
            for n, c in cells.items())
        print(f"  {fname:16s} {line}", flush=True)

    print(f"\nDone ({time.time() - t0:.0f} s total)", flush=True)


if __name__ == "__main__":
    main()
