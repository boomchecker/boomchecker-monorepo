import matplotlib.pyplot as plt
import utils
import numpy as np
from scipy.fftpack import dct
import os

input_folder  = 'nongunshots'
output_folder = 'nongunshots_mfcc'

for filename in os.listdir(input_folder):
    if filename.endswith('.wav'):
        filepath = os.path.join(input_folder, filename)

        # Load signal
        signal, sampling_rate = utils.load_signal(filepath)

        # Find peak index
        peak_index = utils.find_peak(signal)

        # Extract 60[ms] of signal around peak
        windowed_signal = utils.extract_window(signal, sampling_rate, peak_index)

        # Framing the windowed signal + Hamming window
        frames = utils.framing(windowed_signal, sampling_rate)

        # Fast Fourier Transform
        NFFT = 512
        mag_frames, pow_frames = utils.spectrum(frames, NFFT=NFFT)

        # Mel-filter bank
        nfilt = 40 # Number of Mel-filters
        filter_banks = utils.mel_filterbank(pow_frames, sampling_rate, nfilt=nfilt, NFFT=NFFT)

        # Discrete Cosine Transform
        num_ceps = 12
        mfcc = dct(filter_banks, type=2, axis=1, norm='ortho')[:, :num_ceps]

        # Uloženie MFCC, napríklad do npy súboru s rovnakým názvom
        save_path = os.path.join(output_folder, filename.replace('.wav', '_mfcc.npy'))
        np.save(save_path, mfcc)
        
        print(f"Processed and saved MFCC for {filename}")



