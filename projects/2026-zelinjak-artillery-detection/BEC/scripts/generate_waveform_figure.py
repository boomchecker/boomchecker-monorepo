#!/usr/bin/env python3
"""Generate the consolidated waveform figure for the article (Figs 1+2 merged).

Addresses reviewer comments R2#4 (normalized amplitude is unitless -- no empty unit
symbol on the axis label) and R4-minor (Figures 1 and 2 are near-duplicative --
consolidate). Panel (a) shows the full 1 s launch recording, panel (b) the 60 ms
analysis window extracted exactly as in the processing pipeline (ml/utils.py:
peak detection in the first 20 % of the clip, 30 % of the window before the peak,
70 % after).

Reference recording: BEC/scripts/reference_launch_ch1.wav (channel 1 of launch event
0005_0697s_shot_036 from the four-microphone campaign). The signal is loaded through
the same front end as the pipeline (librosa, resampled to 22.05 kHz) and normalized
to [-1, 1] for display.

Output: BEC/article/figs/sec2_waveform_combined.png (300 dpi).
"""

from __future__ import annotations

import sys
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as mticker
import numpy as np

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "ml"))

from utils import extract_window, find_peak, load_signal  # noqa: E402

WAV_PATH = SCRIPT_DIR / "reference_launch_ch1.wav"
OUTPUT_PATH = SCRIPT_DIR.parent / "article" / "figs" / "sec2_waveform_combined.png"

WINDOW_MS = 60
BEFORE_FRAC = 0.3  # must match ml/utils.py extract_window (30 % before / 70 % after peak)


def main() -> None:
    signal, sr = load_signal(str(WAV_PATH))
    peak = np.max(np.abs(signal))
    if peak > 0:
        signal = signal / peak

    peak_index = find_peak(signal)
    window = extract_window(signal, sr, peak_index)
    window_samples = int(sr * WINDOW_MS / 1000)
    window_start = max(0, peak_index - (window_samples - int(window_samples * (1 - BEFORE_FRAC))))

    plt.rcParams.update({
        "font.size": 22,
        "font.family": "serif",
        "font.serif": ["Nimbus Roman", "Liberation Serif", "Times New Roman", "STIXGeneral"],
        "mathtext.fontset": "stix",
    })
    fig, axes = plt.subplots(2, 1, figsize=(10, 8), constrained_layout=True)
    fig.supylabel("Normalized amplitude", fontsize=28, fontweight="bold")

    time_full = np.arange(len(signal)) / sr
    ax = axes[0]
    ax.plot(time_full, signal, color="blue", alpha=0.7, linewidth=2)
    ax.set_xlabel("Time (s)", fontsize=24, fontweight="bold")
    ax.set_xlim(0, time_full[-1])
    ax.set_ylim(-1, 1)
    ax.xaxis.set_major_locator(mticker.MultipleLocator(0.2))
    ax.xaxis.set_major_formatter(mticker.FormatStrFormatter("%.1f"))
    ax.set_title("(a)", fontsize=24, loc="left")

    time_win = np.arange(len(window)) / sr * 1000  # ms
    ax = axes[1]
    ax.plot(time_win, window / (np.max(np.abs(window)) or 1), color="blue", alpha=0.7, linewidth=2.5)
    ax.set_xlabel("Time (ms)", fontsize=24, fontweight="bold")
    ax.set_xlim(0, WINDOW_MS)
    ax.set_ylim(-1, 1)
    ax.xaxis.set_major_locator(mticker.MultipleLocator(10))
    ax.xaxis.set_major_formatter(mticker.FormatStrFormatter("%.0f"))
    ax.set_title("(b)", fontsize=24, loc="left")

    for ax in axes:
        ax.tick_params(axis="both", which="major", labelsize=22, pad=6)
        ax.yaxis.set_major_locator(mticker.MultipleLocator(0.5))
        ax.yaxis.set_major_formatter(mticker.FormatStrFormatter("%.1f"))
        ax.yaxis.set_minor_locator(mticker.MultipleLocator(0.25))
        ax.grid(True, which="major", alpha=0.4)
        ax.grid(True, which="minor", linestyle="-", linewidth=0.5, alpha=0.7)

    axes[0].xaxis.set_minor_locator(mticker.MultipleLocator(0.1))
    axes[1].xaxis.set_minor_locator(mticker.MultipleLocator(5))

    OUTPUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(OUTPUT_PATH, dpi=300)
    print(f"Peak at {peak_index / sr * 1000:.1f} ms, window {window_start / sr * 1000:.0f}"
          f"--{(window_start + window_samples) / sr * 1000:.0f} ms")
    print(f"Saved {OUTPUT_PATH}")


if __name__ == "__main__":
    main()
