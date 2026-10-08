import numpy as np
import pytest
from scipy import signal as sps

from beamforming import geometry as g
from beamforming import signals as sg


def test_steering_sign_matches_model():
    # source on +x: the mic on +x is reached first, so its phase advances (+j 2 pi f tau)
    mic = np.array([[0.1, 0.0, 0.0], [-0.1, 0.0, 0.0]])
    a = sg.steering(mic, np.array([1000.0]), g.unit_vector(0.0, 0.0)[None, :])[:, 0, 0]
    tau = 0.1 / sg.C
    np.testing.assert_allclose(np.angle(a), [2 * np.pi * 1000 * tau, -2 * np.pi * 1000 * tau])


def test_propagate_advances_nearer_microphone():
    rng = np.random.default_rng(0)
    s = sps.lfilter(*sps.butter(4, 0.4), rng.standard_normal(4096))
    mic = np.array([[0.1, 0.0, 0.0], [-0.1, 0.0, 0.0]])
    x = sg.propagate(s, mic, g.unit_vector(0.0, 0.0))
    # cross-correlation lag of mic 0 versus mic 1 equals the TDOA 2 r / c (mic 0 leads)
    n = len(s)
    up = np.fft.irfft(np.fft.rfft(x[1]) * np.conj(np.fft.rfft(x[0])), n=n * 16)
    lag = int(np.argmax(up))
    lag = lag - n * 16 if lag > n * 8 else lag
    assert lag / 16 / sg.FS == pytest.approx(0.2 / sg.C, abs=0.05 / sg.FS)


def test_propagate_sub_sample_shift_is_exact_for_tone():
    n = 1600
    f0 = 600.0  # on a bin of the 1600 sample FFT (10 Hz), so the circular shift is exact
    t = np.arange(n) / sg.FS
    s = np.sin(2 * np.pi * f0 * t)
    mic = np.array([[0.0123, 0.0, 0.0]])
    x = sg.propagate(s, mic, g.unit_vector(0.0, 0.0))[0]
    tau = 0.0123 / sg.C
    np.testing.assert_allclose(x, np.sin(2 * np.pi * f0 * (t + tau)), atol=1e-9)


def test_observe_has_no_wraparound():
    rng = np.random.default_rng(1)
    clip = sps.lfilter(*sps.butter(4, 0.25), rng.standard_normal(8000))  # band-limited like DADS
    mic = g.make_array("2x8_rot")
    u = g.unit_vector(np.deg2rad(40), np.deg2rad(30))
    x = sg.observe(clip, mic, u, offset=2000)
    assert x.shape == (16, sg.SEGMENT)
    # a long, wrap free propagation agrees with the cropped one
    ref = sg.propagate(clip, mic, u)[:, 2000 : 2000 + sg.SEGMENT]
    np.testing.assert_allclose(x, ref, atol=2e-3 * np.abs(ref).max())
    with pytest.raises(ValueError):
        sg.observe(clip, mic, u, offset=10)


@pytest.mark.parametrize("band", [None, sg.BAND_HZ])
@pytest.mark.parametrize("snr_db", [30.0, 10.0, -5.0])
def test_add_noise_reaches_requested_snr(band, snr_db):
    rng = np.random.default_rng(2)
    x = rng.standard_normal((16, sg.SEGMENT))
    y = sg.add_noise(x, snr_db, np.random.default_rng(3), snr_band=band)
    noise = y - x
    measured = 10 * np.log10(sg.band_power(x, sg.FS, band) / sg.band_power(noise, sg.FS, band))
    assert measured == pytest.approx(snr_db, abs=1e-6)


def test_add_noise_is_independent_per_channel_and_seeded():
    x = np.zeros((4, 1000)) + 1.0
    a = sg.add_noise(x, 10, np.random.default_rng(5))
    b = sg.add_noise(x, 10, np.random.default_rng(5))
    np.testing.assert_array_equal(a, b)
    noise = a - x
    assert abs(np.corrcoef(noise)[0, 1]) < 0.15


def test_stft_shape_and_bins():
    x = np.random.default_rng(0).standard_normal((16, sg.SEGMENT))
    X = sg.stft(x)
    assert X.shape == (16, 257, 5)
    bins = sg.band_bins()
    assert bins[0] == 10 and bins[-1] == 64 and len(bins) == 55
    np.testing.assert_allclose(sg.bin_freqs(bins)[[0, -1]], [312.5, 2000.0])
    # a bin centred tone lands in its bin
    t = np.arange(sg.SEGMENT) / sg.FS
    X1 = sg.stft(np.sin(2 * np.pi * 31.25 * 20 * t)[None, :])
    assert np.argmax(np.abs(X1[0]).mean(axis=1)) == 20


def test_decimation_passband_and_stopband():
    fs_in = 48_000
    h = sg.decimation_filter(fs_in)
    w, resp = sps.freqz(h, worN=8192, fs=fs_in)
    mag_db = 20 * np.log10(np.abs(resp) + 1e-12)
    assert mag_db[w <= 7000].min() > -1.0
    assert mag_db[w >= 8000].max() < -55.0

    t = np.arange(48_000) / fs_in
    low = sg.decimate_48k_to_16k(np.sin(2 * np.pi * 1000 * t))
    assert len(low) == pytest.approx(16_000, abs=len(h))
    assert np.std(low[len(h) :]) == pytest.approx(1 / np.sqrt(2), rel=0.02)
    high = sg.decimate_48k_to_16k(np.sin(2 * np.pi * 12_000 * t))  # would alias to 4 kHz
    assert np.std(high[len(h) :]) < 1e-2
