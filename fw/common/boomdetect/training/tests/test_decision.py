"""K-of-N with hysteresis, and the file-level verdicts built on it."""

from __future__ import annotations

import numpy as np
import pytest

from boomdetect_train.decision import (
    KofN,
    MeanN,
    alarm_onsets,
    alarm_states,
    alarm_states_for,
    alarm_states_mean,
    clip_alarmed,
    parse_rule,
)


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


def test_mean_rule_integrates():
    # Mirrored by scenario_mean_rule_integrates in tests/alarm_test.c.
    rule = MeanN(n=4)
    #      0     1    2    3     4     5     6
    rel = [-1.0, 1.0, 1.0, -1.0, -1.0, -1.0, -1.0]
    states = alarm_states_mean(np.array(rel, dtype=np.float32), rule)
    # means over the last four, zero padded: -.25, 0, .25, 0, 0, -.5, -1
    assert states.tolist() == [0, 1, 1, 1, 1, 0, 0]
    assert alarm_onsets(states) == 1
    # One strong window carries three weak ones; the vote sees one hit.
    weak = np.array([3.0, -0.5, -0.5, -0.5], dtype=np.float32)
    assert alarm_states_mean(weak, rule).all()
    assert not alarm_states(weak >= 0, KofN(n=4, k_on=2, k_off=1)).any()
    assert alarm_states_mean(np.empty(0, dtype=np.float32), rule).shape == (0,)


def test_rules_dispatch_on_raw_decisions():
    d = np.array([2.0, 8.0, 4.5, 4.5], dtype=np.float32)  # threshold 5: calls 0 1 0 0
    assert not clip_alarmed(d, 5.0, KofN(n=4, k_on=2, k_off=1))
    # relative -3, 3, -.5, -.5: means -.75, 0, -.125, -.25 -> on at the second window only
    assert clip_alarmed(d, 5.0, MeanN(n=4))
    assert alarm_states_for(d, 5.0, MeanN(n=4)).tolist() == [0, 1, 0, 0]
    with pytest.raises(TypeError):
        alarm_states(d >= 5.0, MeanN(n=4))


def test_parse_rule_and_validation():
    assert parse_rule("2of4") == KofN(n=4, k_on=2, k_off=1)
    assert parse_rule("3of8") == KofN(n=8, k_on=3, k_off=2)
    assert parse_rule("1of4") == KofN(n=4, k_on=1, k_off=1)
    assert parse_rule("mean4") == MeanN(n=4)
    assert parse_rule("MEAN8") == MeanN(n=8)
    assert parse_rule("none") is None and parse_rule(None) is None
    assert str(parse_rule("2of4")) == "2of4" and str(parse_rule("mean4")) == "mean4"
    with pytest.raises(ValueError):
        MeanN(n=0)
    with pytest.raises(ValueError):
        MeanN(n=33)
    with pytest.raises(ValueError):
        KofN(n=33, k_on=2, k_off=1)
