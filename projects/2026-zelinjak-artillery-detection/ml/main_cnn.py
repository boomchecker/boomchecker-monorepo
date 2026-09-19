
import numpy as np
import tensorflow as tf
from sklearn.model_selection import train_test_split
import matplotlib.pyplot as plt
from sklearn.metrics import classification_report, confusion_matrix

from load_data import *
from model import *
from add_noise import *
from evaluation import *
from my_confusion_matrix import *

INPUT_SHAPE = (58, 12, 1)           # Rozmery vstupných dát


# Spustenie načítania
X_clean, y_clean = load_data()

# Rozdelenie na Trénovaciu a Testovaciu množinu (80% tréning, 20% test)
# B) Striktný Split (Rozdelenie 80 / 20) na čistých dátach
X_train_clean, X_test_clean, y_train, y_test = train_test_split(
    X_clean, y_clean, test_size=0.2, random_state=42, stratify=y_clean
)

# C) Augmentujeme IBA trénovaciu množinu (X_train_clean sa zväčší 5-násobne)
X_train_aug, y_train_aug = augment_train_data(X_train_clean, y_train)
print(f"Trénovacia množina (augmentovaná): {len(X_train_aug)} vzoriek")
print(f"Testovacia množina: {len(X_test_clean)} vzoriek")


# Vybudovanie modelu
model = build_baseline_cnn(INPUT_SHAPE)
model.summary()

class_weights = {
    0: 1.0,   # Hluk má normálnu váhu
    1: 4.0    # DANA je 10x dôležitejšia!
}

# Trénovanie
history = model.fit(X_train_aug, y_train_aug,
                    epochs=50,           # Počet cyklov (pre začiatok stačí málo)
                    batch_size=8,        # Malý batch size, lebo máme málo dát
                    validation_data=(X_test_clean, y_test),
                    class_weight=class_weights,
                    verbose=1)

# Uloženie grafu        
plt.figure(figsize=(10, 5))
plt.plot(history.history['accuracy'], label='Train')
plt.plot(history.history['val_accuracy'], label='Val')
plt.title('Pretrénovanie na augmentovaných dátach')
plt.legend()
plt.savefig('graf_augmentovany.png')


# --- POROVNANIE VÝSLEDKOV ---
print("\n=== VÝSLEDKY AUGMENTOVANÉHO MODELU ===")

evaluate_model_at_various_snr(model, X_test_clean, y_test)


clean_acc = model.evaluate(X_test_clean, y_test, verbose=0)[1]

model.save("najlepsi_model.h5")




