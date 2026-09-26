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
SELFTEST_MFCC_FRAMES = 3
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


def f32_to_hex(value: float) -> str:
    return f"{struct.unpack('<I', struct.pack('<f', float(np.float32(value))))[0]:08X}"


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
