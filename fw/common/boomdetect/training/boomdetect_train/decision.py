"""The decision layer above the classifier: K-of-N with hysteresis.

One window is 448 ms of audio and one logit; an alarm should be a property of
seconds, not of one window. The rule mirrors what goes into detect_service.c:
the alarm turns ON when at least `k_on` of the last `n` classified windows were
called drone, and OFF again when fewer than `k_off` of the last `n` were. With
k_off < k_on the state does not flicker on a decision that hovers around the
threshold. Squelched frames produce no window and so do not move the history;
a window that never closes cannot raise an alarm, which is the same on the
board.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class KofN:
    n: int = 4
    k_on: int = 2
    k_off: int = 1

    def __post_init__(self) -> None:
        if not (1 <= self.k_off <= self.k_on <= self.n):
            raise ValueError("need 1 <= k_off <= k_on <= n")


def alarm_states(is_drone: np.ndarray, rule: KofN) -> np.ndarray:
    """Alarm state after each window, from the per-window drone/noise calls."""
    calls = np.asarray(is_drone, dtype=bool)
    out = np.zeros(calls.shape[0], dtype=bool)
    hist: list[bool] = []
    on = False
    for i, c in enumerate(calls):
        hist.append(bool(c))
        if len(hist) > rule.n:
            hist.pop(0)
        hits = sum(hist)
        if not on and hits >= rule.k_on:
            on = True
        elif on and hits < rule.k_off:
            on = False
        out[i] = on
    return out


def alarm_onsets(states: np.ndarray) -> int:
    """How many times the alarm went from OFF to ON."""
    s = np.asarray(states, dtype=bool)
    if s.shape[0] == 0:
        return 0
    prev = np.concatenate([[False], s[:-1]])
    return int((s & ~prev).sum())


def clip_alarmed(decisions: np.ndarray, threshold: float, rule: KofN | None) -> bool:
    """File-level verdict: did the alarm ever turn on (or, without a rule, did any window fire)."""
    d = np.asarray(decisions, dtype=np.float32)
    calls = d >= threshold
    if rule is None:
        return bool(calls.any())
    return bool(alarm_states(calls, rule).any())
