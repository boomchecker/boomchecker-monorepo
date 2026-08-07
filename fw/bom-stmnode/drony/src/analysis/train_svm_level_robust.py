"""Level-robust retrain of the drone SVM (gain augmentation).

The original model's largest weight sits on mean-c0, which carries absolute
signal level (gain g shifts mean-c0 by sqrt(20)*ln(g) and nothing else), so
quiet/distant drones score as noise (verified on real PDM-mic recordings,
2026-08-07). This script retrains the same 26-feature linear SVM with each
training clip additionally presented at K random gains in GAIN_DB_RANGE, so
the classifier learns level invariance itself. The feature layout is
unchanged -> the firmware only needs the regenerated weight header, no C
changes.

Outputs:
  models/drone_detector_svm_v2.pkl, models/scaler_v2.pkl
  src/firmware/Inc/svm_model_data_v2.h
  console report: old vs new model on held-out data (native and -15 dB) and
  on the real mic recordings in data/recordings/.

Run from the drony root:  .venv/Scripts/python src/analysis/train_svm_level_robust.py
"""

import glob
import io
import sys
import time
from pathlib import Path

import numpy as np
import soundfile as sf

sys.path.insert(0, str(Path(__file__).parent))
from mfcc_analyzer import MFCCAnalyzer
from validate_real_wav import MODEL_HEADER, classify, load_svm_model

SEED = 42
FS = 16000
MIN_SAMPLES = 1024
K_AUG = 2                      # extra gain-shifted copies per training clip
GAIN_DB_RANGE = (-30.0, 3.0)
MAX_PARQUET = 2000             # per shard, matches train_svm.py
ROBUST_TEST_GAIN_DB = -15.0
# Synthetic near-silence negatives: white/pink noise in the true near-silence
# band only. Without them a level-invariant model drifts to calling
# featureless quiet audio "drone" (v2 run); with the band reaching 0.02 RMS
# they swallow quiet drones instead (v2b run) - keep the cap below the
# quietest expected drone level.
N_QUIET_NEG = 1500
QUIET_RMS_RANGE = (0.0004, 0.004)

# Real mic-background anchors (train-only): sliced into 1 s clips, duplicated
# with small gain jitter. bg_ticho + bg_ambient only - speech/claps/music and
# all playback recordings stay as untouched validation.
REAL_NEG_FILES = [
    "data/recordings/background/bg_ticho_01.wav",
    "data/recordings/background/bg_ambient_prvni_test.wav",
]
REAL_NEG_COPIES = 10
REAL_NEG_JITTER_DB = 3.0

# v3: unseen-dataset training material (train halves only; the other halves
# stay as untouched validation). Files sorted by name; EVEN indices -> train,
# ODD indices -> validation. 10 s clips are sliced into 1 s segments; drone
# segments quieter than SEG_MIN_RMS are dropped (drone likely inaudible there).
VERSION = "v3"
UNSEEN_TRAIN = [
    # (glob, label)
    ("data/samples/Halmstad/DRONE_*.wav",      1),
    ("data/samples/Halmstad/BACKGROUND_*.wav", 0),
    ("data/samples/Halmstad/HELICOPTER_*.wav", 0),
    ("data/samples/Salford/Ed_*.wav",          1),
]
SEG_SECONDS = 1.0
SEG_MIN_RMS = 0.003

WAV_ROOT = Path("wav/DroneAudioDataset-master/DroneAudioDataset-master/Binary_Drone_Audio")
PARQUETS = ["wav/train-00038-of-00039.parquet", "wav/train-00003-of-00039.parquet"]
REAL_RECORDINGS = [
    ("POZADI ambient*", "data/recordings/background/bg_ambient_prvni_test.wav"),   # * = in training (anchor)
    ("POZADI ticho*",   "data/recordings/background/bg_ticho_01.wav"),
    ("POZADI rec",      "data/recordings/background/bg_rec_02.wav"),
    ("POZADI tleskani", "data/recordings/background/bg_tlesknuti_03.wav"),
    ("POZADI hudba",    "data/recordings/background/bg_hudba_repro_04.wav"),
    ("DRON tichy",      "data/recordings/playback/playback_bebop_train_01.wav"),
    ("DRON stredni",    "data/recordings/playback/playback_bebop_train_02_hlasite.wav"),
    ("DRON max+clip",   "data/recordings/playback/playback_bebop_train_03_30cm.wav"),
]

rng = np.random.default_rng(SEED)
analyzer = MFCCAnalyzer(sample_rate=FS)


def extract_features(x):
    m = analyzer.extract_features(x)
    return np.concatenate([m.mean(axis=1), m.std(axis=1)]).astype(np.float32)


def to_mono_16k(x, fs):
    if x.ndim > 1:
        x = x[:, 0]
    if fs != FS:
        import librosa
        x = librosa.resample(x.astype(np.float32), orig_sr=fs, target_sr=FS)
    return x.astype(np.float32)


def load_clips():
    """Return (list of float32 arrays, labels). Clip order is deterministic."""
    clips, labels = [], []

    import pyarrow.parquet as pq
    for shard in PARQUETS:
        n = 0
        pf = pq.ParquetFile(shard)
        for batch in pf.iter_batches(batch_size=256, columns=["audio", "label"]):
            d = batch.to_pydict()
            for a, lab in zip(d["audio"], d["label"]):
                if n >= MAX_PARQUET:
                    break
                raw = a["bytes"] if isinstance(a, dict) else a
                try:
                    x, fs = sf.read(io.BytesIO(raw), dtype="float32")
                except Exception:
                    continue
                x = to_mono_16k(x, fs)
                if len(x) < MIN_SAMPLES:
                    continue
                clips.append(x)
                labels.append(int(lab))
                n += 1
            if n >= MAX_PARQUET:
                break
        print(f"  {shard}: {n} clips", flush=True)

    for sub, lab in [("yes_drone", 1), ("unknown", 0)]:
        files = sorted(glob.glob(str(WAV_ROOT / sub / "*.wav")))
        n = 0
        for f in files:
            try:
                x, fs = sf.read(f, dtype="float32")
            except Exception:
                continue
            x = to_mono_16k(x, fs)
            if len(x) < MIN_SAMPLES:
                continue
            clips.append(x)
            labels.append(lab)
            n += 1
        print(f"  {sub}: {n} clips", flush=True)

    return clips, np.array(labels)


def synth_quiet_negatives(n):
    """White/pink noise clips at near-silence levels (training-only)."""
    import scipy.signal
    out = []
    for _ in range(n):
        length = int(rng.uniform(1.0, 2.0) * FS)
        x = rng.standard_normal(length).astype(np.float32)
        if rng.random() < 0.5:
            a = rng.uniform(0.9, 0.995)   # one-pole lowpass ~ pink-ish
            x = scipy.signal.lfilter([1.0 - a], [1.0, -a], x).astype(np.float32)
        rms = 10.0 ** rng.uniform(np.log10(QUIET_RMS_RANGE[0]), np.log10(QUIET_RMS_RANGE[1]))
        x *= rms / (float(np.sqrt((x ** 2).mean())) + 1e-12)
        out.append(x)
    return out


def split_unseen(pattern):
    """Deterministic half split of a sorted file list: even->train, odd->val."""
    files = sorted(glob.glob(pattern))
    return files[0::2], files[1::2]


def load_unseen_train_segments():
    """1 s segments from the train halves of the unseen datasets."""
    segs, labels = [], []
    seg_len = int(SEG_SECONDS * FS)
    for pattern, lab in UNSEEN_TRAIN:
        train_files, _ = split_unseen(pattern)
        n = 0
        for f in train_files:
            x, fs = sf.read(f, dtype="float32")
            x = to_mono_16k(x, fs)
            for s in range(0, len(x) - seg_len + 1, seg_len):
                seg = x[s:s + seg_len]
                rms = float(np.sqrt((seg ** 2).mean()))
                if lab == 1 and rms < SEG_MIN_RMS:
                    continue
                segs.append(seg)
                labels.append(lab)
                n += 1
        print(f"  {pattern}: {len(train_files)} train files -> {n} segments (label {lab})",
              flush=True)
    return segs, labels


def eval_unseen_val(models, squelch, threshold):
    """File-level check on the validation halves (alarm rule: any DRONE window)."""
    print(f"\n=== Unseen VALIDACNI poloviny (squelch {squelch}, threshold {threshold}) ===",
          flush=True)
    for pattern, lab in UNSEEN_TRAIN + [("data/samples/Salford/Calib_*.wav", 0)]:
        _, val_files = split_unseen(pattern)
        if not val_files:
            continue
        cells = []
        for mname, model in models.items():
            ok = 0
            win_drone = win_tot = 0
            for f in val_files:
                x, fs = sf.read(f, dtype="float32")
                x = to_mono_16k(x, fs)
                r = classify(x, analyzer, model, squelch, threshold)
                wins = r["windows"] if r else []
                nd = sum(w["decision"] >= threshold for w in wins)
                win_drone += nd
                win_tot += len(wins)
                if (nd > 0) == (lab == 1):
                    ok += 1
            pct = (100.0 * (win_drone if lab == 1 else win_tot - win_drone) / win_tot
                   if win_tot else float("nan"))
            cells.append(f"{mname}: {ok}/{len(val_files)} soub., {pct:3.0f}% oken")
        name = pattern.split("/")[-1].replace("_*.wav", "")
        print(f"  {pattern.split('/')[2][:8]:8s} {name:11s} (label {lab})  " +
              "   ".join(cells), flush=True)


def model_tuple(clf, scaler):
    return (
        float(clf.intercept_[0]),
        scaler.mean_.astype(np.float32),
        (1.0 / (scaler.scale_ + 1e-8)).astype(np.float32),
        clf.coef_[0].astype(np.float32),
    )


def decisions(model, X):
    bias, mean, inv_std, w = model
    return ((X - mean) * inv_std) @ w + bias


def export_header(path, model):
    bias, mean, inv_std, w = model

    def arr(a):
        return ", ".join(f"{v:.8e}f" for v in a)

    guard = f"SVM_MODEL_DATA_{VERSION.upper()}_H"
    path.write_text(
        f"#ifndef {guard}\n#define {guard}\n\n"
        "// Level-robust linear SVM (gain-augmented retrain, see\n"
        "// src/analysis/train_svm_level_robust.py). Same 26-feature layout as v1.\n\n"
        "#define SVM_NUM_FEATURES 26\n"
        f"#define SVM_BIAS {bias:.8e}f\n\n"
        f"static const float svm_scaler_mean[SVM_NUM_FEATURES] = {{\n    {arr(mean)}\n}};\n\n"
        f"static const float svm_scaler_inv_std[SVM_NUM_FEATURES] = {{\n    {arr(inv_std)}\n}};\n\n"
        f"static const float svm_weights[SVM_NUM_FEATURES] = {{\n    {arr(w)}\n}};\n\n"
        "#endif // SVM_MODEL_DATA_V2_H\n",
        encoding="utf-8",
    )


def eval_real(model, squelch, threshold):
    """Per-file DRONE-window counts on the real recordings."""
    rows = []
    for name, p in REAL_RECORDINGS:
        x, fs = sf.read(p, dtype="float32")
        if x.ndim > 1:
            x = x[:, 0]
        if fs == 48000:
            x = np.ascontiguousarray(x[::3])
        r = classify(x, analyzer, model, squelch, threshold)
        if r is None or not r["windows"]:
            rows.append((name, 0, 0, None))
            continue
        d = np.array([w_["decision"] for w_ in r["windows"]])
        rows.append((name, int((d >= threshold).sum()), len(d), float(d.mean())))
    return rows


def main():
    t0 = time.time()
    print("Loading clips...", flush=True)
    clips, y = load_clips()
    print(f"total {len(clips)} clips ({int((y == 1).sum())} drone / {int((y == 0).sum())} noise), "
          f"{time.time() - t0:.0f} s", flush=True)

    # Split BY CLIP before augmentation (no leakage of a clip across the split).
    from sklearn.model_selection import train_test_split
    idx_train, idx_test = train_test_split(
        np.arange(len(clips)), test_size=0.2, random_state=SEED, stratify=y)

    print("Extracting features (train with augmentation)...", flush=True)
    X_train, y_train = [], []
    for k, i in enumerate(idx_train):
        x = clips[i]
        X_train.append(extract_features(x))
        y_train.append(y[i])
        for _ in range(K_AUG):
            g = 10.0 ** (rng.uniform(*GAIN_DB_RANGE) / 20.0)
            X_train.append(extract_features(x * g))
            y_train.append(y[i])
        if (k + 1) % 1000 == 0:
            print(f"  train {k + 1}/{len(idx_train)}  ({time.time() - t0:.0f} s)", flush=True)

    print(f"Adding {N_QUIET_NEG} synthetic near-silence negatives (train only)...", flush=True)
    for x in synth_quiet_negatives(N_QUIET_NEG):
        X_train.append(extract_features(x))
        y_train.append(0)

    print("Adding real mic-background anchors (train only)...", flush=True)
    n_anchor = 0
    for p in REAL_NEG_FILES:
        x, fs = sf.read(p, dtype="float32")
        if x.ndim > 1:
            x = x[:, 0]
        if fs == 48000:
            x = np.ascontiguousarray(x[::3])
        for s in range(0, len(x) - FS + 1, FS):
            seg = x[s:s + FS]
            for _ in range(REAL_NEG_COPIES):
                g = 10.0 ** (rng.uniform(-REAL_NEG_JITTER_DB, REAL_NEG_JITTER_DB) / 20.0)
                X_train.append(extract_features(seg * g))
                y_train.append(0)
                n_anchor += 1
    print(f"  {n_anchor} anchor samples from {len(REAL_NEG_FILES)} recordings", flush=True)

    print("Adding unseen-dataset train segments (train halves only)...", flush=True)
    u_segs, u_labels = load_unseen_train_segments()
    for seg, lab in zip(u_segs, u_labels):
        X_train.append(extract_features(seg))
        y_train.append(lab)
        for _ in range(K_AUG):
            g = 10.0 ** (rng.uniform(*GAIN_DB_RANGE) / 20.0)
            X_train.append(extract_features(seg * g))
            y_train.append(lab)

    X_train = np.stack(X_train)
    y_train = np.array(y_train)

    print("Extracting features (test, native + robustness gain)...", flush=True)
    g15 = 10.0 ** (ROBUST_TEST_GAIN_DB / 20.0)
    X_test = np.stack([extract_features(clips[i]) for i in idx_test])
    X_test15 = np.stack([extract_features(clips[i] * g15) for i in idx_test])
    y_test = y[idx_test]

    print("Training linear SVM (balanced)...", flush=True)
    from sklearn.preprocessing import StandardScaler
    from sklearn.svm import SVC
    scaler = StandardScaler().fit(X_train)
    clf = SVC(kernel="linear", class_weight="balanced", random_state=SEED)
    clf.fit(scaler.transform(X_train), y_train)

    new_model = model_tuple(clf, scaler)
    models = {"v1": load_svm_model(MODEL_HEADER)}
    v2_header = Path(__file__).parent.parent / "firmware" / "Inc" / "svm_model_data_v2.h"
    if VERSION != "v2" and v2_header.exists():
        models["v2"] = load_svm_model(v2_header)
    models[VERSION] = new_model

    print(f"\n=== Held-out metrics (threshold 0, {len(y_test)} clips) ===", flush=True)
    print("  model | native recall/spec/acc | -15 dB recall/spec/acc", flush=True)
    for label, model in models.items():
        cells = []
        for X in (X_test, X_test15):
            pred = decisions(model, X) >= 0
            rec = float(pred[y_test == 1].mean())
            spec = float((~pred[y_test == 0]).mean())
            acc = float((pred == y_test).mean())
            cells.append(f"{rec:.3f}/{spec:.3f}/{acc:.3f}")
        print(f"  {label:5s} |   {cells[0]}    |   {cells[1]}", flush=True)

    b, m, s, w = new_model
    print(f"\n  new w[0] (mean-c0 term, scaled): {w[0] * 1.0:.3f}  (v1 had 1.707)", flush=True)

    eval_unseen_val(models, 0.005, 0.5)
    eval_unseen_val(models, 0.005, 0.25)

    print("\n=== Real recordings (DRONE windows / total, mean decision) ===", flush=True)
    for sq in (0.003, 0.005):
        for thr in (0.25, 0.5):
            print(f"\n-- squelch {sq}  threshold {thr} --", flush=True)
            for label, model in models.items():
                rows = eval_real(model, sq, thr)
                cells = []
                for name, ndrone, ntot, dmean in rows:
                    cells.append(f"{name.split()[-1]}:{ndrone}/{ntot}" +
                                 (f"({dmean:+.1f})" if dmean is not None else ""))
                print(f"  {label}: " + "  ".join(cells), flush=True)

    import joblib
    Path("models").mkdir(exist_ok=True)
    joblib.dump(clf, f"models/drone_detector_svm_{VERSION}.pkl")
    joblib.dump(scaler, f"models/scaler_{VERSION}.pkl")
    export_header(Path(f"src/firmware/Inc/svm_model_data_{VERSION}.h"), new_model)
    print(f"\nSaved models/drone_detector_svm_{VERSION}.pkl, models/scaler_{VERSION}.pkl, "
          f"src/firmware/Inc/svm_model_data_{VERSION}.h  ({time.time() - t0:.0f} s total)", flush=True)


if __name__ == "__main__":
    main()
