import pandas as pd
import numpy as np
import io
import wave
import os
import joblib
from mfcc_analyzer import MFCCAnalyzer

def generate_svm_test_vectors(output_header, n_samples_per_class=5):
    print("Generování testovacích vektorů z parquetů...")
    
    analyzer = MFCCAnalyzer(sample_rate=16000)
    
    # Načtení dat (Drony a Hluk)
    df_drone = pd.read_parquet('train-00038-of-00039.parquet').sample(n_samples_per_class, random_state=1)
    df_noise = pd.read_parquet('train-00003-of-00039.parquet')
    df_noise = df_noise[df_noise['label'] == 0].sample(n_samples_per_class, random_state=1)
    
    test_vectors = []
    expected_labels = []
    
    for df in [df_drone, df_noise]:
        for _, row in df.iterrows():
            audio_bytes = row['audio']['bytes']
            label = row['label']
            
            with wave.open(io.BytesIO(audio_bytes), 'rb') as f:
                raw_audio = f.readframes(f.getnframes())
                audio_np = np.frombuffer(raw_audio, dtype=np.int16).astype(np.float32) / 32768.0
            
            if len(audio_np) < 1024: continue
                
            mfccs = analyzer.extract_features(audio_np)
            # Agregace: mean + std
            mfcc_mean = np.mean(mfccs, axis=1)
            mfcc_std = np.std(mfccs, axis=1)
            feature_vector = np.concatenate((mfcc_mean, mfcc_std))
            
            test_vectors.append(feature_vector)
            expected_labels.append(label)

    # Zápis do C hlavičky
    with open(output_header, 'w') as f:
        f.write("#ifndef SVM_TEST_VECTORS_H\n")
        f.write("#define SVM_TEST_VECTORS_H\n\n")
        f.write(f"#define NUM_TEST_VECTORS {len(test_vectors)}\n")
        f.write(f"#define VECTOR_SIZE {len(test_vectors[0])}\n\n")
        
        f.write("static const float svm_test_vectors[NUM_TEST_VECTORS][VECTOR_SIZE] = {\n")
        for i, vec in enumerate(test_vectors):
            f.write("    {" + ", ".join([f"{v}f" for v in vec]) + "}" + ("," if i < len(test_vectors)-1 else "") + "\n")
        f.write("};\n\n")
        
        f.write("static const int svm_expected_labels[NUM_TEST_VECTORS] = {\n")
        f.write("    " + ", ".join([str(l) for l in expected_labels]) + "\n")
        f.write("};\n\n")
        
        f.write("#endif\n")
    
    print(f"Hotovo. Vektory uloženy do {output_header}")

if __name__ == "__main__":
    generate_svm_test_vectors('src/firmware/Inc/svm_test_vectors.h')
