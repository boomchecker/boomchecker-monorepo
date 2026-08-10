"""Refit the E5b_wide candidate (hidden 64 on rich win14+mix rows) and
re-evaluate it before export - the original experiment run didn't persist it,
and a refit consumes different rng draws, so its metrics must be confirmed
rather than assumed. Saves models/exp_v5/E5b_wide_{clf,scaler}.pkl on success.

Run from the drony root:  .venv/Scripts/python src/analysis/refit_e5b.py
"""

import sys
import time
from pathlib import Path

import joblib
import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import experiment_v5 as e
import train_svm_level_robust as base


def main():
    t0 = time.time()
    print("Loading clips...", flush=True)
    clips, y = base.load_clips()
    from sklearn.model_selection import train_test_split
    idx_train, idx_test = train_test_split(
        np.arange(len(clips)), test_size=0.2, random_state=e.SEED, stratify=y)

    print("Building rich win14+mix rows...", flush=True)
    X, yv = e.build_rows(clips, y, idx_train, e.FEAT_RICH, True, True,
                         lambda s: print(s, flush=True))
    print(f"  {X.shape[0]} rows, fitting (64,) ...", flush=True)
    clf, scaler, score, cfg = e.fit_mlp(X, yv, (64,), e.FEAT_RICH)

    scorers = {"E5b_refit": (cfg, score)}
    print("\n=== Real recordings (squelch 0.003, thr 0.0) ===", flush=True)
    for fname, cells in e.eval_real(scorers, 0.003, 0.0).items():
        c = cells["E5b_refit"]
        print(f"  {fname:16s} {c[0]}/{c[1]}" +
              (f" ({c[2]:+.1f})" if c[2] is not None else ""), flush=True)
    print("\n=== Unseen validation halves (squelch 0.005, thr 0.0) ===", flush=True)
    for pattern, (lab, cells) in e.eval_unseen(scorers, 0.005, 0.0).items():
        ok, n, pct = cells["E5b_refit"]
        nm = pattern.split("/")[-1].replace("_*.wav", "")
        print(f"  {nm:11s} (label {lab})  {ok}/{n} soub  {pct:3.0f}%", flush=True)

    joblib.dump(clf, "models/exp_v5/E5b_wide_clf.pkl")
    joblib.dump(scaler, "models/exp_v5/E5b_wide_scaler.pkl")
    print(f"\nSaved models/exp_v5/E5b_wide_*.pkl  ({time.time() - t0:.0f} s)", flush=True)


if __name__ == "__main__":
    main()
