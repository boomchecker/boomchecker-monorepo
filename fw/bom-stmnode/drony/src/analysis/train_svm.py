import pandas as pd
import numpy as np
import io
import wave
import os
import glob
import librosa
from sklearn.model_selection import train_test_split
from sklearn.svm import SVC
from sklearn.metrics import classification_report, accuracy_score
import joblib
from mfcc_analyzer import MFCCAnalyzer

def load_and_extract_features(file_path, max_samples=1000):
    print(f"Zpracování parquet: {file_path}")
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
            
        mfccs = analyzer.extract_features(audio_np)
        mfcc_mean = np.mean(mfccs, axis=1)
        mfcc_std = np.std(mfccs, axis=1)
        feature_vector = np.concatenate((mfcc_mean, mfcc_std))
        
        features_list.append(feature_vector)
        labels_list.append(label)
        
    return np.array(features_list), np.array(labels_list)

def load_local_wavs(base_dir):
    """
    Načte WAV soubory z lokální složky.
    Předpokládá strukturu: 
    - wav/DroneAudioDataset-master/DroneAudioDataset-master/Binary_Drone_Audio/yes_drone (label 1)
    - wav/DroneAudioDataset-master/DroneAudioDataset-master/Binary_Drone_Audio/unknown (label 0)
    """
    print(f"Hledám lokální WAV soubory v: {base_dir}")
    analyzer = MFCCAnalyzer(sample_rate=16000)
    features_list = []
    labels_list = []
    
    # Najít všechny podadresáře 'yes_drone' a 'unknown'
    yes_drone_path = os.path.join(base_dir, "DroneAudioDataset-master", "DroneAudioDataset-master", "Binary_Drone_Audio", "yes_drone")
    unknown_path = os.path.join(base_dir, "DroneAudioDataset-master", "DroneAudioDataset-master", "Binary_Drone_Audio", "unknown")
    
    folders_to_process = []
    if os.path.exists(yes_drone_path):
        folders_to_process.append((yes_drone_path, 1))
    if os.path.exists(unknown_path):
        folders_to_process.append((unknown_path, 0))
        
    # Přidat i testovací soubory z kořenu wav/ (ruční anotace)
    # Pro zjednodušení teď vezmeme jen ty z datasetu, aby se model nezmátl našimi testovacími mixy
    
    for folder_path, label in folders_to_process:
        wav_files = glob.glob(os.path.join(folder_path, "*.wav"))
        print(f"Nalezeno {len(wav_files)} souborů ve složce {folder_path} (label {label})")
        
        for wav_file in wav_files:
            try:
                y, sr = librosa.load(wav_file, sr=16000)
                if len(y) < 1024:
                    continue
                
                mfccs = analyzer.extract_features(y)
                mfcc_mean = np.mean(mfccs, axis=1)
                mfcc_std = np.std(mfccs, axis=1)
                feature_vector = np.concatenate((mfcc_mean, mfcc_std))
                
                features_list.append(feature_vector)
                labels_list.append(label)
            except Exception as e:
                print(f"Chyba při zpracování {wav_file}: {e}")

    return np.array(features_list), np.array(labels_list)

if __name__ == "__main__":
    # 1. Načtení dat z původních parquet souborů (základní šum a drony)
    # Zmenšíme max_samples, abychom dali větší váhu novým datům
    X_pq1, y_pq1 = load_and_extract_features('wav/train-00038-of-00039.parquet', max_samples=500)
    X_pq0, y_pq0 = load_and_extract_features('wav/train-00003-of-00039.parquet', max_samples=500)
    
    # 2. Načtení nových lokálních WAV souborů
    X_loc, y_loc = load_local_wavs('wav')
    
    # Spojení datasetů
    if len(X_loc) > 0:
        X = np.vstack((X_pq1, X_pq0, X_loc))
        y = np.concatenate((y_pq1, y_pq0, y_loc))
    else:
        X = np.vstack((X_pq1, X_pq0))
        y = np.concatenate((y_pq1, y_pq0))
    
    print(f"\nCelková velikost spojeného datasetu: {X.shape}")
    print(f"Distribuce labelů: {np.bincount(y)}")
    
    # Split dat (80% trénink, 20% testování)
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)
    
    # Trénování SVM
    print("\nTrénování SVM klasifikátoru...")
    clf = SVC(kernel='rbf', C=1.0, gamma='scale', probability=True)
    clf.fit(X_train, y_train)
    
    # Evaluace
    y_pred = clf.predict(X_test)
    print("\nVýsledky klasifikace na testovacím setu (20 % dat):")
    print(f"Accuracy: {accuracy_score(y_test, y_pred):.4f}")
    print(classification_report(y_test, y_pred))
    
    # Uložení modelu
    os.makedirs('models', exist_ok=True)
    joblib.dump(clf, 'models/drone_detector_svm.pkl')
    print("Model uložen do: models/drone_detector_svm.pkl")
