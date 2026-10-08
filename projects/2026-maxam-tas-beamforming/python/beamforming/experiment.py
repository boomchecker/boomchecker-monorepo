"""Deterministic, paired trials for the ablation, geometry sweep and sensitivity runs.

A *trial* is one DADS clip with one random 100 ms segment and one random direction on the
upper hemisphere. Direction ``d`` of all clips comes from its own seeded generator, so the
trials of ``n_dirs = 1`` are the first trials of any larger run, and direction 0 is the one the
validation table uses (the screening, the validation and the first direction of every later run
share their trials). Every random draw of a trial
(segment, noise at a given SNR, microphone perturbation) has its own generator keyed by
``(SEED, clip, direction, purpose)``, so results do not depend on the worker count, the order
of completion or on which SNRs or methods are run next to each other.

A *job* evaluates one trial under one *condition* (geometry, processing configuration, true
speed of sound, microphone position error) for several SNRs and methods and returns the
estimated unit vectors; the caller turns them into errors with :mod:`beamforming.metrics`.
"""

from __future__ import annotations

import os
from collections.abc import Callable, Sequence
from concurrent.futures import ProcessPoolExecutor
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
from numpy.typing import NDArray

from . import crb, dads, doa
from . import geometry as g
from . import signals as sg

SEED = 2026
DIAMETERS = (0.16, 0.20, 0.25)
HEIGHTS = (0.04, 0.07, 0.10)
METHODS: tuple[str, ...] = doa.METHODS
_NOISE, _OFFSET, _PERTURB = 1, 0, 2


@dataclass(frozen=True)
class Geometry:
    """Array topology and size; ``height`` is ignored (stored as 0) for ``1x8``."""

    topology: g.Topology
    diameter: float
    height: float = 0.0

    def mics(self) -> NDArray[np.float64]:
        return g.make_array(self.topology, self.diameter, self.height)

    @property
    def label(self) -> str:
        h = f"/h{self.height * 1000:.0f}" if self.topology != "1x8" else ""
        return f"{self.topology} d{self.diameter * 1000:.0f}{h}"

    def size_key(self) -> tuple[float, float]:
        """Order for "smaller is preferred": diameter first, then height."""
        return (self.diameter, self.height)


def sweep_geometries() -> list[Geometry]:
    """The 21 configurations of the sweep: 1x8 x 3 diameters, 2x8 and 2x8_rot x 3 x 3."""
    out = [Geometry("1x8", d) for d in DIAMETERS]
    for topology in ("2x8", "2x8_rot"):
        out += [Geometry(topology, d, h) for d in DIAMETERS for h in HEIGHTS]
    return out


@dataclass(frozen=True)
class Trial:
    """One clip, segment start and true direction."""

    index: int  # clip index in the manifest
    d: int  # direction index within the clip
    path: Path
    offset: int
    u: NDArray[np.float64] = field(compare=False)


def make_trials(clips: Sequence[dads.Clip], n_dirs: int = 1, seed: int = SEED) -> list[Trial]:
    """``len(clips) * n_dirs`` trials, direction-major (all clips for ``d = 0`` first)."""
    trials = []
    for d in range(n_dirs):
        # d = 0 keeps the generator of scripts/validate.py, later directions use their own
        rng = np.random.default_rng(seed if d == 0 else [seed, d])
        dirs = g.random_directions(len(clips), rng)
        for i, clip in enumerate(clips):
            offset = dads.random_offset(
                clip.n_samples, np.random.default_rng([seed, clip.index, d, _OFFSET])
            )
            trials.append(Trial(clip.index, d, clip.path, offset, dirs[i]))
    return trials


def noise_rng(trial: Trial, snr_db: float, seed: int = SEED) -> np.random.Generator:
    """Noise generator of a trial at a given SNR (independent of the other SNRs)."""
    return np.random.default_rng(
        [seed, trial.index, trial.d, _NOISE, int(round(snr_db * 10)) + 1000]
    )


@dataclass(frozen=True)
class Condition:
    """What the simulation and the processing assume, which may differ from each other."""

    geometry: Geometry
    cfg: doa.Config = doa.DEFAULT
    c_true: float = sg.C  # speed of sound in the simulation; the methods use ``cfg.c``
    sigma_pos: float = (
        0.0  # std of the position error per coordinate in metres, methods use nominal
    )


@dataclass(frozen=True)
class Job:
    trial: Trial
    condition: Condition
    snrs: tuple[float, ...]
    methods: tuple[str, ...] = METHODS
    with_crb: bool = False


@dataclass(frozen=True)
class Outcome:
    """Estimated unit vectors ``(n_snr, n_methods, 3)`` and optional CRB ``(n_snr, 3)`` in deg."""

    est: NDArray[np.float64]
    crb: NDArray[np.float64] | None


def run_job(job: Job) -> Outcome:
    trial, cond = job.trial, job.condition
    nominal = cond.geometry.mics()
    true_mics = nominal
    if cond.sigma_pos > 0:
        rng = np.random.default_rng([SEED, trial.index, trial.d, _PERTURB])
        true_mics = nominal + rng.normal(0.0, cond.sigma_pos, nominal.shape)
    clean = sg.observe(dads.load_clip(trial.path), true_mics, trial.u, trial.offset, c=cond.c_true)
    est = np.empty((len(job.snrs), len(job.methods), 3))
    bounds = np.empty((len(job.snrs), 3)) if job.with_crb else None
    for s, snr in enumerate(job.snrs):
        x = sg.add_noise(clean, snr, noise_rng(trial, snr))
        for m, method in enumerate(job.methods):
            est[s, m] = g.unit_vector(*doa.localize(method, x, nominal, cond.cfg))
        if bounds is not None:
            b = crb.bound(clean, true_mics, trial.u, snr)
            bounds[s] = (b.azimuth, b.elevation, b.angular)
    return Outcome(est, bounds)


def run_parallel[T, R](
    fn: Callable[[T], R], tasks: Sequence[T], workers: int | None = None
) -> list[R]:
    """``fn`` over ``tasks`` in order; in-process for one worker (debugging, tests)."""
    workers = workers or os.cpu_count() or 1
    if workers == 1 or len(tasks) <= 1:
        return [fn(t) for t in tasks]
    with ProcessPoolExecutor(max_workers=workers) as pool:
        return list(pool.map(fn, tasks, chunksize=max(1, len(tasks) // (workers * 8))))


@dataclass(frozen=True)
class Evaluation:
    """Estimates ``(n_trials, n_snr, n_methods, 3)`` and CRB ``(n_trials, n_snr, 3)`` or None."""

    est: NDArray[np.float64]
    crb: NDArray[np.float64] | None


def truth(trials: Sequence[Trial]) -> NDArray[np.float64]:
    """True unit vectors ``(n_trials, 3)``."""
    return np.array([t.u for t in trials])


def evaluate(
    conditions: Sequence[Condition],
    trials: Sequence[Trial],
    snrs: Sequence[float],
    methods: Sequence[str] = METHODS,
    with_crb: bool = False,
    workers: int | None = None,
) -> list[Evaluation]:
    """Every condition on every trial, in one pool; one :class:`Evaluation` per condition."""
    jobs = [Job(t, c, tuple(snrs), tuple(methods), with_crb) for c in conditions for t in trials]
    outcomes = run_parallel(run_job, jobs, workers)
    n = len(trials)
    out = []
    for k in range(len(conditions)):
        chunk = outcomes[k * n : (k + 1) * n]
        bounds = np.stack([o.crb for o in chunk]) if with_crb else None  # type: ignore[misc]
        out.append(Evaluation(np.stack([o.est for o in chunk]), bounds))
    return out


def load_trials(data_dir: Path, n_dirs: int, limit: int = 0) -> list[Trial]:
    """Trials of the cached clips (the first ``limit`` clips if ``limit`` is positive)."""
    clips = dads.list_clips(data_dir)
    return make_trials(clips[:limit] if limit else clips, n_dirs)
