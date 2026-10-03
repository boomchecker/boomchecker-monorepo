"""The decision layer above the classifier: K-of-N with hysteresis, or a mean.

One window is 448 ms of audio and one logit; an alarm should be a property of
seconds, not of one window. Two rules, mirroring fw/common/boomdetect/src/
boomdetect_alarm.c:

* `KofN` (the vote, the board's default): the alarm turns ON when at least
  `k_on` of the last `n` classified windows were called drone, and OFF again
  when fewer than `k_off` of the last `n` were. With k_off < k_on the state
  does not flicker on a decision that hovers around the threshold.
* `MeanN` (soft integration): the alarm is ON while the mean of the last `n`
  relative decisions - decision minus threshold - is >= 0, windows before the
  first counting as 0. A run of windows just below the threshold never alarms;
  one window well above it carries a few weak ones.

Squelched frames produce no window and so do not move the history; a window
that never closes cannot raise an alarm, which is the same on the board.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np

MAX_N = 32  # the board's history width (BOOMDETECT_ALARM_MAX_N)


@dataclass(frozen=True)
class KofN:
    n: int = 4
    k_on: int = 2
    k_off: int = 1

    def __post_init__(self) -> None:
        if not (1 <= self.k_off <= self.k_on <= self.n <= MAX_N):
            raise ValueError(f"need 1 <= k_off <= k_on <= n <= {MAX_N}")

    def __str__(self) -> str:
        return f"{self.k_on}of{self.n}"


@dataclass(frozen=True)
class MeanN:
    n: int = 4

    def __post_init__(self) -> None:
        if not (1 <= self.n <= MAX_N):
            raise ValueError(f"need 1 <= n <= {MAX_N}")

    def __str__(self) -> str:
        return f"mean{self.n}"


Rule = KofN | MeanN


def parse_rule(text: str | None) -> Rule | None:
    """'2of4' -> KofN(4, 2, 1); 'mean4' -> MeanN(4); 'none' or None -> no rule.

    A vote releases one below its onset (at least 1), the hysteresis the
    board's detect_service_parse_rule gives the same token.
    """
    if text is None or text.lower() == "none":
        return None
    t = text.lower()
    if t.startswith("mean"):
        return MeanN(n=int(t[4:]))
    k, n = t.split("of")
    k, n = int(k), int(n)
    return KofN(n=n, k_on=k, k_off=max(1, k - 1))


def alarm_states(is_drone: np.ndarray, rule: KofN) -> np.ndarray:
    """Alarm state after each window, from the per-window drone/noise calls (vote rule)."""
    if not isinstance(rule, KofN):
        raise TypeError("the mean rule needs the decisions, see alarm_states_for")
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


def alarm_states_mean(relative: np.ndarray, rule: MeanN) -> np.ndarray:
    """Alarm state after each window: mean of the last n relative decisions >= 0."""
    rel = np.asarray(relative, dtype=np.float32)
    if rel.shape[0] == 0:
        return np.zeros(0, dtype=bool)
    ring = np.zeros(rule.n, dtype=np.float32)  # windows before the first are 0
    out = np.zeros(rel.shape[0], dtype=bool)
    for i, v in enumerate(rel):
        ring[i % rule.n] = v
        out[i] = float(ring.sum()) / rule.n >= 0.0
    return out


def alarm_states_for(decisions: np.ndarray, threshold: float, rule: Rule) -> np.ndarray:
    """Alarm state after each window under either rule, from raw decisions and a threshold."""
    d = np.asarray(decisions, dtype=np.float32)
    if isinstance(rule, MeanN):
        return alarm_states_mean(d - np.float32(threshold), rule)
    return alarm_states(d >= threshold, rule)


def alarm_onsets(states: np.ndarray) -> int:
    """How many times the alarm went from OFF to ON."""
    s = np.asarray(states, dtype=bool)
    if s.shape[0] == 0:
        return 0
    prev = np.concatenate([[False], s[:-1]])
    return int((s & ~prev).sum())


def clip_alarmed(decisions: np.ndarray, threshold: float, rule: Rule | None) -> bool:
    """File-level verdict: did the alarm ever turn on (or, without a rule, did any window fire)."""
    d = np.asarray(decisions, dtype=np.float32)
    if rule is None:
        return bool((d >= threshold).any())
    return bool(alarm_states_for(d, threshold, rule).any())
