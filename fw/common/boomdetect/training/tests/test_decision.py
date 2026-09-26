"""K-of-N with hysteresis, and the file-level verdicts built on it."""

from __future__ import annotations

import numpy as np
import pytest

from boomdetect_train.decision import KofN, alarm_onsets, alarm_states, clip_alarmed


def test_rule_validation():
    KofN(n=4, k_on=2, k_off=1)
    with pytest.raises(ValueError):
        KofN(n=2, k_on=3, k_off=1)
    with pytest.raises(ValueError):
        KofN(n=4, k_on=1, k_off=2)


def test_single_window_does_not_alarm_with_k_on_two():
    rule = KofN(n=4, k_on=2, k_off=1)
    calls = np.array([0, 1, 0, 0, 0, 1, 0, 0], dtype=bool)
    assert not alarm_states(calls, rule).any()


def test_two_of_four_alarms_and_hysteresis_holds():
    rule = KofN(n=4, k_on=2, k_off=1)
    #        0  1  2  3  4  5  6  7  8
    calls = [0, 1, 1, 0, 0, 0, 0, 0, 0]
    states = alarm_states(np.array(calls, dtype=bool), rule)
    # ON at window 2 (two hits in the last four); the last four windows still
    # contain a hit through window 5 (windows 2..5 hold [1,0,0,0]); at window 6
    # the history is [0,0,0,0] and the alarm drops.
    assert states.tolist() == [0, 0, 1, 1, 1, 1, 0, 0, 0]
    assert alarm_onsets(states) == 1


def test_hysteresis_prevents_flicker():
    rule = KofN(n=4, k_on=2, k_off=1)
    calls = np.array([1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0], dtype=bool)
    states = alarm_states(calls, rule)
    assert alarm_onsets(states) == 1
    assert states[1:9].all()
    assert not states[-1]


def test_clip_alarmed_with_and_without_rule():
    d = np.array([-3.0, 0.2, 0.4, -1.0], dtype=np.float32)
    assert clip_alarmed(d, 0.0, None)
    assert clip_alarmed(d, 0.0, KofN(n=4, k_on=2, k_off=1))
    assert not clip_alarmed(d, 0.3, KofN(n=4, k_on=2, k_off=1))
    assert clip_alarmed(d, 0.3, None)
    assert not clip_alarmed(np.empty(0, dtype=np.float32), 0.0, KofN())
