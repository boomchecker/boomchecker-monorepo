import os
import numpy as np
import tensorflow as tf
from sklearn.metrics import accuracy_score, precision_score, recall_score, f1_score, matthews_corrcoef, confusion_matrix

# --- NASTAVENIA ---
MODEL_PATH = 'najlepsi_model.h5'

# Slovník: Priečinok -> Cieľová trieda (Label)
# 1 = Výstrel (DANA), 0 = Hluk (Non-gunshot)
DIRS_AND_LABELS = {
    'gunshots_mfcc_audio_noise': 1,
    'gunshots_nonDana_mfcc_audio_noise': 0,
    'nongunshots_mfcc_audio_noise': 0
}

def main():
    print(f"Načítavam model: {MODEL_PATH}")
    try:
        model = tf.keras.models.load_model(MODEL_PATH)
    except Exception as e:
        print(f"Chyba pri načítaní modelu: {e}")
        return
    
    X_data = []
    y_true = []
    
    print("Načítavam .npy súbory z priečinkov...")
    
    # 1. Načítanie všetkých dát
    for directory, label in DIRS_AND_LABELS.items():
        if not os.path.exists(directory):
            print(f"UPOZORNENIE: Priečinok '{directory}' neexistuje, preskakujem ho...")
            continue
            
        files_loaded = 0
        for filename in os.listdir(directory):
            if filename.endswith('.npy'):
                filepath = os.path.join(directory, filename)
                
                # Načíta maticu a upraví tvar na (58, 12, 1) pre konvolučnú sieť
                mfcc = np.load(filepath)
                mfcc_cnn = np.expand_dims(mfcc, axis=-1) 
                
                X_data.append(mfcc_cnn)
                y_true.append(label)
                files_loaded += 1
                
        print(f" -> Z priečinka '{directory}' načítaných {files_loaded} vzoriek (Trieda: {label}).")

    if not X_data:
        print("Neboli nájdené žiadne platné .npy dáta na evaluáciu.")
        return

    # 2. Hromadná predikcia
    print("\nSpúšťam klasifikáciu...")
    X_array = np.array(X_data)
    y_true_array = np.array(y_true)
    
    y_pred_probs = model.predict(X_array, verbose=1)
    
    # Prahovanie (všetko >= 0.5 je Výstrel/Trieda 1)
    y_pred_array = (y_pred_probs >= 0.5).astype(int).flatten()
    
    # 3. Výpočet metrík
    acc = accuracy_score(y_true_array, y_pred_array)
    prec = precision_score(y_true_array, y_pred_array, zero_division=0)
    rec = recall_score(y_true_array, y_pred_array, zero_division=0)
    f1 = f1_score(y_true_array, y_pred_array, zero_division=0)
    mcc = matthews_corrcoef(y_true_array, y_pred_array)
    cm = confusion_matrix(y_true_array, y_pred_array)
    
    # 4. Formátovaný výpis (Pripravené pre LaTeX tabuľku)
    print("\n" + "="*50)
    print(" FINÁLNE VÝSLEDKY EVALUÁCIE (AUDIO ŠUM)")
    print("="*50)
    print(f"Otestovaných súborov celkovo : {len(y_true_array)}")
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