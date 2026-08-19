import os
import numpy as np

# Nastavenie ciest k dátam
PATH_GUNSHOT        = 'gunshots_mfcc'           # Trieda 1 (152 mm DANA)
PATH_NONGUNSHOT     = 'nongunshots_mfcc'        # Trieda 0 (Vietor, ticho, bežný hluk)
PATH_HARD_NEGATIVES = 'gunshots_nonDana_mfcc'   # Trieda 0 (Iné zbrane, AK-47, pištole)

NOISE_LEVELS = [0.1, 0.2, 0.3, 0.5]  # Rôzne sily šumu pre trénovanie


def add_noise_to_mfcc(mfcc, noise_level):
    """Pridá Gaussovský šum do MFCC matice."""
    # Vypočítame smerodajnú odchýlku signálu, aby bol šum proporčný
    signal_std = np.std(mfcc)
    noise = np.random.normal(0, signal_std * noise_level, mfcc.shape)
    return mfcc + noise


def load_data():
    features = []
    labels = []
    
    def process_folder(path, label_value):
        count = 0
        for filename in os.listdir(path):
            if filename.endswith('.npy'):
                filepath = os.path.join(path, filename)
                try:
                    original_data = np.load(filepath)
                    if original_data.shape == (58, 12):
                        # 1. Originál
                        features.append(original_data)
                        labels.append(label_value)
                        
                        
                        count += 1
                        
                except: pass

        return count

    # 1. Načítame Danu ako JEDNOTKY
    c_dana = process_folder(PATH_GUNSHOT, 1)
    
    # 2. Načítame bežný hluk ako NULY
    c_noise = process_folder(PATH_NONGUNSHOT, 0)
    
    # 3. Načítame iné zbrane tiež ako NULY! (Toto je to kúzlo)
    c_hard = process_folder(PATH_HARD_NEGATIVES, 0)
    
    print(f"Načítané: {c_dana}x DANA (Trieda 1)")
    print(f"Načítané: {c_noise}x Bežný hluk, {c_hard}x Iné zbrane (Spolu Trieda 0)")


    X = np.array(features)
    y = np.array(labels)
    X = X[..., np.newaxis] # (N, 58, 12) -> (N, 58, 12, 1)
    return X, y