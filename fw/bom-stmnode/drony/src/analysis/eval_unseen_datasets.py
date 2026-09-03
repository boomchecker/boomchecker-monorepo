"""Evaluate v1 and v2 SVM models on unseen public datasets.

Runs the firmware-mimicking pipeline (frames 1024/512 @16 kHz, RMS squelch,
14-frame windows, linear SVM) over datasets the models were NOT trained on:

  data/samples/Halmstad/  - Drone-detection-dataset (Svanström et al.):
                            DRONE_* / BACKGROUND_* / HELICOPTER_*, 10 s each
  data/samples/Salford/   - DroneNoise DB flyovers (Ed_*, drone) and
                            calibration recordings (Calib_*, non-drone)

File-level rule (alarm semantics): a drone file counts as correct if at least
one window fires DRONE; a non-drone file counts as correct if NO window fires.

Run from the drony root:  .venv/Scripts/python src/analysis/eval_unseen_datasets.py
"""

import glob
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

sys.path.insert(0, str(Path(__file__).parent))
from validate_real_wav import MODEL_HEADER, classify, load_svm_model
from mfcc_analyzer import MFCCAnalyzer

FS = 16000
SQUELCH = 0.005
THRESHOLD = 0.5

MODEL_HEADER_V2 = Path(__file__).parent.parent / "firmware" / "Inc" / "svm_model_data_v2.h"

GROUPS = [
    # (dataset, group label, glob pattern, is_drone)
    ("Halmstad", "DRONE",      "data/samples/Halmstad/DRONE_*.wav",      True),
    ("Halmstad", "BACKGROUND", "data/samples/Halmstad/BACKGROUND_*.wav", False),
    ("Halmstad", "HELICOPTER", "data/samples/Halmstad/HELICOPTER_*.wav", False),
    ("Salford",  "DRONE (prelety)", "data/samples/Salford/Ed_*.wav",     True),
    ("Salford",  "KALIBRACE",  "data/samples/Salford/Calib_*.wav",       False),
]


def load_16k(path):
    x, fs = sf.read(path, dtype="float32")
    if x.ndim > 1:
        x = x[:, 0]
    if fs != FS:
        import librosa
        x = librosa.resample(x, orig_sr=fs, target_sr=FS)
    return x.astype(np.float32)


def main():
    analyzer = MFCCAnalyzer(sample_rate=FS)
    models = {
        "v1": load_svm_model(MODEL_HEADER),
        "v2": load_svm_model(MODEL_HEADER_V2),
    }

    print(f"operating point: squelch {SQUELCH}, threshold {THRESHOLD}\n", flush=True)
    header = (f"{'dataset':9s} {'skupina':16s} {'soub.':>5s} "
              f"{'v1 soubory OK':>14s} {'v1 okna':>12s} {'v1 dec':>7s} "
              f"{'v2 soubory OK':>14s} {'v2 okna':>12s} {'v2 dec':>7s}")
    print(header, flush=True)

    for dataset, group, pattern, is_drone in GROUPS:
        files = sorted(glob.glob(pattern))
        if not files:
            print(f"{dataset:9s} {group:16s}  ZADNE SOUBORY ({pattern})", flush=True)
            continue

        stats = {m: {"files_ok": 0, "win_drone": 0, "win_total": 0, "dec": []}
                 for m in models}
        no_windows = 0
        for f in files:
            audio = load_16k(f)
            for mname, model in models.items():
                r = classify(audio, analyzer, model, SQUELCH, THRESHOLD)
                wins = r["windows"] if r else []
                ndrone = sum(w["decision"] >= THRESHOLD for w in wins)
                stats[mname]["win_drone"] += ndrone
                stats[mname]["win_total"] += len(wins)
                stats[mname]["dec"].extend(w["decision"] for w in wins)
                detected = ndrone > 0
                if detected == is_drone:
                    stats[mname]["files_ok"] += 1
                if mname == "v1" and not wins:
                    no_windows += 1

        cells = []
        for m in ("v1", "v2"):
            s = stats[m]
            if is_drone:
                win_ok, win_tot = s["win_drone"], s["win_total"]
            else:
                win_ok, win_tot = s["win_total"] - s["win_drone"], s["win_total"]
            pct = 100.0 * win_ok / win_tot if win_tot else float("nan")
            dmean = np.mean(s["dec"]) if s["dec"] else float("nan")
            cells.append(f"{s['files_ok']:3d}/{len(files):<3d} {win_ok:5d}/{win_tot:<5d} ({pct:3.0f}%) {dmean:+6.2f}")
        note = f"  [{no_windows} souboru bez oken]" if no_windows else ""
        print(f"{dataset:9s} {group:16s} {len(files):5d}   " + "   ".join(cells) + note, flush=True)


if __name__ == "__main__":
    main()
