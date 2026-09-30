import numpy as np
import librosa
import librosa.display
import matplotlib.pyplot as plt
import os

def generate_plots(file_path, label_name):
    # Load audio
    y, sr = librosa.load(file_path, sr=16000)
    
    # Create a figure with 3 subplots
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(10, 12))
    fig.suptitle(f"Acoustic Analysis: {label_name}", fontsize=16)

    # 1. Waveform
    # Use standard plot to avoid waveshow compatibility issues
    time = np.linspace(0, len(y) / sr, len(y))
    ax1.plot(time, y)
    ax1.set_title("Waveform (Time Domain)")
    ax1.set_ylabel("Amplitude")
    ax1.set_xlabel("Time (s)")

    # 2. Power Spectral Density (PSD)
    # Using Welch's method or just FFT magnitude
    D = np.abs(librosa.stft(y, n_fft=1024, hop_length=512))**2
    S_db = librosa.power_to_db(D, ref=np.max)
    img2 = librosa.display.specshow(S_db, sr=sr, hop_length=512, x_axis='time', y_axis='linear', ax=ax2)
    ax2.set_title("Spectrogram / PSD (Frequency Domain)")
    fig.colorbar(img2, ax=ax2, format="%+2.0f dB")

    # 3. MFCC
    mfccs = librosa.feature.mfcc(y=y, sr=sr, n_mfcc=13, n_mels=20, n_fft=1024, hop_length=512)
    img3 = librosa.display.specshow(mfccs, x_axis='time', sr=sr, ax=ax3)
    ax3.set_title("MFCC Coefficients")
    fig.colorbar(img3, ax=ax3)

    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    
    # Save the plot
    output_filename = f"analysis_{label_name}.png"
    plt.savefig(output_filename)
    print(f"Saved plot to {output_filename}")
    plt.close()

if __name__ == "__main__":
    samples = [
        ("data/samples/sample_0_label_1.wav", "Drone"),
        ("data/samples/sample_noise_label_0.wav", "Noise")
    ]
    
    for path, label in samples:
        if os.path.exists(path):
            generate_plots(path, label)
        else:
            print(f"File not found: {path}")
