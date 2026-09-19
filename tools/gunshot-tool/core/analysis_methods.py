import numpy as np
from scipy.signal import medfilt, stft

IMPULSE_MIN_TIME_DELAY = 0.05  # seconds

BASE_WINDOW_SAMPLE_SIZE = 30000
BASE_WINDOW_IMPLUSE_POSITION = 10  # in %

AMPLITUDE_TRESHOLD = 0.45
MEDIAN_FILTER_TRESHOLD = 0.30
ZSCORE_TRESHOLD = 30
ENERGY_THRESHOLD = 20
STFT_TRESHOLD = 20

MEDIAN_WINDOW_SIZE = 101  # must be odd
ENERGY_WINDOW_SIZE = 50


def filter_impulses(impulses, sample_delay):
    impulses = np.asarray(impulses)
    if impulses.size == 0:
        return np.array([], dtype=np.int64)
    filtered = [impulses[0]]
    for number in impulses[1:]:
        if number - filtered[-1] > sample_delay:
            filtered.append(number)
    return np.array(filtered)


def normalize_audio(audio):
    """Scale to peak 1.0. Raises ValueError on silent or non-finite input."""
    peak = np.max(np.abs(audio))
    if not np.isfinite(peak) or peak == 0:
        raise ValueError(
            'Selected channel carries no usable signal (silent or non-finite samples) — '
            'pick a different detection channel.'
        )
    return audio / peak


def filter_peaks(peaks):
    filtered = []
    counter = 0
    for p in peaks:
        if p and counter == 0:
            filtered.append(True)
            counter = 10
        elif counter > 0:
            filtered.append(False)
            counter -= 1
        else:
            filtered.append(False)
    return filtered


def perform_amplitude_thresholding(audio, sr, threshold=AMPLITUDE_TRESHOLD, edge='rising'):
    norm = normalize_audio(audio)
    above = norm > threshold

    if edge == 'rising':
        crossings = np.where(~above[:-1] & above[1:])[0] + 1
    elif edge == 'falling':
        crossings = np.where(above[:-1] & ~above[1:])[0] + 1
    else:  # both
        rising = np.where(~above[:-1] & above[1:])[0] + 1
        falling = np.where(above[:-1] & ~above[1:])[0] + 1
        crossings = np.sort(np.concatenate([rising, falling]))

    if len(crossings) == 0:
        return np.array([], dtype=np.int64)
    return filter_impulses(crossings, IMPULSE_MIN_TIME_DELAY * sr)


def perform_median_filtering(
    audio, sr,
    median_threshold=MEDIAN_FILTER_TRESHOLD,
    median_window_size=MEDIAN_WINDOW_SIZE
):
    audio = normalize_audio(audio)
    filtered_audio = medfilt(audio, kernel_size=median_window_size)
    difference = np.abs(audio - filtered_audio)
    impulses = np.where(difference > median_threshold)[0]
    return filter_impulses(impulses, IMPULSE_MIN_TIME_DELAY * sr)


def perform_zscore_detection(audio, sr, zscore_threshold=ZSCORE_TRESHOLD):
    audio = normalize_audio(audio)
    std = np.std(audio)
    if std == 0:
        raise ValueError('Selected channel has zero variance — Z-Score detection cannot run on it.')
    z_scores = (audio - np.mean(audio)) / std
    impulses = np.where(z_scores > zscore_threshold)[0]
    return filter_impulses(impulses, IMPULSE_MIN_TIME_DELAY * sr)


def perform_energy_analysis(
    audio, sr,
    energy_window_size=ENERGY_WINDOW_SIZE,
    energy_threshold=ENERGY_THRESHOLD
):
    audio = normalize_audio(audio)
    energy = np.convolve(audio**2, np.ones(energy_window_size) / energy_window_size, mode='same')
    threshold = np.mean(energy) + energy_threshold * np.std(energy)
    impulses = np.where(energy > threshold)[0]
    return filter_impulses(impulses, IMPULSE_MIN_TIME_DELAY * sr)


def perform_spectral_analysis(audio, sr, spectrum_threshold=STFT_TRESHOLD):
    audio = normalize_audio(audio)
    _, times, Zxx = stft(audio, fs=sr, window='hamming')
    spectral_energy = np.sum(np.abs(Zxx), axis=0)
    threshold = np.mean(spectral_energy) + spectrum_threshold * np.std(spectral_energy)
    impulse_times = times[np.where(spectral_energy > threshold)]
    impulses = np.array([int(t * sr) for t in impulse_times])
    return filter_impulses(impulses, IMPULSE_MIN_TIME_DELAY * sr)
