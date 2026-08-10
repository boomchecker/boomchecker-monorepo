"""v4: tiny MLP on level-invariant features + expanded training set.

Motivation (see PROGRESS_REPORT_2026-08.md and the v2nm run): the v1-v3
linear SVMs hit a capacity ceiling, and v2nm showed that dropping mean-c0
gives perfect level invariance but costs sensitivity a linear boundary
cannot win back. This script keeps the invariant features (column 0 =
mean-c0 removed -> 25 inputs) and replaces the linear boundary with a tiny
MLP: StandardScaler -> 25 -> H (ReLU) -> 1 logit.

Training set = the v3 recipe (largest we have):
  parquet shards + DroneAudioDataset, each clip at K_AUG extra random gains
  (the ln(x+1e-6) epsilon still bends invariant features near the noise
  floor), synthetic near-silence negatives, real mic-background anchors,
  and the TRAIN halves of Halmstad/Salford (drones, backgrounds,
  helicopters). Real playback recordings stay untouched validation.
  Positives are oversampled to parity (MLPClassifier has no class_weight).

Two hidden sizes are trained; the export picks the better held-out
-15 dB accuracy. Output decision = raw logit (threshold 0 == p 0.5), so the
firmware `detect` threshold argument keeps working (thr_milli 0 = neutral).

Outputs:
  models/mlp_v4.pkl, models/scaler_mlp_v4.pkl
  src/firmware/Inc/mlp_model_data_v4.h   (forward pass in svm_classifier.c)

Run from the drony root:  .venv/Scripts/python src/analysis/train_mlp_v4.py
"""

import sys
import time
from pathlib import Path

import numpy as np
import soundfile as sf

sys.path.insert(0, str(Path(__file__).parent))
import train_svm_level_robust as base
from validate_real_wav import load_svm_model

SEED = 42
VERSION = "v4"
HIDDEN_CANDIDATES = (16, 32)
FW_INC = Path(__file__).parent.parent / "firmware" / "Inc"

rng = np.random.default_rng(SEED)


def build_training_set():
    """v3 data recipe, mirrored from train_svm_level_robust.main()."""
    t0 = time.time()
    print("Loading clips...", flush=True)
    clips, y = base.load_clips()
    print(f"total {len(clips)} clips ({int((y == 1).sum())} drone / "
          f"{int((y == 0).sum())} noise), {time.time() - t0:.0f} s", flush=True)

    from sklearn.model_selection import train_test_split
    idx_train, idx_test = train_test_split(
        np.arange(len(clips)), test_size=0.2, random_state=SEED, stratify=y)

    print("Extracting features (train with augmentation)...", flush=True)
    X_train, y_train = [], []
    for k, i in enumerate(idx_train):
        x = clips[i]
        X_train.append(base.extract_features(x))
        y_train.append(y[i])
        for _ in range(base.K_AUG):
            g = 10.0 ** (rng.uniform(*base.GAIN_DB_RANGE) / 20.0)
            X_train.append(base.extract_features(x * g))
            y_train.append(y[i])
        if (k + 1) % 2000 == 0:
            print(f"  train {k + 1}/{len(idx_train)}  ({time.time() - t0:.0f} s)", flush=True)

    print(f"Adding {base.N_QUIET_NEG} synthetic near-silence negatives...", flush=True)
    for x in base.synth_quiet_negatives(base.N_QUIET_NEG):
        X_train.append(base.extract_features(x))
        y_train.append(0)

    print("Adding real mic-background anchors (train only)...", flush=True)
    for p in base.REAL_NEG_FILES:
        x, fs = sf.read(p, dtype="float32")
        if x.ndim > 1:
            x = x[:, 0]
        if fs == 48000:
            x = np.ascontiguousarray(x[::3])
        for s in range(0, len(x) - base.FS + 1, base.FS):
            seg = x[s:s + base.FS]
            for _ in range(base.REAL_NEG_COPIES):
                g = 10.0 ** (rng.uniform(-base.REAL_NEG_JITTER_DB,
                                         base.REAL_NEG_JITTER_DB) / 20.0)
                X_train.append(base.extract_features(seg * g))
                y_train.append(0)

    print("Adding unseen-dataset train segments (train halves only)...", flush=True)
    u_segs, u_labels = base.load_unseen_train_segments()
    for seg, lab in zip(u_segs, u_labels):
        X_train.append(base.extract_features(seg))
        y_train.append(lab)
        for _ in range(base.K_AUG):
            g = 10.0 ** (rng.uniform(*base.GAIN_DB_RANGE) / 20.0)
            X_train.append(base.extract_features(seg * g))
            y_train.append(lab)

    print("Extracting features (test, native + robustness gain)...", flush=True)
    g15 = 10.0 ** (base.ROBUST_TEST_GAIN_DB / 20.0)
    X_test = np.stack([base.extract_features(clips[i]) for i in idx_test])
    X_test15 = np.stack([base.extract_features(clips[i] * g15) for i in idx_test])

    return (np.stack(X_train), np.array(y_train),
            X_test, X_test15, y[idx_test], t0)


def make_scorer(scaler, clf):
    """Callable 26-feature vector (or matrix) -> logit, mirroring the C code."""
    mu = scaler.mean_.astype(np.float64)
    inv = (1.0 / (scaler.scale_ + 1e-8)).astype(np.float64)
    W1, W2 = clf.coefs_[0], clf.coefs_[1][:, 0]
    b1, b2 = clf.intercepts_[0], float(clf.intercepts_[1][0])

    def score(feats):
        X = np.atleast_2d(np.asarray(feats, dtype=np.float64))
        Z = (X[:, 1:] - mu) * inv
        out = np.maximum(Z @ W1 + b1, 0.0) @ W2 + b2
        return out if np.ndim(feats) > 1 else float(out[0])

    return score


def export_header(path, scaler, clf, hidden, note):
    mu = scaler.mean_.astype(np.float32)
    inv = (1.0 / (scaler.scale_ + 1e-8)).astype(np.float32)
    W1 = clf.coefs_[0].astype(np.float32).T          # (hidden, 25) row-major
    b1 = clf.intercepts_[0].astype(np.float32)
    W2 = clf.coefs_[1][:, 0].astype(np.float32)
    b2 = float(clf.intercepts_[1][0])

    def arr(a):
        return ", ".join(f"{v:.8e}f" for v in a)

    rows = ",\n    ".join("{" + arr(r) + "}" for r in W1)
    path.write_text(
        "#ifndef MLP_MODEL_DATA_V4_H\n#define MLP_MODEL_DATA_V4_H\n\n"
        "// Tiny MLP drone classifier v4 (see src/analysis/train_mlp_v4.py).\n"
        "// Inputs: features[1..25] of the standard 26-vector (mean-c0 excluded\n"
        "// -> level-invariant). Decision = raw logit; threshold 0 == p 0.5.\n"
        f"// {note}\n\n"
        f"#define MLP_NUM_INPUTS 25\n#define MLP_HIDDEN {hidden}\n"
        f"#define MLP_B2 {b2:.8e}f\n\n"
        f"static const float mlp_scaler_mean[MLP_NUM_INPUTS] = {{\n    {arr(mu)}\n}};\n\n"
        f"static const float mlp_scaler_inv_std[MLP_NUM_INPUTS] = {{\n    {arr(inv)}\n}};\n\n"
        f"static const float mlp_w1[MLP_HIDDEN][MLP_NUM_INPUTS] = {{\n    {rows}\n}};\n\n"
        f"static const float mlp_b1[MLP_HIDDEN] = {{\n    {arr(b1)}\n}};\n\n"
        f"static const float mlp_w2[MLP_HIDDEN] = {{\n    {arr(W2)}\n}};\n\n"
        "#endif // MLP_MODEL_DATA_V4_H\n",
        encoding="utf-8",
    )


def held_out_metrics(score, X, y):
    pred = (score(X) if callable(score) else base.decisions(score, X)) >= 0
    return (float(pred[y == 1].mean()), float((~pred[y == 0]).mean()),
            float((pred == y).mean()))


def main():
    X_train, y_train, X_test, X_test15, y_test, t0 = build_training_set()
    n_pos, n_neg = int((y_train == 1).sum()), int((y_train == 0).sum())
    print(f"train matrix {X_train.shape}, {n_pos} pos / {n_neg} neg", flush=True)

    # Oversample positives to parity, then drop mean-c0 (column 0).
    extra = rng.choice(np.flatnonzero(y_train == 1), size=n_neg - n_pos, replace=True)
    order = rng.permutation(len(y_train) + len(extra))
    X_bal = np.concatenate([X_train, X_train[extra]])[order][:, 1:]
    y_bal = np.concatenate([y_train, y_train[extra]])[order]

    from sklearn.neural_network import MLPClassifier
    from sklearn.preprocessing import StandardScaler
    scaler = StandardScaler().fit(X_bal)
    Xs = scaler.transform(X_bal)

    candidates = {}
    for h in HIDDEN_CANDIDATES:
        print(f"Training MLP 25->{h}->1 ...", flush=True)
        clf = MLPClassifier(hidden_layer_sizes=(h,), activation="relu",
                            alpha=1e-4, batch_size=512, learning_rate_init=1e-3,
                            max_iter=300, early_stopping=True, n_iter_no_change=15,
                            validation_fraction=0.1, random_state=SEED)
        clf.fit(Xs, y_bal)
        score = make_scorer(scaler, clf)
        # Export parity: sigmoid(our logit) vs sklearn predict_proba. The
        # scorer mirrors the C code's 1/(scale+1e-8) while sklearn divides by
        # scale exactly, so allow that tiny drift; a real export bug (wrong
        # transpose/order) shows up as ~0.3.
        idx = rng.choice(len(X_test), size=200, replace=False)
        with np.errstate(over="ignore"):
            p_ours = 1.0 / (1.0 + np.exp(-np.asarray(score(X_test[idx]))))
        p_skl = clf.predict_proba(scaler.transform(X_test[idx][:, 1:]))[:, 1]
        diff = float(np.max(np.abs(p_ours - p_skl)))
        assert diff < 5e-4, f"export parity failed (max prob diff {diff:.2e})"
        print(f"  parity vs sklearn OK (max prob diff {diff:.1e})", flush=True)
        candidates[h] = (clf, score)

    print(f"\n=== Held-out metrics (threshold 0, {len(y_test)} clips) ===", flush=True)
    print("  model | native recall/spec/acc | -15 dB recall/spec/acc", flush=True)
    models = {"v1": load_svm_model(FW_INC / "svm_model_data.h")}
    for prev in ("v2", "v3", "v2nm"):
        h = FW_INC / f"svm_model_data_{prev}.h"
        if h.exists():
            models[prev] = load_svm_model(h)
    for h, (clf, score) in candidates.items():
        models[f"v4h{h}"] = score
    acc15 = {}
    for label, model in models.items():
        cells = []
        for X in (X_test, X_test15):
            rec, spec, acc = held_out_metrics(model, X, y_test)
            cells.append(f"{rec:.3f}/{spec:.3f}/{acc:.3f}")
        if label.startswith("v4h"):
            _, _, acc15[label] = held_out_metrics(model, X_test15, y_test)
        print(f"  {label:5s} |   {cells[0]}    |   {cells[1]}", flush=True)

    best = max(acc15, key=acc15.get)
    hidden = int(best[3:])
    clf, score = candidates[hidden]
    print(f"\nExporting {best} (held-out -15 dB acc {acc15[best]:.3f})", flush=True)
    models["v4"] = score
    for k in list(models):
        if k.startswith("v4h"):
            del models[k]

    base.eval_unseen_val(models, 0.005, 0.5)
    base.eval_unseen_val(models, 0.005, 0.25)

    print("\n=== Real recordings (DRONE windows / total, mean decision) ===", flush=True)
    for sq in (0.003, 0.005):
        for thr in (0.25, 0.0):
            print(f"\n-- squelch {sq}  threshold {thr} --", flush=True)
            for label, model in models.items():
                rows = base.eval_real(model, sq, thr)
                cells = []
                for name, ndrone, ntot, dmean in rows:
                    cells.append(f"{name.split()[-1]}:{ndrone}/{ntot}" +
                                 (f"({dmean:+.1f})" if dmean is not None else ""))
                print(f"  {label}: " + "  ".join(cells), flush=True)

    import joblib
    Path("models").mkdir(exist_ok=True)
    joblib.dump(clf, "models/mlp_v4.pkl")
    joblib.dump(scaler, "models/scaler_mlp_v4.pkl")
    note = (f"25->{hidden}->1 ReLU; held-out -15dB acc {acc15[best]:.3f}; "
            f"trained {time.strftime('%Y-%m-%d')} on the v3 recipe, "
            "positives oversampled to parity")
    export_header(FW_INC / "mlp_model_data_v4.h", scaler, clf, hidden, note)
    print(f"\nSaved models/mlp_v4.pkl, models/scaler_mlp_v4.pkl, "
          f"src/firmware/Inc/mlp_model_data_v4.h  ({time.time() - t0:.0f} s total)",
          flush=True)


if __name__ == "__main__":
    main()
