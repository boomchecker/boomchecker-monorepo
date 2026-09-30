import numpy as np
import librosa
import scipy.fftpack
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
        Extracts MFCC features from a raw audio buffer exactly matching CMSIS-DSP behavior.
        """
        # Ensure audio_buffer is a numpy array
        audio_buffer = np.array(audio_buffer, dtype=np.float32)
        
        # 1. Hamming Window
        window = 0.54 - 0.46 * np.cos(2.0 * np.pi * np.arange(self.n_fft) / (self.n_fft - 1))
        
        # 2. STFT (center=False matches CMSIS-DSP frame partitioning)
        stft_out = librosa.stft(
            y=audio_buffer,
            n_fft=self.n_fft,
            hop_length=self.hop_length,
            window=window,
            center=False
        )
        
        # 3. Magnitude (CMSIS-DSP uses magnitude, not magnitude squared)
        mag = np.abs(stft_out)
        
        # 4. Mel Filterbank
        mel_fb = librosa.filters.mel(
            sr=self.sample_rate,
            n_fft=self.n_fft,
            n_mels=self.n_mels,
            fmin=0.0,
            fmax=self.sample_rate / 2.0
        )
        mel_spec = np.dot(mel_fb, mag)
        
        # 5. Natural Log (CMSIS-DSP arm_vlog_f32 uses natural log + 1e-6 offset)
        log_mel = np.log(mel_spec + 1e-6)
        
        # 6. DCT-II orthogonal
        dct_mat = scipy.fftpack.dct(np.eye(self.n_mels), type=2, axis=0, norm='ortho')[:self.n_mfcc]
        mfccs = np.dot(dct_mat, log_mel)
        
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
