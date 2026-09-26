"""The detect defaults quoted by spec.py must match the firmware.

PROTOCOL.md is generated from spec.py, but the numbers it quotes are owned by
the firmware: the RMS squelch by App/detect/detect_service.h, the default model
by the first entry of fw/common/boomdetect/src/classifier_registry.c and every
decision threshold by its model table (fw/common/boomdetect/models/
model_<name>.c). The two drifted once (the contract kept describing a
linear SVM with a 0.5 threshold long after the v6 MLP shipped), so this test
reads the firmware sources from the monorepo and fails when they disagree. It
is skipped when the firmware tree is not checked out next to this package
(e.g. an installed wheel).
"""

from __future__ import annotations

import re
from pathlib import Path

import pytest

from stm32node_cli.protocol import spec

FW_ROOT = Path(__file__).resolve().parents[3]
DETECT_SERVICE_H = FW_ROOT / "bom-stm32node" / "App" / "detect" / "detect_service.h"
MODELS_DIR = FW_ROOT / "common" / "boomdetect" / "models"
MODEL_MLP_V6_C = MODELS_DIR / "model_mlp_v6.c"
REGISTRY_C = FW_ROOT / "common" / "boomdetect" / "src" / "classifier_registry.c"


def _read(path: Path) -> str:
    if not path.exists():
        pytest.skip(f"firmware source not available: {path}")
    return path.read_text(encoding="utf-8")


def _define(text: str, name: str, path: Path) -> int:
    m = re.search(rf"^\s*#define\s+{name}\s+(-?\d+)\b", text, re.MULTILINE)
    assert m, f"{name} not found in {path}"
    return int(m.group(1))


def test_detect_squelch_default_matches_firmware() -> None:
    text = _read(DETECT_SERVICE_H)
    squelch = _define(text, "DETECT_DEFAULT_SQUELCH_MILLI", DETECT_SERVICE_H)
    assert spec.DETECT_DEFAULT_SQUELCH_MILLI == squelch


def _thr_milli(path: Path) -> int:
    text = _read(path)
    m = re.search(r"\.default_thr_milli\s*=\s*(-?\d+)\b", text)
    assert m, f"default_thr_milli not found in {path}"
    return int(m.group(1))


def test_detect_thr_default_matches_mlp_v6_model() -> None:
    assert spec.DETECT_MLP_V6_DEFAULT_THR_MILLI == _thr_milli(MODEL_MLP_V6_C)


def test_default_model_is_the_registrys_first_entry_with_its_threshold() -> None:
    text = _read(REGISTRY_C)
    body = text[text.index("s_models[]") :]
    first = re.search(r"&classifier_(\w+)\s*,", body)
    assert first, f"no registry entry found in {REGISTRY_C}"
    assert spec.DETECT_DEFAULT_MODEL == first.group(1)
    model_c = MODELS_DIR / f"model_{spec.DETECT_DEFAULT_MODEL}.c"
    assert spec.DETECT_DEFAULT_MODEL_THR_MILLI == _thr_milli(model_c)


def test_detect_max_seconds_matches_firmware() -> None:
    text = _read(DETECT_SERVICE_H)
    assert spec.DETECT_MAX_SECONDS == _define(text, "DETECT_MAX_SECONDS", DETECT_SERVICE_H)
    detect = next(c for c in spec.COMMANDS if c.name == "detect")
    assert f"1..{spec.DETECT_MAX_SECONDS}" in detect.description
    assert "0 runs until" in detect.description


def test_detect_alarm_rule_matches_firmware() -> None:
    text = _read(DETECT_SERVICE_H)
    assert spec.DETECT_ALARM_N == _define(text, "DETECT_ALARM_N", DETECT_SERVICE_H)
    assert spec.DETECT_ALARM_K_ON == _define(text, "DETECT_ALARM_K_ON", DETECT_SERVICE_H)
    assert spec.DETECT_ALARM_K_OFF == _define(text, "DETECT_ALARM_K_OFF", DETECT_SERVICE_H)
    assert 1 <= spec.DETECT_ALARM_K_OFF <= spec.DETECT_ALARM_K_ON <= spec.DETECT_ALARM_N


def test_detect_description_quotes_the_defaults() -> None:
    detect = next(c for c in spec.COMMANDS if c.name == "detect")
    assert f"default {spec.DETECT_DEFAULT_SQUELCH_MILLI}" in detect.description
    assert str(spec.DETECT_DEFAULT_MODEL_THR_MILLI) in detect.description
    assert spec.DETECT_DEFAULT_MODEL in detect.description
    assert str(spec.DETECT_MLP_V6_DEFAULT_THR_MILLI) in detect.description
    assert "[dbg]" in detect.usage
    assert f"{spec.DETECT_ALARM_K_ON} of the last {spec.DETECT_ALARM_N}" in detect.response
    assert "ALM t=" in detect.response
    assert "alarms=<n>" in detect.response
