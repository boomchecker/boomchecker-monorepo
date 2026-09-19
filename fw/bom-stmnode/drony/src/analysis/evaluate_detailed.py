import pandas as pd
import numpy as np
import io
import wave
import os
import glob
import librosa
import scipy.fftpack
import joblib
from mfcc_analyzer import MFCCAnalyzer

# ESC-50 human-readable class names
ESC50_CLASSES = {
    0: "dog", 1: "rooster", 2: "pig", 3: "cow", 4: "frog", 5: "cat", 6: "hen", 7: "insects", 8: "sheep", 9: "crow",
    10: "rain", 11: "sea_waves", 12: "crackling_fire", 13: "crickets", 14: "chirping_birds", 15: "water_drops", 16: "wind", 17: "pouring_water", 18: "toilet_flush", 19: "thunderstorm",
    20: "crying_baby", 21: "sneezing", 22: "clapping", 23: "breathing", 24: "coughing", 25: "footsteps", 26: "laughing", 27: "brushing_teeth", 28: "snoring", 29: "drinking_sipping",
    30: "door_wood_creak", 31: "dustbin", 32: "gas_on_stove", 33: "washing_machine", 34: "vacuum_cleaner", 35: "clock_tick", 36: "clock_alarm", 37: "keyboard_typing", 38: "door_wood_knock", 39: "glass_breaking",
    40: "helicopter", 41: "chainsaw", 42: "siren", 43: "car_horn", 44: "engine", 45: "train", 46: "church_bells", 47: "airplane", 48: "fireworks", 49: "hand_saw"
}

# 1. Feature extraction matching train_svm.py (Unnormalized)
def extract_mfcc_unnorm(audio, analyzer):
    mfccs = analyzer.extract_features(audio)
    mfcc_mean = np.mean(mfccs, axis=1)
    mfcc_std = np.std(mfccs, axis=1)
    return np.concatenate((mfcc_mean, mfcc_std))

# 2. Feature extraction matching evaluate_categories.py (Global RMS Normalized)
def extract_mfcc_norm(audio, sr=16000, n_fft=1024, hop_length=512, n_mels=20, n_mfcc=13):
    audio = np.array(audio, dtype=np.float32)
    rms = np.sqrt(np.mean(audio**2))
    if rms > 1e-6:
        audio = audio / rms
    window = 0.54 - 0.46 * np.cos(2.0 * np.pi * np.arange(n_fft) / (n_fft - 1))
    stft_out = librosa.stft(y=audio, n_fft=n_fft, hop_length=hop_length, window=window, center=False)
    mag = np.abs(stft_out)
    mel_fb = librosa.filters.mel(sr=sr, n_fft=n_fft, n_mels=n_mels, fmin=0.0, fmax=sr/2.0)
    mel_spec = np.dot(mel_fb, mag)
    log_mel = np.log(mel_spec + 1e-6)
    dct_mat = scipy.fftpack.dct(np.eye(n_mels), type=2, axis=0, norm='ortho')[:n_mfcc]
    mfccs = np.dot(dct_mat, log_mel)
    
    mfcc_mean = np.mean(mfccs, axis=1)
    mfcc_std = np.std(mfccs, axis=1)
    return np.concatenate((mfcc_mean, mfcc_std))

def parse_esc50_class(filename):
    """
    Parses ESC-50 class index from chunked filenames like '1-100032-A-00.wav' or '1-100038-A-140.wav'.
    """
    base = os.path.splitext(filename)[0]
    parts = base.split('-')
    if len(parts) < 4:
        return None
    code_part = parts[-1]
    # The last character is the chunk index (e.g. 0..4), the preceding characters are the class ID
    if len(code_part) >= 2:
        class_id_str = code_part[:-1]
        try:
            return int(class_id_str)
        except ValueError:
            return None
    return None

def run_evaluation():
    print("Loading model and scaler...")
    model_path = 'models/drone_detector_svm.pkl'
    scaler_path = 'models/scaler.pkl'
    
    if not os.path.exists(model_path) or not os.path.exists(scaler_path):
        print("Error: Trained model or scaler not found.")
        return
        
    clf = joblib.load(model_path)
    scaler = joblib.load(scaler_path)
    analyzer = MFCCAnalyzer(sample_rate=16000)
    
    # Files lists
    parquet_drone_records = []
    parquet_noise_records = []
    
    # 1. Load Parquet Drone (Sample 200 files)
    print("Loading Parquet Drone samples...")
    df_drone = pd.read_parquet('wav/train-00038-of-00039.parquet')
    for idx, row in df_drone.sample(200, random_state=42).iterrows():
        audio_bytes = row['audio']['bytes']
        with wave.open(io.BytesIO(audio_bytes), 'rb') as f:
            raw_audio = f.readframes(f.getnframes())
            audio_np = np.frombuffer(raw_audio, dtype=np.int16).astype(np.float32) / 32768.0
        if len(audio_np) >= 1024:
            parquet_drone_records.append((f"Parquet_Drone_{idx}", audio_np, 1))
            
    # 2. Load Parquet Noise (Sample 200 files)
    print("Loading Parquet Noise samples...")
    df_noise = pd.read_parquet('wav/train-00003-of-00039.parquet')
    for idx, row in df_noise[df_noise['label'] == 0].sample(200, random_state=42).iterrows():
        audio_bytes = row['audio']['bytes']
        with wave.open(io.BytesIO(audio_bytes), 'rb') as f:
            raw_audio = f.readframes(f.getnframes())
            audio_np = np.frombuffer(raw_audio, dtype=np.int16).astype(np.float32) / 32768.0
        if len(audio_np) >= 1024:
            parquet_noise_records.append((f"Parquet_Noise_{idx}", audio_np, 0))

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
            except Exception as e:
                pass

    print("Loading local WAV files...")
    # Clean and mixed drones
    scan_and_add_wavs('wav/DroneAudioDataset-master/DroneAudioDataset-master/Binary_Drone_Audio/yes_drone', 1)
    # ESC-50 Ambient noise
    scan_and_add_wavs('wav/DroneAudioDataset-master/DroneAudioDataset-master/Binary_Drone_Audio/unknown', 0)
    # Root wavs
    scan_and_add_wavs('wav', 1)
    
    # Deduplicate root and nested WAVs by filename to avoid double counting
    seen_names = set()
    dedup_local_records = []
    for name, audio, label in local_records:
        if name not in seen_names:
            seen_names.add(name)
            dedup_local_records.append((name, audio, label))
    
    print(f"Loaded {len(dedup_local_records)} unique local WAVs.")
    
    # Combine everything
    all_test_signals = []
    # Parquets
    for name, audio, label in parquet_drone_records:
        all_test_signals.append((name, audio, label, 'Parquet Drone'))
    for name, audio, label in parquet_noise_records:
        all_test_signals.append((name, audio, label, 'Parquet Noise'))
    # Local WAVs categorizations
    for name, audio, label in dedup_local_records:
        name_lower = name.lower()
        if label == 1:
            if "mixed" in name_lower:
                if "bebop" in name_lower:
                    cat = "Mixed Bebop (Noise+Drone)"
                elif "membo" in name_lower:
                    cat = "Mixed Membo (Noise+Drone)"
                else:
                    cat = "Mixed Drone (Generic)"
            else:
                if "bebop" in name_lower:
                    cat = "Bebop Drone (Clean)"
                elif "membo" in name_lower:
                    cat = "Membo Drone (Clean)"
                else:
                    cat = "Drone (Clean Generic)"
        else:
            cat = "Local Ambient Noise (ESC-50)"
        all_test_signals.append((name, audio, label, cat))

    # Evaluate both methods
    methods = ['Unnormalized (MCU Match)', 'Global RMS Normalized']
    detailed_results = {}
    
    for method in methods:
        detailed_results[method] = []
        print(f"\nRunning evaluation using method: {method}...")
        for name, audio, expected, cat in all_test_signals:
            if method == 'Unnormalized (MCU Match)':
                features = extract_mfcc_unnorm(audio, analyzer)
            else:
                features = extract_mfcc_norm(audio)
            
            scaled = scaler.transform([features])[0]
            pred = clf.predict([scaled])[0]
            decision = clf.decision_function([scaled])[0]
            
            detailed_results[method].append({
                'name': name,
                'category': cat,
                'expected': expected,
                'predicted': pred,
                'decision': decision,
                'correct': (pred == expected)
            })

    # Prepare markdown reports
    report_paths = [
        'docs/explorations/detailed_test_report.md',
        'C:/Users/Kamil/.gemini/antigravity-cli/brain/bec5749e-2acb-452b-ae17-98d4c3074ea1/detailed_test_report.md'
    ]
    
    for report_path in report_paths:
        os.makedirs(os.path.dirname(report_path), exist_ok=True)
        with open(report_path, 'w', encoding='utf-8') as f:
            f.write("# Detailní report zátěžového testu detekčního algoritmu\n\n")
            f.write("Tento report podrobně popisuje úspěšnost Lineárního SVM modelu na jednotlivých nahrávkách. ")
            f.write("Porovnáváme **dvě metody extrakce příznaků**:\n")
            f.write("1. **Unnormalized (MCU Match)**: Příznaky bez globální normalizace signálu. Přesně odpovídá tomu, co počítá firmware v C (obsahuje frame-level RMS stabilizaci, ale zachovává absolutní měřítko spektrální energie).\n")
            f.write("2. **Global RMS Normalized**: Příznaky, u kterých je celý signál předem podělen globální RMS hodnotou (použito v dřívějších skriptech v Pythonu).\n\n")
            
            for method in methods:
                f.write(f"## Výsledky pro metodu: {method}\n\n")
                
                results_df = pd.DataFrame(detailed_results[method])
                
                # Summary table
                f.write("### 1. Souhrnná tabulka dle kategorií\n\n")
                f.write("| Kategorie | Popis | Celkem vzorků | Správně | Úspěšnost | Průměrná jistota (Decision Value) |\n")
                f.write("|:---|:---|:---:|:---:|:---:|:---:|\n")
                
                for cat, group in results_df.groupby('category'):
                    total = len(group)
                    correct = group['correct'].sum()
                    acc = correct / total
                    avg_dec = group['decision'].mean()
                    f.write(f"| **{cat}** | Testovací vzorky z této skupiny | {total} | {correct} | **{acc:.2%}** | {avg_dec:.4f} |\n")
                
                f.write("\n")
                
                # ESC-50 analysis
                esc_df = results_df[results_df['category'] == 'Local Ambient Noise (ESC-50)'].copy()
                if len(esc_df) > 0:
                    esc_df['class_id'] = esc_df['name'].apply(parse_esc50_class)
                    esc_df['class_name'] = esc_df['class_id'].apply(lambda cid: ESC50_CLASSES.get(cid, "unknown") if cid is not None else None)
                    
                    f.write("### 2. Analýza falešných poplachů v ambientním šumu (ESC-50)\n")
                    f.write("Tabulka ukazuje, které konkrétní typy okolních zvuků způsobují falešné detekce (Falešné poplachy, False Positives). U ideálního chování by měly mít všechny zvuky úspěšnost 100% (což znamená 0% FP) a záporný Decision Value.\n\n")
                    f.write("| Třída zvuku | Třída ID | Celkem testováno | Správně odmítnuto | Falešné poplachy (FP Rate) | Průměrný Decision Value |\n")
                    f.write("|:---|:---:|:---:|:---:|:---:|:---:|\n")
                    
                    for (cid, cname), group in esc_df.groupby(['class_id', 'class_name']):
                        if pd.isna(cid):
                            continue
                        total = len(group)
                        correct = group['correct'].sum()
                        fp_rate = (total - correct) / total
                        avg_dec = group['decision'].mean()
                        # Highlight rows with high false alarms (> 10%)
                        fp_str = f"**{fp_rate:.2%}** ⚠️" if fp_rate > 0.1 else f"{fp_rate:.2%}"
                        f.write(f"| {cname} | {int(cid)} | {total} | {correct} | {fp_str} | {avg_dec:.4f} |\n")
                    f.write("\n")
                
                # Session-level drone analysis
                drone_df = results_df[results_df['expected'] == 1].copy()
                if len(drone_df) > 0:
                    f.write("### 3. Analýza detekce dronů dle relací (Sessions)\n")
                    f.write("Drony jsou seskupeny podle jednotlivých nahrávacích relací. Zde vidíme, zda model spolehlivě detekuje dron v průběhu celé nahrávky.\n\n")
                    f.write("| Název relace | Celkem segmentů | Detekováno | Úspěšnost (Sensitivity) | Průměrná jistota (Decision Value) |\n")
                    f.write("|:---|:---:|:---:|:---:|:---:|\n")
                    
                    # Parse session from name: e.g. B_S2_D1_067-bebop_000_.wav -> B_S2_D1_067
                    # mixed_0-bebop_000_.wav -> mixed_0
                    def parse_session(name):
                        if "Parquet" in name:
                            return "Parquet"
                        base = os.path.splitext(name)[0]
                        parts = base.split('-')
                        if len(parts) > 1:
                            return parts[0]
                        return name
                        
                    drone_df['session'] = drone_df['name'].apply(parse_session)
                    for session, group in drone_df.groupby('session'):
                        if session == "Parquet":
                            continue
                        total = len(group)
                        correct = group['correct'].sum()
                        acc = correct / total
                        avg_dec = group['decision'].mean()
                        # Highlight poor detection (< 90%)
                        acc_str = f"**{acc:.2%}** ❌" if acc < 0.9 else f"{acc:.2%}"
                        f.write(f"| {session} | {total} | {correct} | {acc_str} | {avg_dec:.4f} |\n")
                    f.write("\n")
                
                # List of False Negatives (Missed Drones)
                missed_df = results_df[(results_df['expected'] == 1) & (results_df['correct'] == False)]
                f.write("### 4. Seznam nezachycených dronů (False Negatives)\n")
                f.write("Seznam konkrétních nahrávek s drony, které model vyhodnotil jako šum pozadí.\n\n")
                if len(missed_df) == 0:
                    f.write("*Žádné nezachycené drony v testovací sadě.*\n\n")
                else:
                    f.write("| Soubor | Kategorie | Skutečný Label | Predikce | Decision Margin |\n")
                    f.write("|:---|:---|:---:|:---:|:---:|\n")
                    for _, row in missed_df.iterrows():
                        f.write(f"| {row['name']} | {row['category']} | DRONE (1) | NOISE (0) | {row['decision']:.4f} |\n")
                    f.write("\n")

                # List of False Positives (False Alarms)
                fa_df = results_df[(results_df['expected'] == 0) & (results_df['correct'] == False)]
                f.write("### 5. Seznam falešných poplachů (False Positives)\n")
                f.write("Seznam konkrétních nahrávek šumu, které model vyhodnotil jako dron.\n\n")
                if len(fa_df) == 0:
                    f.write("*Žádné falešné poplachy v testovací sadě.*\n\n")
                else:
                    f.write("| Soubor | Kategorie | Skutečný Label | Predikce | Decision Margin |\n")
                    f.write("|:---|:---|:---:|:---:|:---:|\n")
                    for _, row in fa_df.iterrows():
                        f.write(f"| {row['name']} | {row['category']} | NOISE (0) | DRONE (1) | {row['decision']:.4f} |\n")
                    f.write("\n")
                    
            f.write("## Shrnutí a doporučení\n\n")
            f.write("1. **Rozdíl v normalizacích**:\n")
            f.write("   - Unnormalized (MCU Match) zachovává absolutní energii nahrávky. To může pomoci snížit falešné poplachy u tichých ambientních zvuků, protože drony mají obvykle výrazně vyšší celkovou energii.\n")
            f.write("   - Global RMS Normalized odstraňuje celkovou úroveň hlasitosti, což činí model citlivějším na akustické signatury v tichých nahrávkách, ale zvyšuje míru falešných poplachů u tichého šumu, který má podobný spektrální tvar jako dron.\n\n")
            f.write("2. **Typy šumů náchylné k chybám**:\n")
            f.write("   - Prozkoumejte sekce ESC-50 výše. Zvuky jako **vysavač (vacuum_cleaner)**, **motor (engine)**, **chainsaw (motorová pila)**, **helicopter (vrtulník)** nebo **chainsaw** mají frekvenční spektra velmi podobná rotujícím vrtulím dronů (harmonické složky a širokopásmový šum) a mohou mít vysokou chybovost.\n\n")
            f.write("3. **Threshold Tuning**:\n")
            f.write("   - Pokud je míra falešných poplachů příliš vysoká, doporučujeme zvýšit rozhodovací práh (decision threshold) v C kódu (v `svm_classifier.c`) z `0.0f` na vyšší hodnotu (např. `1.0f` nebo `1.5f`). Tím se odfiltruje většina hraničních případů falešných poplachů za cenu mírného poklesu citlivosti u velmi vzdálených dronů.\n")

    print("Detailed evaluation complete and reports saved.")

if __name__ == '__main__':
    run_evaluation()
