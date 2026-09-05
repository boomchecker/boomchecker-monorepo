"""The detect defaults quoted by spec.py must match the firmware's detector.h.

PROTOCOL.md is generated from spec.py, but the numbers it quotes are owned by
the firmware (fw/bom-stm32node/Core/Inc/detector.h). The two drifted once (the
contract kept describing a linear SVM with a 0.5 threshold long after the v6
MLP shipped with 7.25), so this test reads the header from the monorepo and
fails when they disagree. It is skipped when the firmware tree is not checked
out next to this package (e.g. an installed wheel).
"""

from __future__ import annotations

import re
from pathlib import Path

import pytest

from stm32node_cli.protocol import spec

DETECTOR_H = (
    Path(__file__).resolve().parents[3] / "bom-stm32node" / "Core" / "Inc" / "detector.h"
)


def _define(text: str, name: str) -> int:
    m = re.search(rf"^\s*#define\s+{name}\s+(-?\d+)\b", text, re.MULTILINE)
    assert m, f"{name} not found in {DETECTOR_H}"
    return int(m.group(1))


@pytest.fixture(scope="module")
def detector_h() -> str:
    if not DETECTOR_H.exists():
        pytest.skip(f"firmware header not available: {DETECTOR_H}")
    return DETECTOR_H.read_text(encoding="utf-8")


def test_detect_defaults_match_firmware(detector_h: str) -> None:
    squelch = _define(detector_h, "DETECTOR_DEFAULT_SQUELCH_MILLI")
    assert spec.DETECT_DEFAULT_SQUELCH_MILLI == squelch
    assert spec.DETECT_DEFAULT_THR_MILLI == _define(detector_h, "DETECTOR_DEFAULT_THR_MILLI")


def test_detect_description_quotes_the_defaults() -> None:
    detect = next(c for c in spec.COMMANDS if c.name == "detect")
    assert f"default {spec.DETECT_DEFAULT_SQUELCH_MILLI}" in detect.description
    assert f"default {spec.DETECT_DEFAULT_THR_MILLI}" in detect.description
    assert "[dbg]" in detect.usage
