import numpy as np
import pytest

from beamforming import dads
from beamforming import signals as sg


def test_band_fraction():
    t = np.arange(sg.FS) / sg.FS
    assert dads.band_fraction(np.sin(2 * np.pi * 1000 * t)) > 0.99
    assert dads.band_fraction(np.sin(2 * np.pi * 5000 * t)) < 0.01
    assert dads.band_fraction(np.zeros(100)) == 0.0


def test_random_offset_keeps_margin(rng):
    for n in (dads.MIN_SAMPLES, 8000, 16_384):
        offsets = [dads.random_offset(n, rng) for _ in range(200)]
        assert min(offsets) >= sg.MARGIN
        assert max(offsets) + sg.SEGMENT + sg.MARGIN <= n
    with pytest.raises(ValueError):
        dads.random_offset(dads.MIN_SAMPLES - 1, rng)


def test_list_clips_without_data_explains_fix(tmp_path):
    with pytest.raises(FileNotFoundError, match="task sim:fetch"):
        dads.list_clips(tmp_path)


def test_cached_clips_are_usable(drone_clips):
    assert len(drone_clips) == 100
    for x in drone_clips:
        assert len(x) >= dads.MIN_SAMPLES
        assert np.abs(x).max() <= 1.0
        assert dads.band_fraction(x) >= dads.MIN_BAND_FRACTION
