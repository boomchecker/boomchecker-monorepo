import numpy as np
import librosa
import soundfile as sf
import os
from mfcc_analyzer import MFCCAnalyzer

def validate_sample(sample_path):
    print(f"Validating sample: {sample_path}")
    
    # Load audio
    y, sr = sf.read(sample_path)
    print(f"  Loaded audio shape: {y.shape}, Sample rate: {sr}")
    
    # Initialize analyzer
    analyzer = MFCCAnalyzer(sample_rate=sr)
    
    # Extract features
    features = analyzer.extract_features(y)
    print(f"  Extracted MFCC shape: {features.shape}")
    
    # Basic statistics
    print(f"  MFCC Mean: {np.mean(features):.4f}")
    print(f"  MFCC Std: {np.std(features):.4f}")
    print(f"  MFCC Range: [{np.min(features):.4f}, {np.max(features):.4f}]")

if __name__ == "__main__":
    sample_dir = 'data/samples'
    samples = [f for f in os.listdir(sample_dir) if f.endswith('.wav')]
    
    if samples:
        validate_sample(os.path.join(sample_dir, samples[0]))
    else:
        print("No samples found in data/samples/")
