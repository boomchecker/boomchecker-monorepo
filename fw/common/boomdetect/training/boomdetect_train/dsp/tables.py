"""Parse the C tables the firmware's MFCC is generated from.

src/mfcc_tables.h records its own provenance (16 kHz, 1024-sample Hamming
window, 20 mel filters over 0..8000 Hz, 13 DCT rows) and the generator that
wrote it is not in the repository. Reading the header is therefore the only way
to get exactly the numbers the board multiplies by - a librosa call with the
"same" parameters lands a few ULP away on every filter edge, and those ULPs are
what a parity test would then report.

The parser is deliberately dumb: it finds `static const <type> name[..] = {..};`
blocks and reads every number inside the braces. Nested braces (the DCT is a
2-D array) flatten and are reshaped from the declared dimensions.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path

import numpy as np

from boomdetect_train.paths import MFCC_TABLES_H

_ARRAY_RE = re.compile(
    r"static\s+const\s+(?P<type>float|u?int(?:8|16|32|64)_t|int)\s+(?P<name>\w+)\s*"
    r"(?P<dims>(?:\[[^\]]*\])+)\s*=\s*\{(?P<body>.*?)\};",
    re.DOTALL,
)
_DEFINE_RE = re.compile(r"^\s*#define\s+(\w+)\s+(-?\d+)\b", re.MULTILINE)
_NUMBER_RE = re.compile(r"[-+]?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?")


def parse_defines(text: str) -> dict[str, int]:
    """Integer `#define`s in `text`, by name."""
    return {m.group(1): int(m.group(2)) for m in _DEFINE_RE.finditer(text)}


def parse_arrays(text: str, defines: dict[str, int] | None = None) -> dict[str, np.ndarray]:
    """Every `static const` array in `text`, shaped from its declared dimensions.

    A dimension may be a literal or a macro from `defines`. Float arrays come
    back as float32 - the type the board holds them in - and integer arrays as
    int64.
    """
    defines = defines if defines is not None else parse_defines(text)
    out: dict[str, np.ndarray] = {}
    for m in _ARRAY_RE.finditer(text):
        dims: list[int] = []
        for d in re.findall(r"\[([^\]]*)\]", m.group("dims")):
            d = d.strip()
            if not d:
                continue
            dims.append(int(d) if d.lstrip("-").isdigit() else defines[d])
        values = [float(v) for v in _NUMBER_RE.findall(m.group("body"))]
        dtype = np.float32 if m.group("type") == "float" else np.int64
        arr = np.asarray(values, dtype=dtype)
        if dims:
            expected = int(np.prod(dims))
            if arr.size != expected:
                raise ValueError(
                    f"{m.group('name')}: declared {dims} = {expected} values, found {arr.size}"
                )
            arr = arr.reshape(dims)
        out[m.group("name")] = arr
    return out


@dataclass(frozen=True)
class MfccTables:
    """The generated tables, as the firmware uses them."""

    window: np.ndarray  # (n_fft,) float32 Hamming
    filter_pos: np.ndarray  # (n_mels,) first FFT bin of each mel filter
    filter_len: np.ndarray  # (n_mels,) bins per filter
    filter_coefs: np.ndarray  # (sum(filter_len),) packed filter weights
    dct: np.ndarray  # (n_mfcc, n_mels) float32 DCT-II, orthonormal

    @property
    def n_fft(self) -> int:
        return int(self.window.shape[0])

    @property
    def n_mels(self) -> int:
        return int(self.filter_pos.shape[0])

    @property
    def n_mfcc(self) -> int:
        return int(self.dct.shape[0])

    @property
    def n_bins(self) -> int:
        """Magnitude bins the filters can reach: DC through Nyquist."""
        return self.n_fft // 2 + 1

    def mel_matrix(self) -> np.ndarray:
        """The packed filters as a dense (n_mels, n_bins) float64 matrix.

        Dense is convenient for vectorised feature extraction; the board never
        materialises this (it dot-products each filter's slice), but the two are
        the same linear map.
        """
        m = np.zeros((self.n_mels, self.n_bins), dtype=np.float64)
        off = 0
        for i in range(self.n_mels):
            pos, n = int(self.filter_pos[i]), int(self.filter_len[i])
            m[i, pos : pos + n] = self.filter_coefs[off : off + n]
            off += n
        return m


@lru_cache(maxsize=4)
def load_tables(path: Path | str | None = None) -> MfccTables:
    """Read the tables from `path` (default: the firmware's src/mfcc_tables.h)."""
    p = Path(path) if path is not None else MFCC_TABLES_H
    text = p.read_text(encoding="utf-8")
    defines = parse_defines(text)
    arrays = parse_arrays(text, defines)
    tables = MfccTables(
        window=arrays["mfcc_window_coefs"],
        filter_pos=arrays["mfcc_filter_pos"],
        filter_len=arrays["mfcc_filter_lengths"],
        filter_coefs=arrays["mfcc_filter_coefs"],
        dct=arrays["mfcc_dct_coefs"],
    )
    if int(tables.filter_len.sum()) != tables.filter_coefs.shape[0]:
        raise ValueError("mfcc_tables.h: filter lengths do not add up to the packed coefficients")
    if defines.get("MFCC_DCT_COLS", tables.n_mels) != tables.n_mels:
        raise ValueError("mfcc_tables.h: DCT columns disagree with the mel filter count")
    return tables
