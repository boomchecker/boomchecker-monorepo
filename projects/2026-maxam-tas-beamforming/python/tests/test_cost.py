from dataclasses import replace

import pytest

from beamforming import cost, doa

TINY = cost.Workload(M=2, T=1, nfft=8, n_samples=16, K=1, d_coarse=1, d_fine=0, gcc_band_bins=3)


def test_default_workload_is_the_report_configuration():
    w = cost.Workload.from_config()
    assert (w.M, w.T, w.K, w.d_coarse, w.d_fine) == (16, 5, 55, 1368, 121)
    assert w.gcc_band_bins == 341
    assert w == cost.Workload()


def test_workload_follows_the_processing_config():
    assert cost.Workload.from_config(replace(doa.DEFAULT, hop=128)).T == 9
    assert cost.Workload.from_config(replace(doa.DEFAULT, guard_bins=2)).K == 51
    assert cost.Workload.from_config(replace(doa.DEFAULT, fine_span=0.0)).d_fine == 1
    assert cost.Workload.from_config(replace(doa.DEFAULT, coarse_step=10.0)).d_coarse == 36 * 10


def test_fft_cost_is_n_log2_n():
    assert cost.fft_real(512) == 4608.0


def test_das_count_on_a_tiny_case_by_hand():
    parts = cost.cost("das", TINY).parts
    assert parts["stft"] == 2 * 1 * (8 * 3 + 8)  # M T (N log2 N + window)
    assert parts["bin gain"] == 2 * (8 // 2 + 1) * 1 * 2 + 8 // 2 * 10
    assert parts["steering"] == 3 * 2 * 1 + 2 * 2 * 1 * 40 + 2 * 1 * 1 * 4
    assert parts["beamform"] == 1 * 1 * 1 * (4 * 2 + 2) + 1 * 1
    assert cost.cost("das", TINY).mac == 64 + 60 + 174 + 11


def test_mvdr_and_music_count_on_a_tiny_case_by_hand():
    mv = cost.cost("mvdr", TINY).parts
    assert mv["covariance"] == 1 * 3 * 1 * 4  # M (M + 1) / 2 = 3 entries, T = 1
    assert mv["inverse"] == 1 * 4 * 2**3
    assert mv["quadratic form"] == 1 * 1 * (4 + 2) * 4 + 1
    mu = cost.cost("music", TINY).parts
    assert mu["eigendecomposition"] == 1 * 10 * 2**3
    assert mu["projection"] == 1 * 1 * 1 * (4 * 2 + 2) + 1


def test_gcc_cost_is_dominated_by_the_upsampled_inverse_fft():
    full = cost.cost("gcc_phat_ls")
    plain = cost.cost("gcc_phat_ls", replace(cost.Workload(), gcc_upsample=1))
    assert full.parts["inverse fft"] > 0.95 * full.mac
    assert plain.parts["inverse fft"] == pytest.approx(120 * cost.fft_real(3200))
    assert plain.mac < full.mac / 10


def test_music_is_the_cheapest_grid_method_and_mvdr_the_dearest():
    c = {m: cost.cost(m).mac for m in cost.COSTS}
    assert c["music"] < c["das"] <= c["srp_phat"] < c["mvdr"]


def test_cost_grows_with_the_grid_and_the_frames():
    base = cost.cost("das").mac
    assert cost.cost("das", replace(cost.Workload(), d_coarse=4 * 1368)).mac > 2.5 * base
    assert cost.cost("das", replace(cost.Workload(), T=9)).mac > base


def test_time_estimate_uses_cycles_per_mac_and_clock():
    c = cost.Cost({"a": 125e6})
    assert c.mac == 125e6
    assert c.time_s() == pytest.approx(1.0)  # 2 cycles per MAC at 250 MHz
    assert c.time_s(cycles_per_mac=1.0, f_cpu_hz=125e6) == pytest.approx(1.0)


def test_unknown_method_is_rejected():
    with pytest.raises(ValueError, match="unknown method"):
        cost.cost("tops")


def test_every_localisation_method_has_a_cost():
    assert set(cost.COSTS) == set(doa.METHODS)
