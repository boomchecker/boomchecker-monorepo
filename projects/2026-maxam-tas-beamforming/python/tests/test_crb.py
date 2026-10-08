import numpy as np
import pytest

from beamforming import crb
from beamforming import geometry as g
from beamforming import signals as sg

from .helpers import band_noise

MIC = g.make_array("2x8_rot", 0.20, 0.07)
FREQS = np.array([500.0, 1000.0, 1800.0])


def clean_stft(mic, u, rng, bins=(16, 32, 58), frames=5):
    """STFT of a source ``s`` seen through ``mic``: exactly ``a(u) s`` per bin."""
    freqs = sg.bin_freqs(np.array(bins))
    s = rng.standard_normal((len(bins), frames)) + 1j * rng.standard_normal((len(bins), frames))
    a = sg.steering(mic, freqs, np.asarray(u)[None, :])[:, :, 0]
    return a[:, :, None] * s[None], freqs, s


def finite_difference_fim(X, mic, freqs, u, noise_var, step=1e-6):
    """FIM from numerical derivatives of the steering vector (independent of the closed form)."""
    east, north = g.tangent_basis(u)

    def a_of(v):
        v = v / np.linalg.norm(v)
        return sg.steering(mic, freqs, v[None, :])[:, :, 0]

    a = a_of(u)
    d = [(a_of(u + step * t) - a_of(u - step * t)) / (2 * step) for t in (east, north)]
    s = np.einsum("mk,mkt->kt", a.conj(), X) / len(mic)
    power = np.sum(np.abs(s) ** 2, axis=1)
    fim = np.zeros((2, 2))
    for k in range(len(freqs)):
        proj = np.eye(len(mic)) - np.outer(a[:, k], a[:, k].conj()) / len(mic)
        for i in range(2):
            for j in range(2):
                fim[i, j] += (
                    2 * power[k] / noise_var * np.real(d[i][:, k].conj() @ proj @ d[j][:, k])
                )
    return fim


@pytest.mark.parametrize(("az", "el"), [(30.0, 40.0), (200.0, 75.0), (310.0, 15.0)])
def test_closed_form_matches_the_numerical_fisher_information(az, el, rng):
    u = g.unit_vector(np.deg2rad(az), np.deg2rad(el))
    X, freqs, _ = clean_stft(MIC, u, rng)
    got = crb.fisher(X, MIC, freqs, u, 0.7, sg.C)
    want = finite_difference_fim(X, MIC, freqs, u, 0.7)
    np.testing.assert_allclose(got, want, rtol=1e-4, atol=1e-6 * want.max())


def test_two_microphones_at_the_zenith_have_the_textbook_bound(rng):
    # mics at x = +-d/2, source at the zenith: only the east offset is observable and
    # var(e_E) = sigma^2 c^2 / (|s|^2 (2 pi f d)^2) for one bin and one frame
    d, f, sigma2 = 0.1, 1000.0, 0.01
    mic = np.array([[-d / 2, 0.0, 0.0], [d / 2, 0.0, 0.0]])
    u = np.array([0.0, 0.0, 1.0])
    s = 2.0 - 1.0j
    a = sg.steering(mic, np.array([f]), u[None, :])[:, :, 0]
    X = (a * s)[:, :, None]
    fim = crb.fisher(X, mic, np.array([f]), u, sigma2, sg.C)
    assert fim[1, 1] == 0.0
    want = sigma2 * sg.C**2 / (abs(s) ** 2 * (2 * np.pi * f * d) ** 2)
    assert np.linalg.inv(fim[:1, :1])[0, 0] == pytest.approx(want, rel=1e-9)
    assert np.all(np.isinf(crb.covariance(fim)))  # north unobservable: the pair is singular


def test_bound_scales_with_noise_and_aperture(rng):
    u = g.unit_vector(np.deg2rad(60.0), np.deg2rad(50.0))
    X, freqs, _ = clean_stft(MIC, u, rng)
    cov = crb.covariance(crb.fisher(X, MIC, freqs, u, 1.0, sg.C))
    cov2 = crb.covariance(crb.fisher(X, MIC, freqs, u, 2.0, sg.C))
    np.testing.assert_allclose(cov2, 2.0 * cov, rtol=1e-9)
    half = MIC / 2  # the steering of the shrunk array differs, so rebuild the signal
    Xh, _, _ = clean_stft(half, u, np.random.default_rng(12345))
    X0, _, _ = clean_stft(MIC, u, np.random.default_rng(12345))
    big = crb.covariance(crb.fisher(X0, MIC, freqs, u, 1.0, sg.C))
    small = crb.covariance(crb.fisher(Xh, half, freqs, u, 1.0, sg.C))
    np.testing.assert_allclose(small, 4.0 * big, rtol=1e-9)


def test_planar_array_has_no_elevation_bound_at_the_horizon(rng):
    mic = g.make_array("1x8", 0.20)
    u = g.unit_vector(np.deg2rad(80.0), 0.0)
    X, freqs, _ = clean_stft(mic, u, rng)
    cov = crb.covariance(crb.fisher(X, mic, freqs, u, 1.0, sg.C))
    assert np.all(np.isinf(cov))
    up = g.unit_vector(np.deg2rad(80.0), np.deg2rad(60.0))
    X, freqs, _ = clean_stft(mic, up, rng)
    assert np.all(np.isfinite(crb.covariance(crb.fisher(X, mic, freqs, up, 1.0, sg.C))))


def test_noise_variance_matches_add_noise(rng):
    x = band_noise(rng, 1600)[None, :].repeat(4, axis=0)
    noise = sg.add_noise(x, 10.0, np.random.default_rng(1)) - x
    var_bin = np.mean(
        np.abs(np.fft.rfft(noise, axis=1)[:, 40:180]) ** 2
    )  # rectangular, all samples
    assert var_bin == pytest.approx(crb.noise_var_per_bin(x, 10.0), rel=0.1)


def test_segment_bound_equals_the_sum_over_independent_dft_bins(drone_clips):
    clip = drone_clips[0]
    u = g.unit_vector(np.deg2rad(40.0), np.deg2rad(30.0))
    clean = sg.observe(clip, MIC, u, 3000)
    freqs = np.fft.rfftfreq(1600, 1 / sg.FS)
    bins = np.flatnonzero((freqs >= 300.0) & (freqs <= 2000.0))
    assert len(bins) == 171  # 10 Hz spacing, both ends inclusive
    X = np.fft.rfft(clean, axis=1)[:, bins, None]
    fim = crb.fisher(X, MIC, freqs[bins], u, crb.noise_var_per_bin(clean, 10.0), sg.C)
    want = np.sqrt(np.trace(np.linalg.inv(fim)))
    assert np.rad2deg(want) == pytest.approx(crb.bound(clean, MIC, u, 10.0).angular, rel=1e-9)


def test_bound_per_trial_follows_the_snr(drone_clips):
    clip = drone_clips[1]
    u = g.unit_vector(np.deg2rad(100.0), np.deg2rad(45.0))
    clean = sg.observe(clip, MIC, u, 5000)
    hi, lo = crb.bound(clean, MIC, u, 30.0), crb.bound(clean, MIC, u, 10.0)
    assert lo.angular == pytest.approx(10.0 * hi.angular, rel=1e-6)
    assert hi.angular == pytest.approx(np.hypot(hi.azimuth, hi.elevation))


def test_rms_averages_the_variances():
    b = crb.rms([crb.Bound(3.0, 0.0, 3.0), crb.Bound(4.0, 0.0, 4.0)])
    assert b.azimuth == pytest.approx(np.sqrt(12.5)) and b.elevation == 0.0


def test_bound_stays_below_the_error_of_a_real_estimator(drone_clips):
    from dataclasses import replace

    from beamforming import doa

    u = g.unit_vector(np.deg2rad(150.0), np.deg2rad(40.0))
    clean = sg.observe(drone_clips[2], MIC, u, 4000)
    cfg = replace(doa.DEFAULT, fine_step=0.5, fine_span=2.5)
    errs = []
    for k in range(10):
        x = sg.add_noise(clean, 5.0, np.random.default_rng(k))
        errs.append(
            float(g.angular_error_deg(g.unit_vector(*doa.localize("music", x, MIC, cfg)), u))
        )
    ratio = np.sqrt(np.mean(np.square(errs))) / crb.bound(clean, MIC, u, 5.0).angular
    assert 1.0 < ratio < 4.0  # above the bound, but not by orders of magnitude
