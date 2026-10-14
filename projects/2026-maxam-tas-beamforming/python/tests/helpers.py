"""Shared helpers for the accuracy tests."""

import numpy as np

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


def band_noise(rng, n=8000):
    """Band-limited noise (about 320 to 2000 Hz at 16 kHz), a stand-in for a drone clip."""
    from scipy import signal as sps

    return sps.lfilter(*sps.butter(4, [0.04, 0.25], btype="band"), rng.standard_normal(n))


def exact_delays(mic_pos, u):
    """``tau_ij = (r_i - r_j)^T u / c`` for all pairs ``i < j`` and the pair differences."""
    i, j = np.triu_indices(len(mic_pos), k=1)
    diff = np.asarray(mic_pos)[i] - np.asarray(mic_pos)[j]
    return diff, diff @ u / sg.C
