"""v5 experiment session: systematic attempts to beat the v4 classifier.

Experiments (all on level-invariant features - mean-c0 always dropped at fit):
  E0 clip      v4 reproduction under the window-level eval (reference)
  E1 win14     train on 14-frame windows (hop 7) instead of whole clips -
               removes the train(clip)/deploy(14-frame window) mismatch
  E2 mix       whole-clip + synthetic distant drones: positives mixed with
               train backgrounds at SNR 0..15 dB
  E3 win14+mix both
  E4 feats     E3 + richer window aggregation: mean|dMFCC| (prop modulation)
               and max-mean (peakiness) - both computable in firmware from
               the existing 14x13 MFCC block, no new DSP
  E5 arch      best-so-far recipe with wider/deeper nets

Every candidate is scored the same way the firmware runs: 14-frame windows
(classify semantics), clip verdict = max window logit. Suites: held-out
clips (native + -15 dB), Halmstad/Salford validation halves (file + window
level), real mic recordings. NOTE: repeated selection against the same
validation sets can overfit them - prefer candidates that win across ALL
suites, and treat small deltas as noise.

Run from the drony root:  .venv/Scripts/python src/analysis/experiment_v5.py
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
FS = 16000
WLEN, WHOP = 14, 7          # training windows over MFCC frames
POS_WIN_MIN_RMS = 0.002     # drop drone windows this quiet (label noise)
SNR_RANGE = (0.0, 15.0)
K_MIX = 1                   # mixed copies per positive clip
FW_INC = Path(__file__).parent.parent / "firmware" / "Inc"

rng = np.random.default_rng(SEED)

# ---------------------------------------------------------------- features --
FEAT_BASE = dict(delta=False, cmax=False)   # 26 dims (mean13 + std13)
FEAT_RICH = dict(delta=True, cmax=True)     # 52 dims (+ dmean13 + cmax13)


def agg(M, cfg):
    """Aggregate an MFCC block (13, n) -> feature vector. Layout:
    [mean(13), std(13), (dmean 13), (cmax 13)]; index 0 = mean-c0 is the only
    level-dependent entry and is dropped at fit/score time."""
    parts = [M.mean(axis=1), M.std(axis=1)]
    if cfg["delta"]:
        d = np.abs(np.diff(M, axis=1))
        parts.append(d.mean(axis=1) if d.size else np.zeros(13, np.float32))
    if cfg["cmax"]:
        parts.append(M.max(axis=1) - M.mean(axis=1))
    return np.concatenate(parts).astype(np.float32)


def frame_rms(x, n_frames):
    fr = np.lib.stride_tricks.sliding_window_view(x, 1024)[::512][:n_frames]
    return np.sqrt(np.mean(fr.astype(np.float64) ** 2, axis=1))


def clip_features(x, cfg, windows):
    """Feature rows for one waveform: one whole-clip row or one per window."""
    M = base.analyzer.extract_features(x)
    if not windows:
        return [agg(M, cfg)]
    n = M.shape[1]
    if n < WLEN:
        return [agg(M, cfg)]
    return [agg(M[:, s:s + WLEN], cfg) for s in range(0, n - WLEN + 1, WHOP)]


def n_windows_of(x):
    n = 1 + (len(x) - 1024) // 512 if len(x) >= 1024 else 0
    return (n - WLEN) // WHOP + 1 if n >= WLEN else 0


def pos_window_mask(x):
    """Keep-flags for drone windows whose median frame RMS is audible.
    Computed on the NATIVE waveform and applied to every gain variant, so
    gain augmentation keeps teaching invariance instead of losing its quiet
    copies to the filter."""
    nw = n_windows_of(x)
    if nw == 0:
        return None
    n = 1 + (len(x) - 1024) // 512
    fr = frame_rms(x, n)
    return [float(np.median(fr[s:s + WLEN])) >= POS_WIN_MIN_RMS
            for s in range(0, n - WLEN + 1, WHOP)][:nw]


def mix_clip(drone, bg, snr_db):
    b = np.resize(bg, len(drone)).astype(np.float32)
    rd = float(np.sqrt((drone ** 2).mean()))
    rb = float(np.sqrt((b ** 2).mean())) + 1e-12
    return (drone + b * (rd / (10.0 ** (snr_db / 20.0)) / rb)).astype(np.float32)


# ------------------------------------------------------- training set build --
def build_rows(clips, y, idx_train, cfg, windows, mixing, log):
    X, yy = [], []

    def add(x, lab, keep=None):
        rows = clip_features(x, cfg, windows)
        if keep is not None and len(keep) == len(rows):
            rows = [r for r, k in zip(rows, keep) if k]
        X.extend(rows)
        yy.extend([lab] * len(rows))

    neg_pool = [i for i in idx_train if y[i] == 0]
    t0 = time.time()
    for k, i in enumerate(idx_train):
        x = clips[i]
        keep = pos_window_mask(x) if (windows and y[i] == 1) else None
        variants = [x]
        for _ in range(base.K_AUG):
            variants.append(x * 10.0 ** (rng.uniform(*base.GAIN_DB_RANGE) / 20.0))
        if mixing and y[i] == 1:
            for _ in range(K_MIX):
                bg = clips[int(rng.choice(neg_pool))]
                m = mix_clip(x, bg, float(rng.uniform(*SNR_RANGE)))
                variants.append(m)
                variants.append(m * 10.0 ** (rng.uniform(*base.GAIN_DB_RANGE) / 20.0))
        for v in variants:
            add(v, y[i], keep)
        if (k + 1) % 4000 == 0:
            log(f"    {k + 1}/{len(idx_train)} clips ({time.time() - t0:.0f} s)")

    for x in base.synth_quiet_negatives(base.N_QUIET_NEG):
        add(x, 0)
    for p in base.REAL_NEG_FILES:
        x, fs = sf.read(p, dtype="float32")
        if x.ndim > 1:
            x = x[:, 0]
        if fs == 48000:
            x = np.ascontiguousarray(x[::3])
        for s in range(0, len(x) - FS + 1, FS):
            seg = x[s:s + FS]
            for _ in range(base.REAL_NEG_COPIES):
                add(seg * 10.0 ** (rng.uniform(-base.REAL_NEG_JITTER_DB,
                                               base.REAL_NEG_JITTER_DB) / 20.0), 0)
    for seg, lab in zip(*base.load_unseen_train_segments()):
        variants = [seg] + [seg * 10.0 ** (rng.uniform(*base.GAIN_DB_RANGE) / 20.0)
                            for _ in range(base.K_AUG)]
        for v in variants:
            add(v, lab)

    return np.stack(X), np.array(yy)


# ------------------------------------------------------------------ scoring --
def fit_mlp(X, yv, hidden, cfg, seed=SEED):
    from sklearn.neural_network import MLPClassifier
    from sklearn.preprocessing import StandardScaler
    n_pos, n_neg = int((yv == 1).sum()), int((yv == 0).sum())
    extra = rng.choice(np.flatnonzero(yv == 1), size=max(0, n_neg - n_pos), replace=True)
    order = rng.permutation(len(yv) + len(extra))
    Xb = np.concatenate([X, X[extra]])[order][:, 1:]
    yb = np.concatenate([yv, yv[extra]])[order]
    scaler = StandardScaler().fit(Xb)
    clf = MLPClassifier(hidden_layer_sizes=hidden, activation="relu", alpha=1e-4,
                        batch_size=512, learning_rate_init=1e-3, max_iter=300,
                        early_stopping=True, n_iter_no_change=15,
                        validation_fraction=0.1, random_state=seed)
    clf.fit(scaler.transform(Xb), yb)

    mu = scaler.mean_.astype(np.float64)
    inv = (1.0 / (scaler.scale_ + 1e-8)).astype(np.float64)
    Ws = [w.astype(np.float64) for w in clf.coefs_]
    bs = [b.astype(np.float64) for b in clf.intercepts_]

    def score(feats):
        Z = (np.atleast_2d(np.asarray(feats, dtype=np.float64))[:, 1:] - mu) * inv
        for W, b in zip(Ws[:-1], bs[:-1]):
            Z = np.maximum(Z @ W + b, 0.0)
        out = Z @ Ws[-1][:, 0] + bs[-1][0]
        return out if np.ndim(feats) > 1 else float(out[0])

    return clf, scaler, score, cfg


def windowed_decisions(audio, cfg, score, squelch):
    """classify() semantics: squelch resets accumulation, decision per every
    run of 14 accepted frames (non-overlapping), features from the block."""
    if len(audio) < 1024:
        return []
    n = 1 + (len(audio) - 1024) // 512
    M = base.analyzer.extract_features(audio[: (n - 1) * 512 + 1024])
    fr = frame_rms(audio, n)
    out, accum = [], []
    for i in range(n):
        if squelch is not None and fr[i] < squelch:
            accum = []
            continue
        accum.append(i)
        if len(accum) >= WLEN:
            out.append(float(score(agg(M[:, accum], cfg))))
            accum = []
    return out


# --------------------------------------------------------------- eval suites --
def eval_heldout(clips, y, idx_test, scorers):
    """Clip verdict = max window logit (squelch off), native and -15 dB."""
    res = {}
    g15 = 10.0 ** (-15.0 / 20.0)
    for name, (cfg, score) in scorers.items():
        accs = []
        for gain in (1.0, g15):
            pred = []
            for i in idx_test:
                d = windowed_decisions(clips[i] * gain, cfg, score, None)
                pred.append(bool(d) and max(d) >= 0.0)
            pred = np.array(pred)
            yt = y[idx_test]
            accs.append((float(pred[yt == 1].mean()), float((~pred[yt == 0]).mean()),
                         float((pred == yt).mean())))
        res[name] = accs
    return res


def eval_unseen(scorers, squelch, thr):
    rows = {}
    for pattern, lab in base.UNSEEN_TRAIN + [("data/samples/Salford/Calib_*.wav", 0)]:
        _, val_files = base.split_unseen(pattern)
        if not val_files:
            continue
        cells = {}
        for name, (cfg, score) in scorers.items():
            ok = win_pos = win_tot = 0
            for f in val_files:
                x, fs = sf.read(f, dtype="float32")
                x = base.to_mono_16k(x, fs)
                d = windowed_decisions(x, cfg, score, squelch)
                nd = sum(v >= thr for v in d)
                win_pos += nd
                win_tot += len(d)
                if (nd > 0) == (lab == 1):
                    ok += 1
            good = win_pos if lab == 1 else win_tot - win_pos
            cells[name] = (ok, len(val_files),
                           100.0 * good / win_tot if win_tot else float("nan"))
        rows[pattern] = (lab, cells)
    return rows


def eval_real(scorers, squelch, thr):
    rows = {}
    for name_file, p in base.REAL_RECORDINGS:
        x, fs = sf.read(p, dtype="float32")
        if x.ndim > 1:
            x = x[:, 0]
        if fs == 48000:
            x = np.ascontiguousarray(x[::3])
        cells = {}
        for name, (cfg, score) in scorers.items():
            d = windowed_decisions(x, cfg, score, squelch)
            cells[name] = (sum(v >= thr for v in d), len(d),
                           float(np.mean(d)) if d else None)
        rows[name_file] = cells
    return rows


def main():
    t0 = time.time()
    print("Loading clips...", flush=True)
    clips, y = base.load_clips()
    from sklearn.model_selection import train_test_split
    idx_train, idx_test = train_test_split(
        np.arange(len(clips)), test_size=0.2, random_state=SEED, stratify=y)
    log = lambda s: print(s, flush=True)

    # --- reference scorers: v4 (pkl) and v1 (header), evaluated identically --
    import joblib
    v4clf = joblib.load("models/mlp_v4.pkl")
    v4sc = joblib.load("models/scaler_mlp_v4.pkl")

    def make_ref_mlp(clf, scaler):
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

    def make_ref_svm(header):
        bias, mean, inv_std, w = load_svm_model(header)

        def score(feats):
            X = np.atleast_2d(np.asarray(feats, np.float64))
            out = ((X - mean) * inv_std) @ w + bias
            return out if np.ndim(feats) > 1 else float(out[0])
        return score

    scorers = {
        "v1": (FEAT_BASE, make_ref_svm(FW_INC / "svm_model_data.h")),
        "v4": (FEAT_BASE, make_ref_mlp(v4clf, v4sc)),
    }

    # --- experiments ---------------------------------------------------------
    EXPS = [
        ("E1_win14",   FEAT_BASE, True,  False, (32,)),
        ("E2_mix",     FEAT_BASE, False, True,  (32,)),
        ("E3_win+mix", FEAT_BASE, True,  True,  (32,)),
        ("E4_feats",   FEAT_RICH, True,  True,  (32,)),
    ]
    fitted = {}
    for name, cfg, windows, mixing, hidden in EXPS:
        print(f"\n[{name}] building rows (windows={windows} mixing={mixing}) ...",
              flush=True)
        X, yv = build_rows(clips, y, idx_train, cfg, windows, mixing, log)
        print(f"  {X.shape[0]} rows x {X.shape[1]} feats "
              f"({int((yv == 1).sum())} pos), fitting {hidden} ...", flush=True)
        clf, scaler, score, cfg_ = fit_mlp(X, yv, hidden, cfg)
        fitted[name] = (X, yv, clf, scaler)
        scorers[name] = (cfg_, score)
        print(f"  done ({time.time() - t0:.0f} s total)", flush=True)

    # E5: architecture sweep on the winning data recipe happens after a first
    # look - here we just add one deeper candidate on E3/E4 rows for free.
    for name, hidden in (("E5a_deep", (32, 16)), ("E5b_wide", (64,))):
        src = "E4_feats"
        X, yv, _, _ = fitted[src]
        cfg = FEAT_RICH
        print(f"\n[{name}] fitting {hidden} on {src} rows ...", flush=True)
        _, _, score, cfg_ = fit_mlp(X, yv, hidden, cfg)
        scorers[name] = (cfg_, score)

    # --- evaluations ----------------------------------------------------------
    print(f"\n=== Held-out CLIP level (max-window rule, {len(idx_test)} clips) ===",
          flush=True)
    print("  model      | native rec/spec/acc |  -15 dB rec/spec/acc", flush=True)
    ho = eval_heldout(clips, y, idx_test, scorers)
    for name, (nat, g15) in ho.items():
        print(f"  {name:10s} | {nat[0]:.3f}/{nat[1]:.3f}/{nat[2]:.3f}   "
              f"|  {g15[0]:.3f}/{g15[1]:.3f}/{g15[2]:.3f}", flush=True)

    for sq, thr in ((0.005, 0.0),):
        print(f"\n=== Unseen validation halves (squelch {sq}, thr {thr}) ===", flush=True)
        rows = eval_unseen(scorers, sq, thr)
        for pattern, (lab, cells) in rows.items():
            nm = pattern.split("/")[-1].replace("_*.wav", "")[:11]
            line = "  ".join(f"{n}: {c[0]}/{c[1]} soub {c[2]:3.0f}%"
                             for n, c in cells.items())
            print(f"  {nm:11s} (label {lab})  {line}", flush=True)

    for sq, thr in ((0.003, 0.0),):
        print(f"\n=== Real recordings (squelch {sq}, thr {thr}) ===", flush=True)
        rows = eval_real(scorers, sq, thr)
        for fname, cells in rows.items():
            line = "  ".join(
                f"{n}:{c[0]}/{c[1]}" + (f"({c[2]:+.1f})" if c[2] is not None else "")
                for n, c in cells.items())
            print(f"  {fname:16s} {line}", flush=True)

    # --- persist fitted candidates so the winner can be exported without refit
    import joblib
    Path("models/exp_v5").mkdir(parents=True, exist_ok=True)
    for name, (X, yv, clf, scaler) in fitted.items():
        joblib.dump(clf, f"models/exp_v5/{name}_clf.pkl")
        joblib.dump(scaler, f"models/exp_v5/{name}_scaler.pkl")
    print(f"\nCandidates saved to models/exp_v5/  ({time.time() - t0:.0f} s total)",
          flush=True)


if __name__ == "__main__":
    main()
