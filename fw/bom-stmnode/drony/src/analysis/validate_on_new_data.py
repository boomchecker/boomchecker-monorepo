import pandas as pd
import numpy as np
import io
import wave
import joblib
import os
from sklearn.metrics import classification_report, accuracy_score
from mfcc_analyzer import MFCCAnalyzer

def evaluate_on_file(file_path, model_path, max_samples=1000):
    print(f"Validace modelu na souboru: {file_path}")
    
    if not os.path.exists(model_path):
        print(f"Chyba: Model {model_path} nebyl nalezen.")
        return

    # Načtení modelu
    clf = joblib.load(model_path)
    
    # Načtení dat
    df = pd.read_parquet(file_path)
    if len(df) > max_samples:
        df = df.sample(max_samples, random_state=42)
    
    analyzer = MFCCAnalyzer(sample_rate=16000)
    features_list = []
    labels_list = []
    
    for _, row in df.iterrows():
        audio_bytes = row['audio']['bytes']
        label = row['label']
        
        with wave.open(io.BytesIO(audio_bytes), 'rb') as f:
            raw_audio = f.readframes(f.getnframes())
            audio_np = np.frombuffer(raw_audio, dtype=np.int16).astype(np.float32) / 32768.0
            
        if len(audio_np) < 1024:
            continue
            
        mfccs = analyzer.extract_features(audio_np)
        
        # Stejná extrakce jako při trénování (mean + std)
        mfcc_mean = np.mean(mfccs, axis=1)
        mfcc_std = np.std(mfccs, axis=1)
        feature_vector = np.concatenate((mfcc_mean, mfcc_std))
        
        features_list.append(feature_vector)
        labels_list.append(label)
        
    X_val = np.array(features_list)
    y_val = np.array(labels_list)
    
    # Predikce
    y_pred = clf.predict(X_val)
    
    print(f"\n--- Výsledky pro {file_path} ---")
    print(f"Počet testovaných vzorků: {len(y_val)}")
    print(f"Accuracy: {accuracy_score(y_val, y_pred):.4f}")
    print(classification_report(y_val, y_pred))

if __name__ == "__main__":
    new_file = 'train-00036-of-00039.parquet'
    model_file = 'models/drone_detector_svm.pkl'
    
    evaluate_on_file(new_file, model_file)
