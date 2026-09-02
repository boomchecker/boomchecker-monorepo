import numpy as np
from mfcc_analyzer import MFCCAnalyzer

def test_buffer_processing():
    """
    Simulates real-time buffer processing as it would happen on STM32.
    """
    fs = 16000
    window_size = 1024
    
    # Create a 2-second dummy signal
    t = np.linspace(0, 2.0, int(fs * 2.0), endpoint=False)
    signal = 0.5 * np.sin(2 * np.pi * 600 * t) # 600Hz tone
    
    analyzer = MFCCAnalyzer(sample_rate=fs, n_fft=window_size, hop_length=window_size)
    
    print(f"Processing signal in chunks of {window_size} samples...")
    
    # Process in chunks (simulating DMA buffers)
    for i in range(0, len(signal) - window_size, window_size):
        buffer = signal[i : i + window_size]
        features = analyzer.extract_features(buffer)
        
        # Features shape will be (n_mfcc, 1) because hop_length == window_size and center=False
        print(f"Chunk {i//window_size}: Mean MFCC = {np.mean(features):.4f}")

if __name__ == "__main__":
    test_buffer_processing()
