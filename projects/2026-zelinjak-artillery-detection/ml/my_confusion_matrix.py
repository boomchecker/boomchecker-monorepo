import numpy as np
from sklearn.metrics import classification_report, confusion_matrix
from evaluation import *
from sklearn.metrics import roc_curve, auc
import matplotlib.pyplot as plt
from sklearn.metrics import matthews_corrcoef

def evaluate_model_at_various_snr(model, X_test_clean, y_test):
    print("\n" + "="*50)
    print(" KOMPLEXNÁ ANALÝZA: MATICE ZÁMIEN PRE VŠETKY SNR ")
    print("="*50)

    plt.close('all')   # Zatvorí všetky zabudnuté grafy z predchádzajúceho kódu!
    plt.clf()


    snr_levels = [None, 30, 20, 10, 5, 0]


    for snr in snr_levels:  
        if snr is None:
            print("\n" + "#"*10 + " ČISTÉ DÁTA (SNR > 30 dB) " + "#"*10)
        else:
            print(f"\n" + "#"*10 + f" ÚROVEŇ ŠUMU: {snr} dB " + "#"*10)
        
        acc, X_test_current = evaluate_with_noise(model, X_test_clean, y_test, snr)
        print(f"Celková úspešnosť (Accuracy): {acc*100:.2f}%")

        y_pred_prob = model.predict(X_test_current, verbose=0)
        y_pred_classes = (y_pred_prob > 0.5).astype(int)

        mcc = matthews_corrcoef(y_test, y_pred_classes)
        print("\nDetailný report:")
        print(classification_report(y_test, y_pred_classes, 
                                    target_names=["Iný hluk (0)", "DANA (1)"], 
                                    zero_division=0))
        print(f">>> MCC (Matthews Correlation Coefficient): {mcc:.4f} <<<")
        print("-" * 50)
    
