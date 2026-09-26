"""Which frames make a window: the two gating policies, side by side.

This is the part of the chain the model was NOT trained against, and the reason
it is a separate module: training slid windows of 14 frames with a hop of 7
over contiguous audio and kept a window if the median frame RMS cleared the
gate; the firmware takes disjoint runs of 14 accepted frames and a frame below
the gate resets the run, so a deployed window can straddle silence. Both are
expressible here so the skew between them can be measured rather than argued.
"""

from __future__ import annotations

from collections.abc import Iterator
from dataclasses import dataclass
from enum import StrEnum

import numpy as np

ACCUM_FRAMES = 14
TRAIN_HOP_FRAMES = 7
DEFAULT_SQUELCH = 0.010


class Gate(StrEnum):
    """How a window is gated (mirrors boomdetect_gate_t)."""

    PER_FRAME = "per_frame"  # the firmware: every frame must clear the gate
    WINDOW_MEDIAN = "window_median"  # training: judged as a whole on the median


@dataclass(frozen=True)
class Window:
    """Indices of the frames that make one classified window."""

    frames: np.ndarray  # (accum,) int frame indices, ascending

    @property
    def start(self) -> int:
        return int(self.frames[0])

    @property
    def end(self) -> int:
        return int(self.frames[-1])


def firmware_windows(
    rms: np.ndarray, squelch: float = DEFAULT_SQUELCH, accum: int = ACCUM_FRAMES
) -> Iterator[Window]:
    """boomdetect_step()'s policy: disjoint runs of `accum` accepted frames.

    A frame below `squelch` resets the accumulator; `squelch` 0 disables the gate.
    """
    run: list[int] = []
    for i, r in enumerate(np.asarray(rms, dtype=np.float32)):
        if squelch > 0.0 and r < squelch:
            run = []
            continue
        run.append(i)
        if len(run) >= accum:
            yield Window(np.asarray(run, dtype=np.int64))
            run = []


def sliding_windows(
    rms: np.ndarray,
    squelch: float | None = None,
    accum: int = ACCUM_FRAMES,
    hop: int = TRAIN_HOP_FRAMES,
) -> Iterator[Window]:
    """The training pipeline's policy: overlapping windows, gated on the median.

    `squelch` None keeps every window; otherwise a window whose median frame RMS
    is below it is dropped as a whole (the frames are still contiguous).
    """
    rms = np.asarray(rms, dtype=np.float32)
    n = rms.shape[0]
    for s in range(0, n - accum + 1, hop):
        idx = np.arange(s, s + accum, dtype=np.int64)
        if squelch is not None and float(np.median(rms[idx])) < squelch:
            continue
        yield Window(idx)


def windows(
    rms: np.ndarray,
    gate: Gate = Gate.PER_FRAME,
    squelch: float | None = DEFAULT_SQUELCH,
    accum: int = ACCUM_FRAMES,
    hop: int | None = None,
) -> list[Window]:
    """Dispatch on `gate`. `hop` only matters for the sliding policy."""
    if gate == Gate.PER_FRAME:
        return list(firmware_windows(rms, 0.0 if squelch is None else squelch, accum))
    return list(sliding_windows(rms, squelch, accum, hop if hop is not None else TRAIN_HOP_FRAMES))


def window_seconds(accum: int = ACCUM_FRAMES, hop_samples: int = 512, sr: int = 16000) -> float:
    """Audio covered by one window of contiguous frames (about 0.448 s)."""
    return ((accum - 1) * hop_samples + 1024) / sr
