import librosa
from scipy.signal import find_peaks
import numpy as np
import matplotlib.pyplot as plt
from numpy.fft import rfft


def load_signal(file_name):
    '''Load the audio file using librosa library'''

    audio_file = file_name
    y, sr = librosa.load(audio_file)

    return y, sr


def find_peak(y):
    '''Finding peak - it is in first 20% of recording'''

    search_region = y[:int(len(y) * 0.2)] # Choosing region for faster finding
    if len(search_region) == 0:
        return 0
    peaks, _ = find_peaks(search_region, height=np.max(search_region)*0.5)
    if len(peaks) == 0:
        return int(np.argmax(np.abs(search_region)))
    peak_idx_relative = peaks[np.argmax(search_region[peaks])]
    peak_index = peak_idx_relative

    return peak_index


def extract_window(y, sr, peak_index):
    '''Extract the windowed signal segment from the audio signal'''
    '''Window is not symetrical around peak - 30% before peak , 70% after peak'''
    '''Window length is 1323 samples afer execution the function'''

    window_length_ms = 60 # Lenght od windowed signal in [ms]
    window_length_samples = int(sr * window_length_ms / 1000) # Lenght od windowed signal in sumples

    window_length_after_peak = int(window_length_samples * 0.7)  # 70% after the peak
    window_length_before_peak = window_length_samples - window_length_after_peak  # 30%

    # Calculate start and end indices of the window around the peak
    start = max(0, peak_index - window_length_before_peak)
    end = min(len(y), peak_index + window_length_after_peak)

    windowed_signal = y[start:end]

    return windowed_signal


def framing(windowed_y, sr):
    '''Framing windowed signal for detail analysis'''
    '''Around 58 frams of length 54 samples each frame'''

    frame_size = 0.0025  # 2,5[ms]
    frame_stride = 0.001  # 1 [ms]
    frame_length, frame_step = frame_size * sr, frame_stride * sr
    signal_length = len(windowed_y)
    frame_length = int(round(frame_length))
    frame_step = int(round(frame_step))
    num_frames = int(np.ceil(float(np.abs(signal_length - frame_length)) / frame_step))

    # Pad signal to ensure all frames have equal number of samples
    pad_signal_length = num_frames * frame_step + frame_length
    z = np.zeros((pad_signal_length - signal_length))
    pad_signal = np.append(windowed_y, z)

    # Slice the signal into frames
    indices = np.tile(np.arange(0, frame_length), (num_frames, 1)) + np.tile(np.arange(0, num_frames * frame_step, frame_step), (frame_length, 1)).T
    frames = pad_signal[indices.astype(np.int32, copy=False)]

    # Apply Hamming window
    frames *= np.hamming(frame_length)

    return frames


def spectrum(frames, NFFT):
    '''Compute power spectrum of each frame using FFT'''

    mag_frames = np.absolute(np.fft.rfft(frames, NFFT)) # Magnitude of the FFT
    pow_frames = ((1.0 / NFFT) * ((mag_frames) ** 2))   # Power Spectrum    

    return mag_frames, pow_frames


def mel_filterbank(pow_frames, sr, nfilt=40, NFFT=512):
    '''Apply Mel-filterbank to power spectrum to get Mel energies'''

    low_freq_mel = 0
    high_freq_mel = 2595 * np.log10(1 + (sr / 2) / 700) # Convert Hz to Mel

    mel_points = np.linspace(low_freq_mel, high_freq_mel, nfilt + 2) # Equally spaced in Mel scale
    hz_points = 700 * (10 ** (mel_points / 2595) - 1) # Convert Mel to Hz

    bin = np.floor((NFFT + 1) * hz_points / sr)

    fbank = np.zeros((nfilt, int(np.floor(NFFT / 2 + 1))))
    for m in range(1, nfilt + 1):
        f_m_minus = int(bin[m - 1])   # left
        f_m = int(bin[m])             # center
        f_m_plus = int(bin[m + 1])    # right

        for k in range(f_m_minus, f_m):
            fbank[m - 1, k] = (k - bin[m - 1]) / (bin[m] - bin[m - 1])
        for k in range(f_m, f_m_plus):
            fbank[m - 1, k] = (bin[m + 1] - k) / (bin[m + 1] - bin[m])

    filter_banks = np.dot(pow_frames, fbank.T)
    filter_banks = np.where(filter_banks == 0, np.finfo(float).eps, filter_banks)  # Numerical stability
    filter_banks = 20 * np.log10(filter_banks)  # dB

    return filter_banks
