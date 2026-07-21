import matplotlib.pyplot as plt
import utils
import numpy as np
from scipy.fftpack import dct

file_name = "gunshot.wav"

# Load signal
signal, sampling_rate = utils.load_signal(file_name)

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


#====================DISPLAY GRAPHS===================
plt.figure(figsize=(14, 5))

plt.subplot(7, 2, 1)
plt.plot(windowed_signal)
plt.title('Waveform of the Audio Signal')
plt.xlabel('Time')
plt.ylabel('Amplitude')

plt.subplot(7, 2, 2)
plt.plot(frames[0])
plt.title('First Frame of the Signal')
plt.xlabel('Samples')
plt.ylabel('Amplitude')

plt.subplot(7, 2, 7)
plt.plot(pow_frames[0])
plt.title('Power Spectrum of the First Frame')
plt.xlabel('Frequency Bin')
plt.ylabel('Amplitude')

plt.subplot(7, 2, 8)
plt.plot(mag_frames[0])
plt.title('Magnitude Spectrum of the First Frame')
plt.xlabel('Frequency Bin')
plt.ylabel('Amplitude')

plt.subplot(7, 2, 13)
plt.imshow(filter_banks.T, cmap='hot', aspect='auto')
plt.title('Filter Bank Energies')
plt.xlabel('Frame Index')
plt.ylabel('Filter Index')

plt.subplot(7, 2, 14)
plt.imshow(mfcc.T, cmap='hot', aspect='auto')
plt.title('MFCC')
plt.xlabel('Frame Index')
plt.ylabel('Cepstral Coefficient Index')



plt.show()


