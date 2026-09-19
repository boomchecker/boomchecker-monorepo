import numpy as np
from load_data import *

def augment_train_data(X_train, y_train):
    aug_features, aug_labels = [], []
    
    for i in range(len(X_train)):
        original = X_train[i, :, :, 0] # Získame pôvodnú 2D maticu
        
        # Pridáme originál do novej sady
        aug_features.append(original)
        aug_labels.append(y_train[i])
        
        # Pridáme zašumené verzie
        for level in NOISE_LEVELS:
            noisy_mfcc = add_noise_to_mfcc(original, level)
            aug_features.append(noisy_mfcc)
            aug_labels.append(y_train[i])
            
    X_aug = np.array(aug_features)
    y_aug = np.array(aug_labels)
    return X_aug[..., np.newaxis], y_aug        