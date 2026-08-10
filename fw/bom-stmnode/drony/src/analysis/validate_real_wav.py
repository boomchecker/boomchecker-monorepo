"""Run the firmware detection pipeline on recorded WAV files.

Mirrors src/firmware/Src/audio_sai_handler.c step by step so that WAVs recorded
from the real microphone chain (stm32node-cli `record` -> 48 kHz/16-bit mono)
can be evaluated on the PC before porting the detector to the MCU:

  48 kHz WAV -> [decimate by 3] -> 16 kHz float
  -> frames of 1024, hop 512
  -> per-frame RMS squelch (< threshold resets accumulation)
  -> MFCC (mfcc_analyzer.MFCCAnalyzer, parity-verified against CMSIS-DSP)
  -> 14 consecutive accepted frames -> mean+std aggregation (26 features)
  -> StandardScaler + linear SVM (weights parsed from svm_model_data.h)
  -> decision value; DRONE if decision >= threshold (firmware: 0.5)

Resample modes for 48 kHz input:
  decimate3 (default) -- take every 3rd sample; matches audio_sai_handler.c.
      Valid because the bom-stm32node mic chain already band-limits to 8 kHz
      (101-tap FIR in pdm_pcm.c). Verified: preserves the decision value of
      native-16k audio to ~0.02.
  fir15 -- the 15-tap decimation FIR that audio_sai_handler.c used before
      2026-08-06. MEASURED HARMFUL: its passband droop shifts a +1.14 drone
      decision to -0.22 (misclassification). Kept for comparison only.
  librosa -- high-quality polyphase resampling, any input rate.

Usage:
  python validate_real_wav.py rec1.wav [rec2.wav ...]
  python validate_real_wav.py rec.wav --gain-db 12      # test level sensitivity
  python validate_real_wav.py rec.wav --no-squelch      # ignore the RMS gate
  python validate_real_wav.py rec.wav --threshold 0.3   # more sensitive operating point

Dependencies: numpy, scipy, librosa, soundfile (+ matplotlib pulled in by
mfcc_analyzer). Same environment as the other src/analysis scripts.
"""

import argparse
import glob
import re
import sys
from pathlib import Path

import numpy as np
import scipy.signal
import soundfile as sf

sys.path.insert(0, str(Path(__file__).parent))
from mfcc_analyzer import MFCCAnalyzer

# --- Constants mirroring the firmware (dsp_config.h / audio_sai_handler.c) ---
TARGET_FS = 16000
WINDOW_SIZE = 1024
HOP = 512
NUM_MFCC = 13
ACCUM_FRAMES = 14           # MAX_ACCUMULATED_FRAMES
RMS_SQUELCH = 0.010         # RMS_SQUELCH_THRESHOLD
SVM_THRESHOLD = 0.5         # svm_predict: decision >= 0.5

# audio_sai_handler.c fir_coeffs (48 kHz -> 16 kHz anti-alias, decimate by 3)
FIRMWARE_FIR = np.array([
    -0.0034, -0.0076, 0.0000, 0.0354, 0.0910, 0.1478, 0.1874, 0.1874,
     0.1478,  0.0910, 0.0354, 0.0000, -0.0076, -0.0034, 0.0000,
], dtype=np.float32)

MODEL_HEADER = Path(__file__).parent.parent / "firmware" / "Inc" / "svm_model_data.h"


def load_svm_model(header_path: Path):
    """Parse bias, scaler and weights from the generated C header (the exact
    numbers the firmware uses -- no pickle/sklearn version issues)."""
    text = header_path.read_text(encoding="utf-8")
    bias = float(re.search(r"#define\s+SVM_BIAS\s+([-+0-9.eE]+)f?", text).group(1))

    def parse_array(name):
        block = re.search(name + r"\[[^]]*\]\s*=\s*\{([^}]*)\}", text).group(1)
        return np.array([float(v) for v in re.findall(r"([-+0-9.eE]+)f", block)],
                        dtype=np.float32)

    mean = parse_array("svm_scaler_mean")
    inv_std = parse_array("svm_scaler_inv_std")
    weights = parse_array("svm_weights")
    assert len(mean) == len(inv_std) == len(weights) == 2 * NUM_MFCC
    return bias, mean, inv_std, weights


def load_audio(path: Path, gain_db: float, resample_mode: str):
    """Load WAV as float32 in [-1, 1] (int16/32768 like the firmware),
    downmix to mono, optionally apply a gain trim, and bring it to 16 kHz."""
    audio, fs = sf.read(path, dtype="float32", always_2d=True)
    if audio.shape[1] > 1:
        print(f"  ! {audio.shape[1]} channels, using channel 0")
    audio = audio[:, 0]

    if gain_db != 0.0:
        audio = audio * (10.0 ** (gain_db / 20.0))

    if resample_mode == "auto":
        if fs == TARGET_FS:
            resample_mode = "none"
        elif fs % TARGET_FS == 0 and fs // TARGET_FS == 3:
            resample_mode = "decimate3"
        else:
            resample_mode = "librosa"

    if resample_mode == "none":
        if fs != TARGET_FS:
            raise SystemExit(f"{path}: {fs} Hz, expected {TARGET_FS} (use --resample)")
    elif resample_mode == "decimate3":
        if fs != 3 * TARGET_FS:
            raise SystemExit(f"{path}: decimate3 needs 48000 Hz input, got {fs}")
        audio = audio[::3]
    elif resample_mode == "fir15":
        if fs != 3 * TARGET_FS:
            raise SystemExit(f"{path}: fir15 resample needs 48000 Hz input, got {fs}")
        # arm_fir_decimate_f32 equivalent: causal FIR, keep every 3rd output
        # (output phase may differ from the MCU by <1 sample; irrelevant for MFCC)
        audio = scipy.signal.lfilter(FIRMWARE_FIR, 1.0, audio)[::3].astype(np.float32)
    elif resample_mode == "librosa":
        import librosa
        audio = librosa.resample(audio, orig_sr=fs, target_sr=TARGET_FS)
    else:
        raise SystemExit(f"unknown resample mode: {resample_mode}")

    return audio.astype(np.float32), fs, resample_mode


def classify(audio, analyzer, model, squelch, threshold):
    """Replicate audio_sai_pipeline_poll(): per-frame squelch with accumulation
    reset, decision after every run of 14 accepted frames.

    `model` is either the linear-SVM 4-tuple (bias, mean, inv_std, weights)
    or any callable mapping the 26-feature vector to a decision value
    (e.g. the v4 MLP logit)."""
    if callable(model):
        score = model
    else:
        bias, mean, inv_std, weights = model

        def score(feats):
            return np.dot((feats - mean) * inv_std, weights) + bias
    n_frames = 1 + (len(audio) - WINDOW_SIZE) // HOP if len(audio) >= WINDOW_SIZE else 0
    if n_frames == 0:
        return None

    # All MFCC frames at once == per-frame CMSIS processing (center=False framing)
    mfccs = analyzer.extract_features(audio[: (n_frames - 1) * HOP + WINDOW_SIZE])
    assert mfccs.shape[1] == n_frames, (mfccs.shape, n_frames)

    frames = np.lib.stride_tricks.sliding_window_view(audio, WINDOW_SIZE)[::HOP][:n_frames]
    frame_rms = np.sqrt(np.mean(frames.astype(np.float64) ** 2, axis=1))

    windows = []
    accum, start_frame = [], None
    squelched = 0
    for i in range(n_frames):
        if squelch is not None and frame_rms[i] < squelch:
            accum, start_frame = [], None
            squelched += 1
            continue
        if start_frame is None:
            start_frame = i
        accum.append(mfccs[:, i])
        if len(accum) >= ACCUM_FRAMES:
            block = np.stack(accum, axis=0)                    # (14, 13)
            feats = np.concatenate([block.mean(axis=0), block.std(axis=0)])  # ddof=0
            decision = float(score(feats))
            windows.append({
                "t0": start_frame * HOP / TARGET_FS,
                "t1": (i * HOP + WINDOW_SIZE) / TARGET_FS,
                "decision": decision,
                "drone": decision >= threshold,
            })
            accum, start_frame = [], None

    return {
        "n_frames": n_frames,
        "squelched": squelched,
        "rms_min": float(frame_rms.min()),
        "rms_med": float(np.median(frame_rms)),
        "rms_max": float(frame_rms.max()),
        "peak": float(np.max(np.abs(audio))) if len(audio) else 0.0,
        "windows": windows,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("wavs", nargs="+", type=Path)
    ap.add_argument("--gain-db", type=float, default=0.0,
                    help="gain trim applied before processing (level-sensitivity tests)")
    ap.add_argument("--squelch", type=float, default=RMS_SQUELCH,
                    help=f"RMS squelch threshold (firmware: {RMS_SQUELCH})")
    ap.add_argument("--no-squelch", action="store_true", help="disable the RMS gate")
    ap.add_argument("--threshold", type=float, default=SVM_THRESHOLD,
                    help=f"SVM decision threshold (firmware: {SVM_THRESHOLD})")
    ap.add_argument("--resample", choices=["auto", "none", "decimate3", "fir15", "librosa"],
                    default="auto", help="how to get to 16 kHz (default: auto)")
    args = ap.parse_args()

    # Expand glob patterns ourselves -- cmd/PowerShell do not do it for us.
    wavs = []
    for p in args.wavs:
        matches = sorted(glob.glob(str(p))) if any(ch in str(p) for ch in "*?[") else [p]
        if not matches:
            raise SystemExit(f"no files match: {p}")
        wavs.extend(Path(m) for m in matches)

    model = load_svm_model(MODEL_HEADER)
    analyzer = MFCCAnalyzer(sample_rate=TARGET_FS, n_mfcc=NUM_MFCC,
                            n_fft=WINDOW_SIZE, hop_length=HOP)
    squelch = None if args.no_squelch else args.squelch

    for path in wavs:
        audio, orig_fs, mode = load_audio(path, args.gain_db, args.resample)
        print(f"\n=== {path.name} ===")
        print(f"  input {orig_fs} Hz -> {TARGET_FS} Hz ({mode}), "
              f"{len(audio) / TARGET_FS:.2f} s, gain {args.gain_db:+.1f} dB")

        r = classify(audio, analyzer, model, squelch, args.threshold)
        if r is None:
            print("  ! too short (< 1024 samples), skipped")
            continue

        print(f"  frames: {r['n_frames']}  squelched: {r['squelched']} "
              f"({100.0 * r['squelched'] / r['n_frames']:.0f}%)  "
              f"frame RMS min/med/max: {r['rms_min']:.4f}/{r['rms_med']:.4f}/{r['rms_max']:.4f}  "
              f"peak: {r['peak']:.3f}")
        if r["rms_med"] < RMS_SQUELCH and squelch is not None:
            print(f"  ! median RMS below the {RMS_SQUELCH} squelch -- signal very quiet; "
                  f"try --gain-db or --no-squelch to see raw decisions")

        if not r["windows"]:
            print("  no complete 14-frame windows (squelch kept resetting or file too short)")
        for w in r["windows"]:
            verdict = "DRONE" if w["drone"] else "noise"
            print(f"    {w['t0']:7.2f}-{w['t1']:6.2f} s  decision {w['decision']:+7.3f}  {verdict}")

        if r["windows"]:
            d = np.array([w["decision"] for w in r["windows"]])
            n_drone = int(sum(w["drone"] for w in r["windows"]))
            print(f"  summary: {n_drone}/{len(d)} windows DRONE, "
                  f"decision mean {d.mean():+.3f}, max {d.max():+.3f}")


if __name__ == "__main__":
    main()
