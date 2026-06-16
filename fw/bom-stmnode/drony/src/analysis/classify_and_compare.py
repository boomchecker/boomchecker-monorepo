import numpy as np
import librosa
import librosa.display
import matplotlib.pyplot as plt
import joblib
import os

from mfcc_analyzer import MFCCAnalyzer

def process_file(file_path):
    y, sr = librosa.load(file_path, sr=16000)
    analyzer = MFCCAnalyzer(sample_rate=16000, n_mfcc=13, n_mels=20, n_fft=1024, hop_length=512)
    
    # Extract MFCCs using the same logic as training
    mfccs = analyzer.extract_features(y)
    
    # For classification, we use mean and standard deviation to match training
    mfcc_mean = np.mean(mfccs, axis=1)
    mfcc_std = np.std(mfccs, axis=1)
    feature_vector = np.concatenate((mfcc_mean, mfcc_std))
    
    # Also prepare PSD for plotting
    D = np.abs(librosa.stft(y, n_fft=1024, hop_length=512))**2
    S_db = librosa.power_to_db(D, ref=np.max)
    
    return y, sr, mfccs, S_db, feature_vector

def run_analysis():
    import glob
    # Load model
    model_path = "models/drone_detector_svm.pkl"
    if not os.path.exists(model_path):
        print(f"Model not found at {model_path}")
        return
    
    clf = joblib.load(model_path)
    
    wav_files = glob.glob("wav/*.wav")
    print(f"Found {len(wav_files)} WAV files for analysis.")
    
    results = []
    for path in wav_files:
        name = os.path.basename(path)
        y, sr, mfccs, S_db, feature_vector = process_file(path)
        # Prediction with probability
        pred = clf.predict([feature_vector])[0]
        probs = clf.predict_proba([feature_vector])[0]
        
        label = "DRONE" if pred == 1 else "NOISE"
        # Confidence is probability of the predicted class
        confidence = probs[1] if pred == 1 else probs[0]
        
        results.append((name, y, sr, mfccs, S_db, label, confidence))
        print(f"[{label}] {name:30} | Conf: {confidence:7.2%} | Drone Prob: {probs[1]:7.2%}")

    # Plotting (only first 6 to keep it readable)
    to_plot = results[:6]
    cols = 2
    rows = (len(to_plot) + 1) // 2
    fig, axes = plt.subplots(rows * 3, cols, figsize=(15, 5 * rows))
    fig.suptitle("Batch Analysis: Drone Detection Performance", fontsize=18)

    for idx, (name, y, sr, mfccs, S_db, label, confidence) in enumerate(to_plot):
        r_base = (idx // cols) * 3
        c = idx % cols
        
        # 1. Waveform
        time = np.linspace(0, len(y) / sr, len(y))
        axes[r_base, c].plot(time, y)
        axes[r_base, c].set_title(f"{name}\nResult: {label} ({confidence:.1%})")
        
        # 2. PSD
        librosa.display.specshow(S_db, sr=sr, hop_length=512, x_axis='time', y_axis='linear', ax=axes[r_base+1, c])
        
        # 3. MFCC
        librosa.display.specshow(mfccs, x_axis='time', sr=sr, ax=axes[r_base+2, c])

    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    output_png = "comparison_batch_results.png"
    plt.savefig(output_png)
    print(f"\nBatch comparison plot saved to {output_png}")

if __name__ == "__main__":
    run_analysis()
