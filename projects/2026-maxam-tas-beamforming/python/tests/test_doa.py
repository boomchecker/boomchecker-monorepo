import numpy as np
import pytest
from scipy import signal as sps

from beamforming import dads, doa
from beamforming import geometry as g
from beamforming import signals as sg

# off-grid directions (azimuth, elevation in degrees): wrap near 360, near horizon, near zenith
DIRECTIONS = [
    (37.3, 22.7),
    (121.4, 63.1),
    (203.8, 41.9),
    (287.2, 11.4),
    (357.6, 33.3),
    (73.9, 4.2),
    (160.2, 87.0),
    (245.5, 55.5),
]
TOL_DEG = 2.0


def simulate(clip, mic_pos, az_deg, el_deg, snr_db, rng):
    u = g.unit_vector(np.deg2rad(az_deg), np.deg2rad(el_deg))
    x = sg.observe(clip, mic_pos, u, dads.random_offset(len(clip), rng))
    return sg.add_noise(x, snr_db, rng), u


def estimate_error(method, x, mic_pos, u_true):
    az, el = doa.localize(method, x, mic_pos)
    return float(g.angular_error_deg(g.unit_vector(az, el), u_true))


@pytest.mark.parametrize("method", doa.METHODS)
@pytest.mark.parametrize(("az", "el"), DIRECTIONS)
def test_2x8_rot_error_within_tolerance(method, az, el, drone_clips, rng):
    mic = g.make_array("2x8_rot", 0.20, 0.07)
    clip = drone_clips[int(az) % len(drone_clips)]
    x, u = simulate(clip, mic, az, el, 30.0, rng)
    assert estimate_error(method, x, mic, u) <= TOL_DEG


@pytest.mark.parametrize("method", doa.METHODS)
@pytest.mark.parametrize(("az", "el"), [d for d in DIRECTIONS if d[1] <= 60])
def test_1x8_azimuth_within_tolerance(method, az, el, drone_clips, rng):
    mic = g.make_array("1x8", 0.20)
    clip = drone_clips[int(az) % len(drone_clips)]
    x, _ = simulate(clip, mic, az, el, 30.0, rng)
    est_az, _ = doa.localize(method, x, mic)
    assert g.azimuth_error_deg(est_az, np.deg2rad(az)) <= TOL_DEG


@pytest.mark.parametrize("method", ["srp_phat", "music", "gcc_phat_ls"])
def test_2x8_without_rotation_smoke(method, drone_clips, rng):
    mic = g.make_array("2x8", 0.20, 0.07)
    for az, el in [(37.3, 22.7), (203.8, 41.9)]:
        x, u = simulate(drone_clips[3], mic, az, el, 30.0, rng)
        assert estimate_error(method, x, mic, u) <= TOL_DEG


@pytest.mark.parametrize("method", doa.METHODS)
def test_band_limited_noise_source_and_low_snr(method, rng):
    clip = sps.lfilter(*sps.butter(4, [0.04, 0.25], btype="band"), rng.standard_normal(8000))
    mic = g.make_array("2x8_rot", 0.20, 0.07)
    errors = []
    for _ in range(6):
        az, el = rng.uniform(0, 360), np.rad2deg(np.arcsin(rng.uniform(0.1, 1)))
        x, u = simulate(clip, mic, az, el, 10.0, rng)
        errors.append(estimate_error(method, x, mic, u))
    assert np.median(errors) <= TOL_DEG


def make_bins(rng, n_mics=16, n_bins=6, n_frames=5):
    mic = g.make_array("2x8_rot", 0.2, 0.07)[:n_mics]
    X = rng.standard_normal((n_mics, n_bins, n_frames)) + 1j * rng.standard_normal(
        (n_mics, n_bins, n_frames)
    )
    freqs = np.linspace(400, 1800, n_bins)
    dirs = g.unit_vector(np.deg2rad([10.0, 100.0, 250.0]), np.deg2rad([20.0, 60.0, 5.0]))
    return X, mic, freqs, dirs


def test_das_bins_matches_quadratic_form(rng):
    X, mic, freqs, dirs = make_bins(rng)
    cfg = doa.Config()
    bins = doa.das_bins(X, mic, freqs, dirs, cfg)
    a = sg.steering(mic, freqs, dirs)
    for k in range(len(freqs)):
        R = X[:, k] @ X[:, k].conj().T / X.shape[2]
        for d in range(len(dirs)):
            assert bins[k, d] == pytest.approx((a[:, k, d].conj() @ R @ a[:, k, d]).real)


def test_srp_phat_bins_matches_gcc_pair_sum(rng):
    X, mic, freqs, dirs = make_bins(rng)
    bins = doa.srp_phat_bins(X, mic, freqs, dirs, doa.Config())
    P = X / np.abs(X)
    a = sg.steering(mic, freqs, dirs)
    M = X.shape[0]
    for k in (0, 3):
        for d in range(len(dirs)):
            total = 0.0
            for m in range(M):
                for n in range(m + 1, M):
                    # R_mn(tau_mn): phase of the PHAT cross spectrum compensated by the delays
                    cross = P[m, k] * P[n, k].conj()
                    comp = np.exp(-2j * np.pi * freqs[k] * (a_tau(a, freqs, k, d, m, n)))
                    total += np.mean((cross * comp).real)
            assert bins[k, d] == pytest.approx(total)


def a_tau(a, freqs, k, d, m, n):
    """tau_m - tau_n recovered from the steering phases (small enough not to wrap)."""
    return (np.angle(a[m, k, d]) - np.angle(a[n, k, d])) / (2 * np.pi * freqs[k])


def test_mvdr_bins_matches_explicit_inverse(rng):
    X, mic, freqs, dirs = make_bins(rng, n_frames=20)
    cfg = doa.Config(loading=1e-2)
    bins = doa.mvdr_bins(X, mic, freqs, dirs, cfg)
    a = sg.steering(mic, freqs, dirs)
    for k in range(len(freqs)):
        R = X[:, k] @ X[:, k].conj().T / X.shape[2]
        R = R + 1e-2 * np.trace(R).real / R.shape[0] * np.eye(R.shape[0])
        Ri = np.linalg.inv(R)
        for d in range(len(dirs)):
            assert bins[k, d] == pytest.approx(1 / (a[:, k, d].conj() @ Ri @ a[:, k, d]).real)


def test_music_bins_matches_noise_subspace_form(rng):
    X, mic, freqs, dirs = make_bins(rng, n_frames=20)
    bins = doa.music_bins(X, mic, freqs, dirs, doa.Config(n_src=1))
    a = sg.steering(mic, freqs, dirs)
    for k in range(len(freqs)):
        R = X[:, k] @ X[:, k].conj().T / X.shape[2]
        _, vec = np.linalg.eigh(R)
        En = vec[:, :-1]
        for d in range(len(dirs)):
            v = En.conj().T @ a[:, k, d]
            assert bins[k, d] == pytest.approx(1 / np.real(v.conj() @ v))


def test_normalised_methods_use_coarse_weights_in_fine_stage():
    m = doa.GRID_METHODS["music"]
    bins = np.array([[1.0, 4.0], [10.0, 5.0]])
    w = doa.bin_weights(m, bins)
    np.testing.assert_allclose(w, [0.25, 0.1])
    np.testing.assert_allclose(doa.combine(bins, w), [0.25 + 1.0, 1.0 + 0.5])
    assert doa.bin_weights(doa.GRID_METHODS["das"], bins) is None


def test_power_map_peaks_at_true_direction(drone_clips, rng):
    mic = g.make_array("2x8_rot", 0.20, 0.07)
    x, _ = simulate(drone_clips[0], mic, 120.0, 60.0, 20.0, rng)
    X = sg.stft(x)
    bins = sg.band_bins()
    for method in doa.GRID_METHODS:
        P = doa.power_map(method, X[:, bins], mic, sg.bin_freqs(bins))
        assert P.shape == (72, 19)
        i, j = np.unravel_index(np.argmax(P), P.shape)
        assert (i * 5, j * 5) == (120, 60)


def test_unknown_method_raises():
    with pytest.raises(ValueError, match="unknown method"):
        doa.localize("nope", np.zeros((16, 1600)), g.make_array("2x8"))
