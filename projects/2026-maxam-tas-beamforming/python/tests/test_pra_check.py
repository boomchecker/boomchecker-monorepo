import numpy as np
import pytest

from beamforming import doa, pra_check
from beamforming import geometry as g
from beamforming import signals as sg

from .helpers import DIRECTIONS, simulate

# own method and the pra algorithm that implements the same estimator
PAIRS = [("srp_phat", "SRP"), ("music", "NormMUSIC")]


def coarse_argmax(method, X, mic, bins):
    P = doa.power_map(method, X[:, bins], mic, sg.bin_freqs(bins))
    i, j = np.unravel_index(np.argmax(P), P.shape)
    az, el = g.coarse_grid(5.0)
    k = i * g.grid_shape(5.0)[1] + j
    return g.unit_vector(az[k], el[k])


@pytest.mark.parametrize(("own", "ref"), PAIRS)
@pytest.mark.parametrize(("az", "el"), DIRECTIONS[:6])
def test_own_coarse_maximum_matches_pra(own, ref, az, el, drone_clips, rng):
    mic = g.make_array("2x8_rot", 0.20, 0.07)
    x, _ = simulate(drone_clips[int(az) % len(drone_clips)], mic, az, el, 20.0, rng)
    X = sg.stft(x)
    bins = sg.band_bins()
    a, e = pra_check.locate(ref, X, mic, bins=bins)
    # identical estimator on an identical grid: the same node (measured 0 deg down to 0 dB SNR)
    err = g.angular_error_deg(g.unit_vector(a, e), coarse_argmax(own, X, mic, bins))
    assert err <= 0.1


@pytest.mark.parametrize("name", pra_check.PRA_METHODS)
def test_all_pra_methods_run_and_stay_in_upper_hemisphere(name, drone_clips, rng):
    mic = g.make_array("2x8_rot", 0.20, 0.07)
    x, u = simulate(drone_clips[0], mic, 120.0, 50.0, 30.0, rng)
    a, e = pra_check.locate(name, sg.stft(x), mic)
    assert 0.0 <= a < 2 * np.pi
    assert 0.0 <= e <= np.pi / 2 + 1e-9
    if name != "TOPS":  # pra TOPS indexing issue, see module docstring
        assert g.angular_error_deg(g.unit_vector(a, e), u) <= 5.0


def test_unknown_pra_method_raises():
    with pytest.raises(ValueError, match="unknown pra method"):
        pra_check.locate("nope", np.zeros((16, 257, 5), complex), g.make_array("2x8"))
