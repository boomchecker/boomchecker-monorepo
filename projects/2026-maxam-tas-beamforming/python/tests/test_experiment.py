import numpy as np
import pytest

from beamforming import dads, doa
from beamforming import experiment as ex
from beamforming import geometry as g
from beamforming import metrics as mt


def test_sweep_has_21_unique_geometries():
    geos = ex.sweep_geometries()
    assert len(geos) == len(set(geos)) == 21
    assert sum(geo.topology == "1x8" for geo in geos) == 3
    assert all(geo.height == 0.0 for geo in geos if geo.topology == "1x8")
    assert {geo.height for geo in geos if geo.topology == "2x8_rot"} == set(ex.HEIGHTS)
    assert len({geo.label for geo in geos}) == 21


def test_geometry_builds_the_array_of_geometry_module():
    geo = ex.Geometry("2x8_rot", 0.25, 0.04)
    np.testing.assert_array_equal(geo.mics(), g.make_array("2x8_rot", 0.25, 0.04))
    assert geo.label == "2x8_rot d250/h40" and ex.Geometry("1x8", 0.16).label == "1x8 d160"


def test_trials_are_deterministic_and_nested_in_n_dirs(clip_meta):
    clips = clip_meta[:12]
    one = ex.make_trials(clips, 1)
    five = ex.make_trials(clips, 5)
    again = ex.make_trials(clips, 5)
    assert len(one) == 12 and len(five) == 60
    for a, b in zip(one, five[:12], strict=True):
        assert (a.index, a.d, a.offset) == (b.index, b.d, b.offset)
        np.testing.assert_array_equal(a.u, b.u)
    for a, b in zip(five, again, strict=True):
        assert (a.offset == b.offset) and np.array_equal(a.u, b.u)
    assert all(np.isclose(np.linalg.norm(t.u), 1.0) and t.u[2] >= 0 for t in five)


def test_first_trials_share_the_directions_of_the_validation_table(clip_meta):
    trials = ex.make_trials(clip_meta, 1)
    want = g.random_directions(len(clip_meta), np.random.default_rng(2026))
    np.testing.assert_array_equal(np.array([t.u for t in trials]), want)


def test_noise_streams_are_independent_per_snr_and_trial(clip_meta):
    t0, t1 = ex.make_trials(clip_meta[:2], 1)
    a = ex.noise_rng(t0, 10.0).standard_normal(4)
    np.testing.assert_array_equal(a, ex.noise_rng(t0, 10.0).standard_normal(4))
    assert not np.array_equal(a, ex.noise_rng(t0, 0.0).standard_normal(4))
    assert not np.array_equal(a, ex.noise_rng(t1, 10.0).standard_normal(4))
    assert not np.array_equal(ex.noise_rng(t0, -10.0).standard_normal(4), a)


def job(trial, snrs=(30.0,), methods=("das",), **cond):
    geo = cond.pop("geometry", ex.Geometry("2x8_rot", 0.20, 0.07))
    return ex.Job(trial, ex.Condition(geo, **cond), snrs, methods, with_crb=False)


@pytest.fixture
def trials(clip_meta):
    return ex.make_trials(clip_meta[:6], 1)


def test_job_estimates_the_true_direction_at_high_snr(trials):
    out = ex.run_job(job(trials[0], methods=ex.METHODS))
    assert out.est.shape == (1, len(ex.METHODS), 3) and out.crb is None
    err = g.angular_error_deg(out.est[0], trials[0].u)
    assert err.max() <= 3.0


def test_results_do_not_depend_on_the_other_snrs_or_methods(trials):
    full = ex.run_job(job(trials[1], snrs=(10.0, 0.0), methods=("das", "music")))
    only = ex.run_job(job(trials[1], snrs=(0.0,), methods=("music",)))
    np.testing.assert_array_equal(only.est[0, 0], full.est[1, 1])


def test_wrong_speed_of_sound_biases_the_estimate_and_position_noise_is_seeded(trials):
    base = ex.run_job(job(trials[2]))
    fast = ex.run_job(job(trials[2], c_true=355.0))
    assert not np.allclose(base.est, fast.est)
    p1 = ex.run_job(job(trials[2], sigma_pos=0.002))
    p2 = ex.run_job(job(trials[2], sigma_pos=0.002))
    np.testing.assert_array_equal(p1.est, p2.est)
    assert not np.allclose(p1.est, base.est)


def test_job_uses_the_given_processing_config(trials):
    t = trials[3]
    a = ex.run_job(job(t, snrs=(0.0,), methods=("music",)))
    b = ex.run_job(job(t, snrs=(0.0,), methods=("music",), cfg=doa.Config(guard_bins=3)))
    assert not np.array_equal(a.est, b.est)


def test_job_crb_matches_the_snr(trials):
    out = ex.run_job(
        ex.Job(
            trials[0], ex.Condition(ex.Geometry("2x8_rot", 0.2, 0.07)), (30.0, 10.0), ("das",), True
        )
    )
    assert out.crb is not None and out.crb.shape == (2, 3)
    assert out.crb[1, 2] == pytest.approx(10 * out.crb[0, 2], rel=1e-6)


def test_run_parallel_keeps_order_and_matches_serial(trials):
    jobs = [job(t) for t in trials[:4]]
    serial = ex.run_parallel(ex.run_job, jobs, workers=1)
    parallel = ex.run_parallel(ex.run_job, jobs, workers=2)
    for a, b in zip(serial, parallel, strict=True):
        np.testing.assert_array_equal(a.est, b.est)


def test_outcomes_feed_the_metrics(trials):
    est = np.array([ex.run_job(job(t)).est[0, 0] for t in trials])
    truth = np.array([t.u for t in trials])
    s = mt.summarize(mt.errors(est, truth))
    assert s.n == 6 and s.rmse < 3.0


def test_clip_loading_is_cached_by_path_only(trials):
    assert dads.load_clip(trials[0].path).shape[0] >= trials[0].offset + 1600


def test_evaluate_matches_single_jobs_and_orders_by_condition(trials):
    geos = [ex.Geometry("2x8_rot", 0.20, 0.07), ex.Geometry("1x8", 0.20)]
    conds = [ex.Condition(geo) for geo in geos]
    ev = ex.evaluate(conds, trials[:3], (30.0,), ("das", "music"), with_crb=True, workers=1)
    assert len(ev) == 2 and ev[0].est.shape == (3, 1, 2, 3) and ev[0].crb.shape == (3, 1, 3)
    direct = ex.run_job(ex.Job(trials[1], conds[1], (30.0,), ("das", "music"), True))
    np.testing.assert_array_equal(ev[1].est[1], direct.est)
    np.testing.assert_array_equal(ev[1].crb[1], direct.crb)
    assert not np.array_equal(ev[0].est, ev[1].est)
    np.testing.assert_array_equal(ex.truth(trials[:3]), np.array([t.u for t in trials[:3]]))


def test_evaluate_without_crb_has_none(trials):
    (ev,) = ex.evaluate(
        [ex.Condition(ex.Geometry("1x8", 0.2))], trials[:2], (20.0,), ("das",), workers=1
    )
    assert ev.crb is None
