import pandas as pd
import numpy as np
import io
import wave
import os
from sklearn.model_selection import train_test_split
from sklearn.svm import SVC
from sklearn.metrics import classification_report, accuracy_score
import joblib
from mfcc_analyzer import MFCCAnalyzer

def load_and_extract_features(file_path, max_samples=1000):
    print(f"Zpracování souboru: {file_path}")
    df = pd.read_parquet(file_path)
    
    # Pro urychlení prototypu omezíme počet vzorků, pokud je jich moc
    if len(df) > max_samples:
        df = df.sample(max_samples, random_state=42)
    
    analyzer = MFCCAnalyzer(sample_rate=16000)
    features_list = []
    labels_list = []
    
    for _, row in df.iterrows():
        audio_bytes = row['audio']['bytes']
        label = row['label']
        
        # Přečtení raw dat z WAV kontejneru
        with wave.open(io.BytesIO(audio_bytes), 'rb') as f:
            raw_audio = f.readframes(f.getnframes())
            audio_np = np.frombuffer(raw_audio, dtype=np.int16).astype(np.float32) / 32768.0
            
        # Ošetření příliš krátkých signálů pro n_fft=1024
        if len(audio_np) < 1024:
            continue
            
        # Extrakce MFCC
        # MFCCAnalyzer vrací (n_mfcc, n_frames).
        mfccs = analyzer.extract_features(audio_np)
        
        # Agregace MFCC: průměr a standardní odchylka (více informací než jen průměr)
        mfcc_mean = np.mean(mfccs, axis=1)
        mfcc_std = np.std(mfccs, axis=1)
        feature_vector = np.concatenate((mfcc_mean, mfcc_std))
        
        features_list.append(feature_vector)
        labels_list.append(label)
        
    return np.array(features_list), np.array(labels_list)

if __name__ == "__main__":
    # Načtení dat z obou souborů
    # train-00038-of-00039.parquet -> Drony (label 1)
    # train-00003-of-00039.parquet -> Mix, vezmeme hluk (label 0)
    
    X1, y1 = load_and_extract_features('train-00038-of-00039.parquet', max_samples=1000)
    X0, y0 = load_and_extract_features('train-00003-of-00039.parquet', max_samples=1000)
    
    X = np.vstack((X1, X0))
    y = np.concatenate((y1, y0))
    
    print(f"\nCelková velikost datasetu: {X.shape}")
    print(f"Distribuce labelů: {np.bincount(y)}")
    
    # Split dat
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)
    
    # Trénování SVM
    print("\nTrénování SVM klasifikátoru...")
    clf = SVC(kernel='rbf', C=1.0, gamma='scale', probability=True)
    clf.fit(X_train, y_train)
    
    # Evaluace
    y_pred = clf.predict(X_test)
    print("\nVýsledky klasifikace:")
    print(f"Accuracy: {accuracy_score(y_test, y_pred):.4f}")
    print(classification_report(y_test, y_pred))
    
    # Uložení modelu
    os.makedirs('models', exist_ok=True)
    joblib.dump(clf, 'models/drone_detector_svm.pkl')
    print("Model uložen do: models/drone_detector_svm.pkl")
