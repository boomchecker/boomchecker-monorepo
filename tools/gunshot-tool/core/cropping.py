import numpy as np
from typing import List, Tuple


def crop_bounds(channels: List[np.ndarray], peak_sample: int, window_size: int, pre_peak_pct: float) -> Tuple[int, int]:
    """
    Sample range of the analysis window around a peak, clamped to the recording.
    Single source of truth so the preview chart, playback, downloads and the saved
    library WAV always cover exactly the same samples.
    """
    pre = int(window_size * pre_peak_pct / 100)
    post = window_size - pre
    n_samples = min(len(ch) for ch in channels)
    start, end = max(0, peak_sample - pre), min(n_samples, peak_sample + post)
    if end <= start:
        raise ValueError(
            f'Peak at sample {peak_sample} falls outside the {n_samples}-sample recording — '
            'nothing to crop.'
        )
    return start, end


def crop_channels(channels: List[np.ndarray], peak_sample: int, window_size: int,
                  pre_peak_pct: float, ch_indices: List[int] = None) -> List[np.ndarray]:
    """Crop the selected channels synchronously around a peak."""
    if ch_indices is None:
        ch_indices = list(range(len(channels)))
    start, end = crop_bounds(channels, peak_sample, window_size, pre_peak_pct)
    return [channels[i][start:end] for i in ch_indices]
