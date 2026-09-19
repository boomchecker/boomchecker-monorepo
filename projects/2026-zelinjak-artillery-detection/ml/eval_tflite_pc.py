import os
import numpy as np
import tensorflow as tf
from sklearn.metrics import accuracy_score, precision_score, recall_score, f1_score, matthews_corrcoef, confusion_matrix

# --- NASTAVENIA ---
# Sem daj cestu k tvojmu KVANTIZOVANÉMU modelu, ktorý si nahrával do ESP32
MODEL_PATH = 'model.tflite' 

DIRS_AND_LABELS = {
    'gunshots_mfcc_audio_noise': 1,
    'gunshots_nonDana_mfcc_audio_noise': 0,
    'nongunshots_mfcc_audio_noise': 0
}

def main():
    print(f"Načítavam TFLite model: {MODEL_PATH}")
    try:
        interpreter = tf.lite.Interpreter(model_path=MODEL_PATH)
        interpreter.allocate_tensors()
    except Exception as e:
        print(f"Chyba pri načítaní modelu: {e}")
        return

    # Získanie informácií o vstupoch a výstupoch modelu
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()
    
    # Kontrola, či model reálne očakáva int8 na vstupe
    input_dtype = input_details[0]['dtype']
    print(f"Model očakáva vstupný dátový typ: {input_dtype}")

    y_true = []
    y_pred_probs = []
    
    print("\nNačítavam .npy súbory a spúšťam inferenciu (TFLite)...")
    
    for directory, label in DIRS_AND_LABELS.items():
        if not os.path.exists(directory):
            print(f"Preskakujem neexistujúci priečinok: {directory}")
            continue
            
        files_loaded = 0
        for filename in os.listdir(directory):
            if filename.endswith('.npy'):
                filepath = os.path.join(directory, filename)
                
                # Načítanie matice a úprava tvaru (1, 58, 12, 1) pre TFLite
                mfcc = np.load(filepath)
                input_tensor = np.expand_dims(mfcc, axis=(0, -1)).astype(np.float32)
                
                # Ak je model plne kvantizovaný a vyžaduje int8 na vstupe,
                # musíme vstup manuálne kvantizovať (rovnako ako to robíš v C++ na ESP32)
                if input_dtype == np.int8:
                    scale, zero_point = input_details[0]['quantization']
                    quantized_tensor = np.round(input_tensor / scale + zero_point)
                    # TOTO zabezpečí, že čísla nepretečú do mínusu, presne ako na ESP32!
                    input_tensor = np.clip(quantized_tensor, -128, 127).astype(np.int8)
                
                # Vloženie dát do modelu a spustenie výpočtu
                interpreter.set_tensor(input_details[0]['index'], input_tensor)
                interpreter.invoke()
                output_data = interpreter.get_tensor(output_details[0]['index'])
                
                # Ak je výstup int8, musíme ho dekvantizovať späť na float (0.0 - 1.0)
                if output_details[0]['dtype'] == np.int8:
                    scale, zero_point = output_details[0]['quantization']
                    pred_prob = (output_data[0][0] - zero_point) * scale
                else:
                    pred_prob = output_data[0][0]
                
                y_pred_probs.append(pred_prob)
                y_true.append(label)
                files_loaded += 1
                
        print(f" -> Z priečinka '{directory}' otestovaných {files_loaded} vzoriek.")

    if not y_true:
        print("Neboli nájdené žiadne dáta.")
        return

    # Prahovanie (všetko >= 0.5 je Výstrel/Trieda 1)
    y_pred_array = (np.array(y_pred_probs) >= 0.5).astype(int)
    y_true_array = np.array(y_true)
    
    # Výpočet metrík
    acc = accuracy_score(y_true_array, y_pred_array)
    prec = precision_score(y_true_array, y_pred_array, zero_division=0)
    rec = recall_score(y_true_array, y_pred_array, zero_division=0)
    f1 = f1_score(y_true_array, y_pred_array, zero_division=0)
    mcc = matthews_corrcoef(y_true_array, y_pred_array)
    cm = confusion_matrix(y_true_array, y_pred_array)
    
    # Formátovaný výpis
    print("\n" + "="*50)
    print(" VÝSLEDKY EVALUÁCIE (PC s INT8 TFLITE MODELOM)")
    print("="*50)
    print(f"Accuracy  : {acc*100:.2f} %")
    print(f"Precision : {prec:.4f}")
    print(f"Recall    : {rec:.4f}")
    print(f"F1-score  : {f1:.4f}")
    print(f"MCC       : {mcc:.4f}")
    print("-" * 50)
    print("Matica zámien (Confusion Matrix):")
    if cm.shape == (2, 2):
        print(f"TN (Správny Hluk)    : {cm[0][0]}  |  FP (Falošný Poplach): {cm[0][1]}")
        print(f"FN (Prehliadnutý)    : {cm[1][0]}  |  TP (Správny Výstrel): {cm[1][1]}")
    else:
        print(cm)
    print("="*50)

if __name__ == '__main__':
    main()