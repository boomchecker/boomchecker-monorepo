"""The board's deterministic fixture signal, reproduced.

src/boomdetect_selftest.c drives the chain from an integer LCG so that every
platform scores the same bits. This is the same generator, so the Python front
end can be compared against tests/vectors/selftest_host.txt without a board.
"""

from __future__ import annotations

import struct
from pathlib import Path

import numpy as np

SELFTEST_INPUT_LEN = 66048
SELFTEST_SEED = 1
_MASK32 = 0xFFFFFFFF


def lcg_signal(n: int = SELFTEST_INPUT_LEN, seed: int = SELFTEST_SEED) -> np.ndarray:
    """int16 samples: `state = state * 1103515245 + 12345`, top 16 bits, quartered."""
    out = np.empty(n, dtype=np.int16)
    state = seed & _MASK32
    for i in range(n):
        state = (state * 1103515245 + 12345) & _MASK32
        v = ((state >> 16) & 0xFFFF) - 32768
        # C integer division truncates toward zero; Python's // floors.
        out[i] = int(v / 4)
    return out


def fnv1a(samples: np.ndarray) -> int:
    """FNV-1a over the little-endian bytes of int16 samples, as the C prints in DSTSIG."""
    h = 2166136261
    for x in np.asarray(samples, dtype=np.int16):
        u = int(x) & 0xFFFF
        h = ((h ^ (u & 0xFF)) * 16777619) & _MASK32
        h = ((h ^ ((u >> 8) & 0xFF)) * 16777619) & _MASK32
    return h


def hex_to_f32(token: str) -> float:
    return struct.unpack("<f", struct.pack("<I", int(token, 16)))[0]


def parse_fixture(path: Path | str) -> dict:
    """Read a DST* fixture into arrays.

    Returns {"mfcc": (frames, 13), "feat": {window: {"mean"|"std"|"dmea"|"cmax": (13,)}},
             "dec": {window: float}, "sig": str}. The C pads the group name to four
    characters ("std "); the key here is the stripped name.
    """
    mfcc: dict[int, list[float]] = {}
    feat: dict[int, dict[str, list[float]]] = {}
    dec: dict[int, float] = {}
    sig = None
    for raw in Path(path).read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line.startswith("DST"):
            continue
        tok = line.split()
        if tok[0] == "DSTMFCC":
            f = int(tok[1].split("=")[1])
            mfcc[f] = [hex_to_f32(t) for t in tok[2:]]
        elif tok[0] == "DSTFEAT":
            w = int(tok[1].split("=")[1])
            feat.setdefault(w, {})[tok[2]] = [hex_to_f32(t) for t in tok[3:]]
        elif tok[0] == "DSTDEC":
            w = int(tok[1].split("=")[1])
            dec[w] = hex_to_f32(tok[2].split("=")[1])
        elif tok[0] == "DSTSIG":
            sig = line
    frames = sorted(mfcc)
    feat_arrays = {
        w: {k: np.asarray(v, dtype=np.float32) for k, v in d.items()} for w, d in feat.items()
    }
    return {
        "mfcc": np.asarray([mfcc[f] for f in frames], dtype=np.float32),
        "feat": feat_arrays,
        "dec": dec,
        "sig": sig,
    }


# --- the layout-4 fixture signal -------------------------------------------
#
# The LCG above is 1.4 s long, and the modulation features need two seconds of
# envelope before they say anything, so layout 4 gets its own signal: the same
# LCG noise, amplitude-modulated in integer arithmetic (AM_PERIOD) so both sides
# see identical int16 samples and the envelope has one line to find.
AM_INPUT_LEN = 240000  # 5 s at 48 kHz: 155 frames, the ring full from frame 61
AM_PERIOD = 250  # samples at 48 kHz -> 192 Hz, inside the 150-250 Hz band
AM_DEPTH_Q10 = 512  # modulation depth 0.5 in Q10


def am_table(period: int = AM_PERIOD, depth_q10: int = AM_DEPTH_Q10) -> np.ndarray:
    """int16 Q10 modulator, one period: round(depth * sin(2 pi k / period))."""
    k = np.arange(period)
    return np.round(depth_q10 * np.sin(2.0 * np.pi * k / period)).astype(np.int16)


def am_lcg_signal(n: int = AM_INPUT_LEN, seed: int = SELFTEST_SEED) -> np.ndarray:
    """lcg_signal() times (1024 + am_table[i mod period]) / 1024, truncated like C."""
    base = lcg_signal(n, seed).astype(np.int64)
    table = am_table().astype(np.int64)
    gain = 1024 + table[np.arange(n) % table.shape[0]]
    prod = base * gain
    # C integer division truncates toward zero; numpy's // floors.
    out = np.sign(prod) * (np.abs(prod) // 1024)
    return out.astype(np.int16)
