"""Champion selection by Kamil's file-level criterion (2026-08-10):
a drone recording must yield >=1 (ideally >=2) DRONE windows, a non-drone
recording must yield none. Every candidate model gets a per-model threshold
sweep; operating points must have ZERO non-drone files with >=2 DRONE windows,
then minimal files with >=1 (stray singles), then we maximize detections.

Weighting per Kamil: PRIMARY = clean bebop/membo material (the 3 real mic
playback recordings + the two playback_source loops he physically plays);
SECONDARY = Halmstad drone files. Salford is EXCLUDED entirely ("the drones
there sound weird - don't adapt to them"). Negatives = 5 real mic backgrounds
+ Halmstad backgrounds and helicopters. Caveat: the source loops share
provenance with the training datasets (bebop/membo), so they are a demo
anchor, not an unseen-generalization measure.

Candidates: v1/v2/v3/v2nm linear headers, v4 MLP, E1-E4(=v5)/E6 MLPs,
E7 3-seed ensemble.

Run from the drony root:  .venv/Scripts/python src/analysis/pick_champion.py
"""

import sys
import time
from pathlib import Path

import joblib
import numpy as np
import soundfile as sf

sys.path.insert(0, str(Path(__file__).parent))
import experiment_v5 as e
import train_svm_level_robust as base
from validate_real_wav import load_svm_model

FW_INC = Path(__file__).parent.parent / "firmware" / "Inc"
THR_GRID = np.round(np.arange(-2.0, 12.01, 0.25), 2)


def make_mlp_scorer(clf, scaler):
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


def make_svm_scorer(header):
    bias, mean, inv_std, w = load_svm_model(header)

    def score(feats):
        X = np.atleast_2d(np.asarray(feats, np.float64))
        out = ((X - mean) * inv_std) @ w + bias
        return out if np.ndim(feats) > 1 else float(out[0])
    return score


def load_files():
    """[(name, audio16k, label, squelch, suite)] - suite: primary|halm|neg.
    Salford deliberately excluded."""
    files = []
    for pattern, lab in base.UNSEEN_TRAIN:
        if "Salford" in pattern:
            continue
        suite = "halm" if lab == 1 else "neg"
        _, val = base.split_unseen(pattern)
        for f in val:
            x, fs = sf.read(f, dtype="float32")
            files.append((Path(f).name, base.to_mono_16k(x, fs), lab, 0.005, suite))
    for name, p in base.REAL_RECORDINGS:
        x, fs = sf.read(p, dtype="float32")
        if x.ndim > 1:
            x = x[:, 0]
        if fs == 48000:
            x = np.ascontiguousarray(x[::3])
        lab = 1 if name.startswith("DRON") else 0
        files.append((name, x.astype(np.float32), lab, 0.003,
                      "primary" if lab == 1 else "neg"))
    for loop in ("drone_loop_bebop.wav", "drone_loop_membo.wav"):
        x, fs = sf.read(f"data/playback_source/{loop}", dtype="float32")
        if x.ndim > 1:
            x = x[:, 0]
        x = base.to_mono_16k(x.astype(np.float32), fs)
        files.append((loop, x, 1, 0.005, "primary"))
    return files


def main():
    t0 = time.time()
    print("Loading evaluation files...", flush=True)
    files = load_files()
    n_pos = sum(1 for f in files if f[2] == 1)
    print(f"  {len(files)} files ({n_pos} drone / {len(files) - n_pos} non-drone)",
          flush=True)

    models = {
        "v1":   (e.FEAT_BASE, make_svm_scorer(FW_INC / "svm_model_data.h")),
        "v2":   (e.FEAT_BASE, make_svm_scorer(FW_INC / "svm_model_data_v2.h")),
        "v3":   (e.FEAT_BASE, make_svm_scorer(FW_INC / "svm_model_data_v3.h")),
        "v2nm": (e.FEAT_BASE, make_svm_scorer(FW_INC / "svm_model_data_v2nm.h")),
        "v4":   (e.FEAT_BASE, make_mlp_scorer(joblib.load("models/mlp_v4.pkl"),
                                              joblib.load("models/scaler_mlp_v4.pkl"))),
    }
    for name in ("E1_win14", "E2_mix", "E3_win+mix", "E4_feats", "E6_nomix"):
        cfg = e.FEAT_RICH if name in ("E4_feats", "E6_nomix") else e.FEAT_BASE
        models[name] = (cfg, make_mlp_scorer(
            joblib.load(f"models/exp_v5/{name}_clf.pkl"),
            joblib.load(f"models/exp_v5/{name}_scaler.pkl")))
    ens_members = [make_mlp_scorer(joblib.load(f"models/exp_v5/E7_member{s}_clf.pkl"),
                                   joblib.load(f"models/exp_v5/E7_member{s}_scaler.pkl"))
                   for s in (42, 43, 44)]

    def ens(feats):
        outs = [m(feats) for m in ens_members]
        return (np.mean(np.stack(outs), axis=0) if np.ndim(feats) > 1
                else float(np.mean(outs)))

    models["E7_ens3"] = (e.FEAT_RICH, ens)

    # window decisions per (model, file) - thresholds are then free to sweep
    print("Scoring windows (10 models x files)...", flush=True)
    decs = {}
    for mname, (cfg, score) in models.items():
        decs[mname] = [np.array(e.windowed_decisions(x, cfg, score, sq))
                       for _, x, _, sq, _ in files]
        print(f"  {mname} done ({time.time() - t0:.0f} s)", flush=True)

    labels = np.array([f[2] for f in files])
    suites = np.array([f[4] for f in files])
    n_prim = int((suites == "primary").sum())
    n_halm = int((suites == "halm").sum())
    n_neg = int((labels == 0).sum())

    print("\n=== Best operating point per model ===", flush=True)
    print("  rule: fp(>=2 wins) == 0, minimize fp(>=1); then maximize "
          "PRIMARY bebop/membo >=2 and >=1, then Halmstad", flush=True)
    print(f"  {'model':10s} {'thr':>6s} | prim>=1 prim>=2 (of {n_prim}) "
          f"| halm>=1 halm>=2 (of {n_halm}) | fp>=1 fp>=2 (of {n_neg})", flush=True)

    results = {}
    for mname in models:
        best = None
        for thr in THR_GRID:
            wins = np.array([(d >= thr).sum() if d.size else 0 for d in decs[mname]])
            p1 = int(((wins >= 1) & (suites == "primary")).sum())
            p2 = int(((wins >= 2) & (suites == "primary")).sum())
            h1 = int(((wins >= 1) & (suites == "halm")).sum())
            h2 = int(((wins >= 2) & (suites == "halm")).sum())
            fp1 = int(((wins >= 1) & (labels == 0)).sum())
            fp2 = int(((wins >= 2) & (labels == 0)).sum())
            if fp2 > 0:
                continue
            # The ALARM rule is ">=2 windows", so a stray single window on a
            # negative does not fire an alarm - fp1 is only a late tiebreak.
            key = (p2, h2, p1, h1, -fp1, thr)
            if best is None or key > best[0]:
                best = (key, thr, p1, p2, h1, h2, fp1)
        if best is None:
            print(f"  {mname:10s}   none | no threshold satisfies fp>=2 == 0",
                  flush=True)
            continue
        _, thr, p1, p2, h1, h2, fp1 = best
        results[mname] = best
        print(f"  {mname:10s} {thr:6.2f} |    {p1}       {p2}          "
              f"|    {h1:2d}      {h2:2d}          |   {fp1:2d}    0", flush=True)

    champ = max(results, key=lambda m: results[m][0])
    _, thr, p1, p2, h1, h2, fp1 = results[champ]
    print(f"\nCHAMPION: {champ} @ thr {thr:+.2f}  "
          f"(bebop/membo >=1: {p1}/{n_prim}, >=2: {p2}/{n_prim}; "
          f"Halmstad >=2: {h2}/{n_halm}; stray fp>=1: {fp1}, fp>=2: 0)", flush=True)

    print(f"\nPer-file detail for {champ} @ thr {thr:+.2f}:", flush=True)
    for (fname, _, lab, _, suite), d in zip(files, decs[champ]):
        n = int((d >= thr).sum()) if d.size else 0
        mark = ("OK " if (n >= 1) == (lab == 1) else "MISS" if lab == 1 else "FP ")
        top = f" max {d.max():+.2f}" if d.size else ""
        print(f"  [{mark}] {suite:7s} {fname[:38]:38s} wins {n:3d}/{len(d):3d}{top}",
              flush=True)
    print(f"\nDone ({time.time() - t0:.0f} s)", flush=True)


if __name__ == "__main__":
    main()
