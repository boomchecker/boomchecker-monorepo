# Gunshot Audio Signal Processing and MFCC Extraction

This project provides a pipeline for processing gunshot and non-gunshot audio recordings and extracting 
Mel-Frequency Cepstral Coefficients (MFCC) features suitable for further analysis or machine learning tasks.

## Project Structure

The project consists of three main Python scripts:

### 1. utils.py

This module contains reusable functions for audio processing steps:

- `load_signal(file_name)`: Load a `.wav` audio file using `librosa`.
- `find_peak(y)`: Locate the impulse peak within the first 20% of the recording to center analysis.
- `extract_window(y, sr, peak_index)`: Extract a 60 ms non-symmetrical window around the detected peak (40% before, 60% after).
- `framing(windowed_y, sr)`: Segment the windowed signal into overlapping frames of ~2.5 ms length with 1 ms stride and apply a Hamming window.
- `spectrum(frames, NFFT)`: Compute the magnitude and power spectrum using FFT on each frame.
- `mel_filterbank(pow_frames, sr, nfilt=40, NFFT=512)`: Apply a Mel-filterbank to the power spectrum to obtain log Mel filter energies.

### 2. main.py

Batch process all `.wav` files in a libdata input folder, extract MFCC features, and save the MFCC arrays as `.npy` files in an output folder. The processing steps are:

- Load each audio file.
- Detect the peak.
- Extract the 60 ms window.
- Frame the signal and apply the Hamming window.
- Calculate FFT and power spectrum.
- Compute Mel-filterbank energies.
- Apply Discrete Cosine Transform (DCT) to obtain 12 MFCC coefficients.
- Save the MFCC result for each file.

### 3. single_input.py

Demonstrates MFCC extraction and visualization for one sample audio file (`gunshot.wav`). It includes plots showing:

- Waveform of the extracted window.
- First frame of the window after Hamming.
- Power spectrum and magnitude spectrum of the first frame.
- Mel filter bank energies.
- MFCC matrix as a heatmap.

## How to Use

1. Place your `.wav` files in the designated input folders for gunshot or non-gunshot samples.
2. Adjust `input_folder` and `output_folder` paths in `main.py`.
3. Run `main.py` to process all audio files and extract MFCC features.
4. Use `single_input.py` to visualize MFCC extraction for individual files and validate the pipeline.

## Dependencies

- Python 3.x
- numpy
- scipy
- librosa
- matplotlib

## Install dependencies using:

    pip install numpy scipy librosa matplotlib

