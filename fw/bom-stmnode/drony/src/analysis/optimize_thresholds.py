import pandas as pd
import numpy as np
import io
import wave
import os
import glob
import librosa
import joblib
from mfcc_analyzer import MFCCAnalyzer

def load_data():
    print("Loading model and scaler...")
    clf = joblib.load('models/drone_detector_svm.pkl')
    scaler = joblib.load('models/scaler.pkl')
    analyzer = MFCCAnalyzer(sample_rate=16000)

    # We will collect (name, audio_np, expected_label, category)
    records = []

    # 1. Load Parquet Drone (Sample 200 files)
    print("Loading Parquet Drone samples...")
    df_drone = pd.read_parquet('wav/train-00038-of-00039.parquet')
    for idx, row in df_drone.sample(200, random_state=42).iterrows():
        audio_bytes = row['audio']['bytes']
        with wave.open(io.BytesIO(audio_bytes), 'rb') as f:
            raw_audio = f.readframes(f.getnframes())
            audio_np = np.frombuffer(raw_audio, dtype=np.int16).astype(np.float32) / 32768.0
        if len(audio_np) >= 1024:
            records.append((f"Parquet_Drone_{idx}", audio_np, 1, 'Parquet Drone'))
            
    # 2. Load Parquet Noise (Sample 200 files)
    print("Loading Parquet Noise samples...")
    df_noise = pd.read_parquet('wav/train-00003-of-00039.parquet')
    for idx, row in df_noise[df_noise['label'] == 0].sample(200, random_state=42).iterrows():
        audio_bytes = row['audio']['bytes']
        with wave.open(io.BytesIO(audio_bytes), 'rb') as f:
            raw_audio = f.readframes(f.getnframes())
            audio_np = np.frombuffer(raw_audio, dtype=np.int16).astype(np.float32) / 32768.0
        if len(audio_np) >= 1024:
            records.append((f"Parquet_Noise_{idx}", audio_np, 0, 'Parquet Noise'))

    # 3. Load Git/Local WAV files
    local_records = []
    def scan_and_add_wavs(folder_path, expected_label):
        if not os.path.exists(folder_path):
            return
        wav_files = glob.glob(os.path.join(folder_path, "**/*.wav"), recursive=True)
        for path in wav_files:
            name = os.path.basename(path)
            try:
                y, _ = librosa.load(path, sr=16000)
                if len(y) >= 1024:
                    local_records.append((name, y, expected_label))
            except Exception:
                pass

    print("Loading local WAV files...")
    scan_and_add_wavs('wav/DroneAudioDataset-master/DroneAudioDataset-master/Binary_Drone_Audio/yes_drone', 1)
    scan_and_add_wavs('wav/DroneAudioDataset-master/DroneAudioDataset-master/Binary_Drone_Audio/unknown', 0)
    scan_and_add_wavs('wav', 1)
    
    # Deduplicate local records
    seen_names = set()
    for name, audio, label in local_records:
        if name not in seen_names:
            seen_names.add(name)
            name_lower = name.lower()
            if label == 1:
                if "mixed" in name_lower:
                    cat = "Mixed Bebop (Noise+Drone)" if "bebop" in name_lower else ("Mixed Membo (Noise+Drone)" if "membo" in name_lower else "Mixed Drone")
                else:
                    cat = "Bebop Drone (Clean)" if "bebop" in name_lower else ("Membo Drone (Clean)" if "membo" in name_lower else "Drone (Clean)")
            else:
                cat = "Local Ambient Noise (ESC-50)"
            records.append((name, audio, label, cat))

    print(f"Total dataset size for optimization: {len(records)} samples.")
    return records, clf, scaler, analyzer

def extract_and_cache(records, clf, scaler, analyzer):
    print("Extracting features and caching decisions...")
    cached_data = []
    for i, (name, audio, expected, cat) in enumerate(records):
        if i % 1000 == 0 and i > 0:
            print(f"Processed {i}/{len(records)}...")
        
        # Calculate RMS
        rms = np.sqrt(np.mean(audio**2))
        
        # Extract features (unnormalized MCU Match)
        mfccs = analyzer.extract_features(audio)
        mfcc_mean = np.mean(mfccs, axis=1)
        mfcc_std = np.std(mfccs, axis=1)
        features = np.concatenate((mfcc_mean, mfcc_std))
        
        # SVM prediction margin
        scaled = scaler.transform([features])[0]
        decision = clf.decision_function([scaled])[0]
        
        cached_data.append({
            'name': name,
            'expected': expected,
            'category': cat,
            'rms': rms,
            'decision': decision
        })
    return cached_data

def run_grid_search(cached_data):
    print("Running grid search sweep...")
    # Squelch thresholds to test
    squelch_sweeps = [0.0, 0.001, 0.002, 0.005, 0.01, 0.015, 0.02, 0.025, 0.03, 0.04, 0.05]
    # SVM decision thresholds to test
    svm_sweeps = [0.0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.1, 1.2, 1.3, 1.4, 1.5]
    
    best_overall_acc = 0.0
    best_params = None
    best_results = None
    
    print("\n| Squelch RMS | SVM Threshold | Noise Acc (Specificity) | Drone Acc (Sensitivity) | Total Acc | FP Rate | FN Rate |")
    print("|-------------|---------------|-------------------------|-------------------------|-----------|---------|---------|")
    
    for squelch in squelch_sweeps:
        for svm_th in svm_sweeps:
            preds = []
            targets = []
            categories_stat = {}
            
            for item in cached_data:
                # Squelch gate
                if item['rms'] < squelch:
                    pred = 0
                else:
                    pred = 1 if item['decision'] >= svm_th else 0
                
                preds.append(pred)
                targets.append(item['expected'])
                
                cat = item['category']
                if cat not in categories_stat:
                    categories_stat[cat] = {'total': 0, 'correct': 0}
                categories_stat[cat]['total'] += 1
                if pred == item['expected']:
                    categories_stat[cat]['correct'] += 1
            
            preds = np.array(preds)
            targets = np.array(targets)
            
            # Specificity & Sensitivity
            noise_idx = (targets == 0)
            drone_idx = (targets == 1)
            
            noise_acc = np.mean(preds[noise_idx] == 0)
            drone_acc = np.mean(preds[drone_idx] == 1)
            overall_acc = np.mean(preds == targets)
            
            fp_rate = np.mean(preds[noise_idx] == 1)
            fn_rate = np.mean(preds[drone_idx] == 0)
            
            # Print select combinations
            if (squelch in [0.0, 0.005, 0.01, 0.015, 0.02]) and (svm_th in [0.0, 0.3, 0.5, 0.8, 1.0]):
                print(f"| {squelch:11.3f} | {svm_th:13.1f} | {noise_acc:23.2%} | {drone_acc:23.2%} | {overall_acc:9.2%} | {fp_rate:7.2%} | {fn_rate:7.2%} |")
                
            # Criteria for best: We want FP Rate < 2.5% and maximize overall accuracy
            if fp_rate < 0.025:
                if overall_acc > best_overall_acc:
                    best_overall_acc = overall_acc
                    best_params = (squelch, svm_th)
                    best_results = {
                        'noise_acc': noise_acc,
                        'drone_acc': drone_acc,
                        'overall_acc': overall_acc,
                        'fp_rate': fp_rate,
                        'fn_rate': fn_rate,
                        'categories': categories_stat
                    }

    print("\n=== Best Optimized Configuration ===")
    print(f"Squelch RMS Threshold: {best_params[0]:.4f}")
    print(f"SVM Decision Threshold: {best_params[1]:.2f}")
    print(f"Noise Accuracy (Specificity): {best_results['noise_acc']:.2%}")
    print(f"Drone Accuracy (Sensitivity): {best_results['drone_acc']:.2%}")
    print(f"Overall Accuracy: {best_results['overall_acc']:.2%}")
    print(f"False Positive (Alarm) Rate: {best_results['fp_rate']:.2%}")
    print(f"False Negative (Miss) Rate: {best_results['fn_rate']:.2%}")
    
    print("\nAccuracies by category with best parameters:")
    for cat, stat in best_results['categories'].items():
        acc = stat['correct'] / stat['total']
        print(f"  - {cat:30}: {stat['correct']:5d}/{stat['total']:5d} ({acc:.2%})")

    return best_params

if __name__ == '__main__':
    records, clf, scaler, analyzer = load_data()
    cached_data = extract_and_cache(records, clf, scaler, analyzer)
    best_params = run_grid_search(cached_data)
