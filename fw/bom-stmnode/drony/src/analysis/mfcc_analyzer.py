import numpy as np
import librosa
import matplotlib.pyplot as plt

class MFCCAnalyzer:
    def __init__(self, sample_rate=16000, n_mfcc=13, n_mels=20, n_fft=1024, hop_length=512):
        """
        Initializes the MFCC analyzer with parameters matching the STM32 firmware config.
        """
        self.sample_rate = sample_rate
        self.n_mfcc = n_mfcc
        self.n_mels = n_mels
        self.n_fft = n_fft
        self.hop_length = hop_length

    def extract_features(self, audio_buffer):
        """
        Extracts MFCC features from a raw audio buffer (numpy array).
        """
        # Ensure audio_buffer is a numpy array
        audio_buffer = np.array(audio_buffer, dtype=np.float32)
        
        # Extract MFCCs
        # librosa.feature.mfcc uses power spectrogram by default
        mfccs = librosa.feature.mfcc(
            y=audio_buffer, 
            sr=self.sample_rate, 
            n_mfcc=self.n_mfcc,
            n_mels=self.n_mels,
            n_fft=self.n_fft,
            hop_length=self.hop_length,
            center=False # Match firmware windowing behavior
        )
        
        return mfccs

    def plot_mfcc(self, mfccs, title="MFCC Spectrogram"):
        """
        Visualizes the extracted MFCC features.
        """
        plt.figure(figsize=(10, 4))
        librosa.display.specshow(mfccs, x_axis='time', sr=self.sample_rate)
        plt.colorbar()
        plt.title(title)
        plt.tight_layout()
        plt.show()

if __name__ == "__main__":
    # Test with a dummy buffer (sine wave)
    fs = 16000
    duration = 1.0  # seconds
    t = np.linspace(0, duration, int(fs * duration), endpoint=False)
    # Sine wave at 440Hz + some noise
    dummy_audio = 0.5 * np.sin(2 * np.pi * 440 * t) + 0.1 * np.random.randn(len(t))
    
    analyzer = MFCCAnalyzer(sample_rate=fs)
    features = analyzer.extract_features(dummy_audio)
    
    print(f"Extracted MFCC shape: {features.shape}")
    print("First frame MFCCs:")
    print(features[:, 0])
