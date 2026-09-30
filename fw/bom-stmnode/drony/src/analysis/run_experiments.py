import pandas as pd
import numpy as np
import io
import wave
import os
import glob
import librosa
import scipy.fftpack
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler, MinMaxScaler
from sklearn.svm import SVC
from sklearn.metrics import classification_report, accuracy_score, f1_score
import time

# Custom MFCC implementation mimicking CMSIS-DSP with configurable normalization
def extract_mfcc_custom(audio, sr=16000, n_fft=1024, hop_length=512, n_mels=20, n_mfcc=13, norm_mode='max'):
    audio = np.array(audio, dtype=np.float32)
    
    # 1. Normalization
    if norm_mode == 'max':
        mx = np.max(np.abs(audio))
        if mx > 0:
            audio = audio / mx
    elif norm_mode == 'rms':
        rms = np.sqrt(np.mean(audio**2))
        if rms > 1e-6:
            audio = audio / rms
    # if norm_mode == 'none', do nothing
    
    # 2. Windowing
    window = 0.54 - 0.46 * np.cos(2.0 * np.pi * np.arange(n_fft) / (n_fft - 1))
    
    # 3. STFT
    stft_out = librosa.stft(y=audio, n_fft=n_fft, hop_length=hop_length, window=window, center=False)
    mag = np.abs(stft_out)
    
    # 4. Mel Filterbank
    mel_fb = librosa.filters.mel(sr=sr, n_fft=n_fft, n_mels=n_mels, fmin=0.0, fmax=sr/2.0)
    mel_spec = np.dot(mel_fb, mag)
    
    # 5. Log-Mel
    log_mel = np.log(mel_spec + 1e-6)
    
    # 6. DCT-II orthogonal
    dct_mat = scipy.fftpack.dct(np.eye(n_mels), type=2, axis=0, norm='ortho')[:n_mfcc]
    mfccs = np.dot(dct_mat, log_mel)
    
    return mfccs

def load_data_raw(wav_base_dir, pq_drone_path, pq_noise_path):
    print("Loading raw audio datasets...")
    raw_drones = []
    raw_noise = []
    
    # 1. Load Parquet Drone files
    df_drone = pd.read_parquet(pq_drone_path)
    for _, row in df_drone.iterrows():
        audio_bytes = row['audio']['bytes']
        with wave.open(io.BytesIO(audio_bytes), 'rb') as f:
            raw_audio = f.readframes(f.getnframes())
            audio_np = np.frombuffer(raw_audio, dtype=np.int16).astype(np.float32) / 32768.0
        if len(audio_np) >= 1024:
            raw_drones.append(audio_np)
            
    # 2. Load Parquet Noise files
    df_noise = pd.read_parquet(pq_noise_path)
    for _, row in df_noise.iterrows():
        audio_bytes = row['audio']['bytes']
        with wave.open(io.BytesIO(audio_bytes), 'rb') as f:
            raw_audio = f.readframes(f.getnframes())
            audio_np = np.frombuffer(raw_audio, dtype=np.int16).astype(np.float32) / 32768.0
        if len(audio_np) >= 1024:
            raw_noise.append(audio_np)
            
    # 3. Load Local Yes Drone WAVs (Bebop, Membo, mixed)
    yes_drone_path = os.path.join(wav_base_dir, "DroneAudioDataset-master", "DroneAudioDataset-master", "Binary_Drone_Audio", "yes_drone")
    if os.path.exists(yes_drone_path):
        wav_files = glob.glob(os.path.join(yes_drone_path, "*.wav"))
        for f in wav_files:
            try:
                y, _ = librosa.load(f, sr=16000)
                if len(y) >= 1024:
                    raw_drones.append(y)
            except Exception:
                pass
                
    # 4. Load Local Unknown WAVs (ambient noise)
    unknown_path = os.path.join(wav_base_dir, "DroneAudioDataset-master", "DroneAudioDataset-master", "Binary_Drone_Audio", "unknown")
    if os.path.exists(unknown_path):
        wav_files = glob.glob(os.path.join(unknown_path, "*.wav"))
        for f in wav_files:
            try:
                y, _ = librosa.load(f, sr=16000)
                if len(y) >= 1024:
                    raw_noise.append(y)
            except Exception:
                pass
                
    print(f"Total loaded: {len(raw_drones)} Drone signals, {len(raw_noise)} Noise signals")
    return raw_drones, raw_noise

def run_experiment(raw_drones, raw_noise, norm_mode, scaling_mode, kernel_mode):
    # Balanced Dataset
    min_len = min(len(raw_drones), len(raw_noise))
    # Downsample noise to match drone count
    np.random.seed(42)
    selected_noise_idx = np.random.choice(len(raw_noise), min_len, replace=False)
    selected_noise = [raw_noise[i] for i in selected_noise_idx]
    selected_drones = raw_drones[:min_len]
    
    # Feature extraction
    X = []
    y = []
    
    for audio in selected_drones:
        mfccs = extract_mfcc_custom(audio, norm_mode=norm_mode)
        mfcc_mean = np.mean(mfccs, axis=1)
        mfcc_std = np.std(mfccs, axis=1)
        feature_vector = np.concatenate((mfcc_mean, mfcc_std))
        X.append(feature_vector)
        y.append(1)
        
    for audio in selected_noise:
        mfccs = extract_mfcc_custom(audio, norm_mode=norm_mode)
        mfcc_mean = np.mean(mfccs, axis=1)
        mfcc_std = np.std(mfccs, axis=1)
        feature_vector = np.concatenate((mfcc_mean, mfcc_std))
        X.append(feature_vector)
        y.append(0)
        
    X = np.array(X)
    y = np.array(y)
    
    # Train-test split
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)
    
    # Scaling
    scaler = None
    if scaling_mode == 'standard':
        scaler = StandardScaler()
        X_train = scaler.fit_transform(X_train)
        X_test = scaler.transform(X_test)
    elif scaling_mode == 'minmax':
        scaler = MinMaxScaler()
        X_train = scaler.fit_transform(X_train)
        X_test = scaler.transform(X_test)
        
    # SVM Kernel setup
    if kernel_mode == 'rbf':
        clf = SVC(kernel='rbf', C=1.0, gamma='scale')
    elif kernel_mode == 'linear':
        clf = SVC(kernel='linear', C=1.0)
    elif kernel_mode == 'cubic':
        clf = SVC(kernel='poly', degree=3, C=1.0)
    else:
        raise ValueError(f"Unknown kernel: {kernel_mode}")
        
    clf.fit(X_train, y_train)
    y_pred = clf.predict(X_test)
    
    acc = accuracy_score(y_test, y_pred)
    f1_drone = f1_score(y_test, y_pred, pos_label=1)
    f1_noise = f1_score(y_test, y_pred, pos_label=0)
    n_sv = len(clf.support_)
    
    # Estimated Flash Memory overhead in bytes:
    # SVs array size: n_sv * 26 * sizeof(float) = n_sv * 104 bytes
    # Coefficients: n_sv * sizeof(float) = n_sv * 4 bytes
    # Plus index/classes headers (negligible)
    flash_bytes = n_sv * 108 if kernel_mode != 'linear' else 26 * 4 + 4
    
    return acc, f1_drone, f1_noise, n_sv, flash_bytes

if __name__ == "__main__":
    t0 = time.time()
    raw_drones, raw_noise = load_data_raw(
        wav_base_dir='wav',
        pq_drone_path='wav/train-00038-of-00039.parquet',
        pq_noise_path='wav/train-00003-of-00039.parquet'
    )
    print(f"Data loaded in {time.time() - t0:.2f}s")
    
    # Define combinations
    experiments = [
        # Normalization experiments (no scaling, RBF kernel)
        {'norm': 'max', 'scaling': 'none', 'kernel': 'rbf', 'name': '1. Max Norm, No Scaling, RBF (Current)'},
        {'norm': 'rms', 'scaling': 'none', 'kernel': 'rbf', 'name': '2. RMS Norm, No Scaling, RBF'},
        {'norm': 'none', 'scaling': 'none', 'kernel': 'rbf', 'name': '3. No Norm, No Scaling, RBF'},
        
        # Scaling experiments (with RMS Norm, RBF kernel)
        {'norm': 'rms', 'scaling': 'standard', 'kernel': 'rbf', 'name': '4. RMS Norm, Std Scaler, RBF'},
        {'norm': 'rms', 'scaling': 'minmax', 'kernel': 'rbf', 'name': '5. RMS Norm, MinMax Scaler, RBF'},
        
        # Kernel experiments (with RMS Norm, No Scaling)
        {'norm': 'rms', 'scaling': 'none', 'kernel': 'linear', 'name': '6. RMS Norm, No Scaling, Linear'},
        {'norm': 'rms', 'scaling': 'none', 'kernel': 'cubic', 'name': '7. RMS Norm, No Scaling, Cubic (Poly d=3)'},
        
        # Best scaled + kernel combinations
        {'norm': 'rms', 'scaling': 'standard', 'kernel': 'linear', 'name': '8. RMS Norm, Std Scaler, Linear'},
        {'norm': 'rms', 'scaling': 'standard', 'kernel': 'cubic', 'name': '9. RMS Norm, Std Scaler, Cubic (Poly d=3)'},
    ]
    
    results = []
    for exp in experiments:
        t_start = time.time()
        print(f"\nRunning: {exp['name']}...")
        acc, f1_drone, f1_noise, n_sv, flash_bytes = run_experiment(
            raw_drones, raw_noise, 
            norm_mode=exp['norm'], 
            scaling_mode=exp['scaling'], 
            kernel_mode=exp['kernel']
        )
        print(f"Done in {time.time() - t_start:.2f}s | Acc: {acc:.4f} | F1 Drone: {f1_drone:.4f} | SVs: {n_sv}")
        results.append({
            'name': exp['name'],
            'accuracy': acc,
            'f1_drone': f1_drone,
            'f1_noise': f1_noise,
            'n_sv': n_sv,
            'flash_bytes': flash_bytes
        })
        
    # Write report file
    report_path = 'docs/explorations/model_exploration_report.md'
    os.makedirs(os.path.dirname(report_path), exist_ok=True)
    with open(report_path, 'w', encoding='utf-8') as f:
        f.write("# Model Exploration & Performance Report\n\n")
        f.write("This report compares different signal preprocessing (normalization), feature scaling, and SVM kernel options to optimize the acoustic drone detection system for STM32H5.\n\n")
        
        f.write("## Experiment Results Table\n\n")
        f.write("| # | Experiment Configuration | Accuracy | F1 Drone | F1 Noise | Support Vectors | Est. Flash Memory |\n")
        f.write("|---|--------------------------|----------|----------|----------|-----------------|-------------------|\n")
        for i, r in enumerate(results, 1):
            f.write(f"| {i} | {r['name']} | {r['accuracy']:.4f} | {r['f1_drone']:.4f} | {r['f1_noise']:.4f} | {r['n_sv']} | {r['flash_bytes'] / 1024.0:.2f} KB |\n")
            
        f.write("\n## Analysis and Key Findings\n\n")
        f.write("### 1. Preprocessing (Normalization)\n")
        f.write("- **RMS Normalization** vs **Max Amplitude**:\n")
        f.write("  Evaluating how dividing by the Root Mean Square (RMS) compares to scaling by peak amplitude.\n")
        f.write("- **No Normalization**:\n")
        f.write("  Keeps absolute energy levels, which may help distinguish low energy ambient noises from closer drone noises.\n\n")
        
        f.write("### 2. Feature Scaling\n")
        f.write("- **StandardScaler** (Standardization) and **MinMaxScaler**:\n")
        f.write("  Normalizing feature vectors to prevent large variance features (like first coefficients) from dominating decision functions.\n\n")
        
        f.write("### 3. SVM Kernels & Memory Efficiency on MCU\n")
        f.write("- **Linear Kernel**:\n")
        f.write("  Reduces Flash storage size to just 108 bytes, as we only need to store a single weight vector of 26 parameters and 1 bias.\n")
        f.write("- **Cubic Kernel** (Polynomial degree 3):\n")
        f.write("  A non-linear boundary that can capture more complex feature boundaries compared to Linear.\n")
        f.write("- **RBF Kernel**:\n")
        f.write("  Standard kernel, but requires storing a massive amount of support vectors (very high Flash consumption).\n")
        
    print(f"\nReport successfully saved to {report_path}")
