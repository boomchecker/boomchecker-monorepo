import serial
import time
import numpy as np
import struct
import os
import glob

# --- NASTAVENIA KOMUNIKÁCIE ---
SERIAL_PORT = '/dev/ttyACM0'
BAUD_RATE = 115200
THRESHOLD = 0.5  

# --- NASTAVENIA DÁT ---
# Zložky s ČISTÝMI testovacími .npy dátami (tých 20 % z rozdelenia)
DATASETS = {
    "gunshots_mfcc": 1,      # 1 = Výstrel
    "nongunshots_mfcc": 0,   # 0 = Iný zvuk
    "gunshots_nonDana_mfcc" : 0

}

SNR_LEVELS = [None, 30, 20, 10, 5, 0] # None znamená čisté dáta bez šumu

# --- POMOCNÁ FUNKCIA PRE ŠUM ---
def add_snr_noise_to_mfcc(X_clean, snr_db):
    """Pridá šum do MFCC matice podľa presného SNR v dB."""
    if snr_db is None:
        return X_clean
        
    signal_power = np.mean(X_clean ** 2)
    if signal_power == 0:
        return X_clean
        
    noise_power = signal_power / (10 ** (snr_db / 10))
    noise = np.random.normal(0, np.sqrt(noise_power), X_clean.shape)
    return X_clean + noise

# --- HLAVNÁ EVALUAČNÁ SLUČKA PRE ESP32 ---
def evaluate_esp32_with_noise():
    print(f"Pripájam sa na ESP32 ({SERIAL_PORT})...")
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=3)
        ser.setDTR(False)
        ser.setRTS(False)
        time.sleep(2) # Počkáme na boot
        print("✅ ESP32 je pripravené!\n")
    except Exception as e:
        print(f"❌ Chyba pripojenia: {e}")
        return

    print("="*60)
    print(" EVALUÁCIA ESP32 MODELU (MATEMATICKÝ ŠUM V MFCC) ".center(60))
    print("="*60)

    # Prechádzame jednotlivé úrovne šumu
    for snr in SNR_LEVELS:
        snr_label = "ČISTÉ DÁTA (Bez šumu)" if snr is None else f"SNR = {snr} dB"
        print(f"\n" + "-"*60)
        print(f" TESTUJEM ÚROVEŇ: {snr_label} ".center(60, "-"))
        print("-"*60)

        TP, TN, FP, FN = 0, 0, 0, 0
        total_time_ms = 0
        total_files = 0

        for folder_name, true_label in DATASETS.items():
            if not os.path.isdir(folder_name):
                print(f"⚠️ Priečinok '{folder_name}' nenašiel sa, preskakujem.")
                continue
                
            npy_files = glob.glob(os.path.join(folder_name, '*.npy'))
            
            for file_path in npy_files:
                try:
                    # 1. Načítanie čistých dát
                    X_clean = np.load(file_path)
                    
                    if X_clean.shape != (58, 12):
                        continue # Preskočíme zlý rozmer
                        
                    # 2. Aplikácia šumu na MFCC maticu
                    X_noisy = add_snr_noise_to_mfcc(X_clean, snr)
                    
                    # 3. Odoslanie do ESP32
                    mfcc_flat = X_noisy.flatten()
                    byte_data = struct.pack(f'<{len(mfcc_flat)}f', *mfcc_flat)
                    
                    ser.reset_input_buffer()
                    ser.write(byte_data)
                    ser.flush()
                    
                    # 4. Čakanie na predikciu z ESP32
                    prediction_val = None
                    start_wait = time.time()
                    
                    while (time.time() - start_wait) < 2.0: # 2s timeout
                        if ser.in_waiting > 0:
                            line = ser.readline().decode('utf-8', errors='ignore').strip()
                            if "PREDIKCIA" in line:
                                parts = line.split()
                                prediction_val = float(parts[1])
                                time_ms = int(parts[3])
                                total_time_ms += time_ms
                                break
                    
                    # 5. Vyhodnotenie
                    if prediction_val is not None:
                        total_files += 1
                        predicted_label = 1 if prediction_val > THRESHOLD else 0
                        
                        if true_label == 1 and predicted_label == 1: TP += 1
                        elif true_label == 0 and predicted_label == 0: TN += 1
                        elif true_label == 0 and predicted_label == 1: FP += 1
                        elif true_label == 1 and predicted_label == 0: FN += 1
                        
                except Exception as e:
                    print(f"Chyba pri súbore {file_path}: {e}")

        # --- VÝPIS VÝSLEDKOV PRE DANÉ SNR ---
        if total_files > 0:
            accuracy = (TP + TN) / total_files
            precision = TP / (TP + FP) if (TP + FP) > 0 else 0
            recall = TP / (TP + FN) if (TP + FN) > 0 else 0
            f1_score = 2 * (precision * recall) / (precision + recall) if (precision + recall) > 0 else 0
            avg_time = total_time_ms / total_files
            
            print(f"Otestovaných súborov: {total_files} | Priemerný čas: {avg_time:.2f} ms")
            print(f"Matica zámien: TP={TP:3d} | TN={TN:3d} | FP={FP:3d} | FN={FN:3d}")
            print(f"Accuracy:  {accuracy * 100:.2f} %")
            print(f"F1-Score:  {f1_score:.4f}")
        else:
            print("Nenašli sa žiadne platné súbory.")

    ser.close()
    print("\n✅ Evaluácia dokončená.")

if __name__ == '__main__':
    evaluate_esp32_with_noise()