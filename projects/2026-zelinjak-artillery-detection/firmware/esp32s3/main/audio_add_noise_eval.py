import serial
import time
import numpy as np
import struct
import os
import glob

SERIAL_PORT = '/dev/ttyACM0'
BAUD_RATE = 115200
THRESHOLD = 0.5  # Hranica: ak je predikcia > 0.5, považujeme to za VÝSTREL

# Nastavenie priečinkov a ich skutočných tried (Ground Truth)
# UPRAV SI CESTY, AK ICH MÁŠ ULOŽENÉ INDE!
DATASETS = {
    "gunshots_mfcc_audio_noise": 1,           # 1 = Výstrel Dana
    "gunshots_nonDana_mfcc_audio_noise": 0,   # 0 = Výstrel NonDana
    "nongunshots_mfcc_audio_noise": 0         # 0 = Iný zvuk
}

def evaluate_model():
    print(f"Pripájam sa na {SERIAL_PORT}...")
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=3)
        ser.setDTR(False)
        ser.setRTS(False)
        time.sleep(2) # Počkáme na boot ESP32
        print("ESP32 je pripravené!\n")
    except Exception as e:
        print(f"Chyba pripojenia: {e}")
        return

    # Štatistiky pre maticu zámien (Confusion Matrix)
    TP = 0  # True Positive  (Bol výstrel, AI povedala výstrel)
    TN = 0  # True Negative  (Nebol výstrel, AI povedala nebol)
    FP = 0  # False Positive (Nebol výstrel, ale AI falošne poplachovala)
    FN = 0  # False Negative (Bol výstrel, ale AI ho prepočula)
    
    total_files   = 0
    total_time_ms = 0

    print("="*50)
    print(" ZAČÍNAM EVALUÁCIU ".center(50, "="))
    print("="*50)

    for folder_name, true_label in DATASETS.items():
        if not os.path.isdir(folder_name):
            print(f"⚠️ Upozornenie: Priečinok '{folder_name}' sa nenašiel, preskakujem.")
            continue
            
        npy_files = glob.glob(os.path.join(folder_name, '*.npy'))
        print(f"\nSpracovávam priečinok: {folder_name} ({len(npy_files)} súborov, Očakávaná trieda: {true_label})")
        
        for file_path in npy_files:
            try:
                # 1. Načítanie a kontrola dát
                mfcc_data = np.load(file_path)
                mfcc_flat = mfcc_data.flatten()
                
                if len(mfcc_flat) != 696:
                    print(f"Preskakujem {file_path}: zlý rozmer ({len(mfcc_flat)})")
                    continue
                    
                byte_data = struct.pack(f'<{len(mfcc_flat)}f', *mfcc_flat)
                
                # 2. Vyčistenie buffera a odoslanie do ESP32
                ser.reset_input_buffer()
                ser.write(byte_data)
                ser.flush()
                
                # 3. Čakanie na odpoveď (Synchrónne čítanie)
                prediction_val = None
                start_wait = time.time()
                
                while (time.time() - start_wait) < 3.0: # Timeout 3 sekundy na 1 súbor
                    if ser.in_waiting > 0:
                        line = ser.readline().decode('utf-8', errors='ignore').strip()
                        if "PREDIKCIA" in line:
                            # Príklad: "PREDIKCIA: 0.9961 (Cas: 32 ms)"
                            parts = line.split()
                            prediction_val = float(parts[1])
                            time_ms = int(parts[3])
                            total_time_ms += time_ms
                            break
                
                # 4. Vyhodnotenie jedného súboru
                if prediction_val is not None:
                    total_files += 1
                    predicted_label = 1 if prediction_val > THRESHOLD else 0
                    
                    if true_label == 1 and predicted_label == 1:
                        TP += 1
                    elif true_label == 0 and predicted_label == 0:
                        TN += 1
                    elif true_label == 0 and predicted_label == 1:
                        FP += 1
                    elif true_label == 1 and predicted_label == 0:
                        FN += 1
                else:
                    print(f"❌ Chyba komunikácie pri súbore: {file_path}")
                    
            except Exception as e:
                print(f"Chyba pri súbore {file_path}: {e}")

    # --- VÝPOČET FINÁLNYCH ŠTATISTÍK ---
    if total_files > 0:
        accuracy = (TP + TN) / total_files
        precision = TP / (TP + FP) if (TP + FP) > 0 else 0
        recall = TP / (TP + FN) if (TP + FN) > 0 else 0
        f1_score = 2 * (precision * recall) / (precision + recall) if (precision + recall) > 0 else 0
        avg_time = total_time_ms / total_files
        
        print("\n" + "="*50)
        print(" VÝSLEDKY EVALUÁCIE (Do diplomovky) ".center(50, "="))
        print("="*50)
        print(f"Otestovaných súborov: {total_files}")
        print(f"Priemerný čas inferencie: {avg_time:.2f} ms")
        print("\n--- MATICA ZÁMIEN (Confusion Matrix) ---")
        print(f"True Positives  (TP):  {TP} (Správne zachytené výstrely)")
        print(f"True Negatives  (TN):  {TN} (Správne ignorované ticho)")
        print(f"False Positives (FP):  {FP} (Falošné poplachy!)")
        print(f"False Negatives (FN):  {FN} (Nezachytené výstrely!)")
        print("\n--- METRIKY ---")
        print(f"Accuracy  (Presnosť):  {accuracy * 100:.2f} %")
        print(f"Precision:             {precision * 100:.2f} %")
        print(f"Recall (Senzitivita):  {recall * 100:.2f} %")
        print(f"F1-Score:              {f1_score:.4f}")
        print("="*50 + "\n")
    else:
        print("\nNenašli sa žiadne platné súbory na testovanie.")

    ser.close()

if __name__ == '__main__':
    evaluate_model()