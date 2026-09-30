import pandas as pd
import numpy as np
import io
import wave
import os
import joblib
from mfcc_analyzer import MFCCAnalyzer

def generate_svm_test_vectors(output_header, model_path, scaler_path, n_samples_per_class=5):
    print("Generating test vectors for C SVM logic validation...")
    
    if not os.path.exists(model_path) or not os.path.exists(scaler_path):
        print("Error: Model or Scaler not found.")
        return
        
    clf = joblib.load(model_path)
    scaler = joblib.load(scaler_path)
    analyzer = MFCCAnalyzer(sample_rate=16000)
    
    # Load Parquet Data (Drone and Noise)
    df_drone = pd.read_parquet('wav/train-00038-of-00039.parquet').sample(n_samples_per_class, random_state=1)
    df_noise = pd.read_parquet('wav/train-00003-of-00039.parquet')
    df_noise = df_noise[df_noise['label'] == 0].sample(n_samples_per_class, random_state=1)
    
    test_vectors = []
    expected_labels = []
    
    for df in [df_drone, df_noise]:
        for _, row in df.iterrows():
            audio_bytes = row['audio']['bytes']
            
            with wave.open(io.BytesIO(audio_bytes), 'rb') as f:
                raw_audio = f.readframes(f.getnframes())
                audio_np = np.frombuffer(raw_audio, dtype=np.int16).astype(np.float32) / 32768.0
            
            if len(audio_np) < 1024:
                continue
                
            mfccs = analyzer.extract_features(audio_np)
            # Aggregate: mean + std
            mfcc_mean = np.mean(mfccs, axis=1)
            mfcc_std = np.std(mfccs, axis=1)
            feature_vector = np.concatenate((mfcc_mean, mfcc_std))
            
            # Predict using scaled features in Python
            scaled_vector = scaler.transform([feature_vector])[0]
            pred = int(clf.predict([scaled_vector])[0])
            
            test_vectors.append(feature_vector)
            expected_labels.append(pred)

    # Write to C header
    with open(output_header, 'w') as f:
        f.write("#ifndef SVM_TEST_VECTORS_H\n")
        f.write("#define SVM_TEST_VECTORS_H\n\n")
        f.write(f"#define NUM_TEST_VECTORS {len(test_vectors)}\n")
        f.write(f"#define VECTOR_SIZE {len(test_vectors[0])}\n\n")
        
        f.write("static const float svm_test_vectors[NUM_TEST_VECTORS][VECTOR_SIZE] = {\n")
        for i, vec in enumerate(test_vectors):
            f.write("    {" + ", ".join([f"{v:.8e}f" for v in vec]) + "}" + ("," if i < len(test_vectors)-1 else "") + "\n")
        f.write("};\n\n")
        
        f.write("static const int svm_expected_labels[NUM_TEST_VECTORS] = {\n")
        f.write("    " + ", ".join([str(l) for l in expected_labels]) + "\n")
        f.write("};\n\n")
        
        f.write("#endif\n")
    
    print(f"Test vectors successfully written to: {output_header}")

if __name__ == "__main__":
    generate_svm_test_vectors(
        output_header='src/firmware/Inc/svm_test_vectors.h',
        model_path='models/drone_detector_svm.pkl',
        scaler_path='models/scaler.pkl'
    )
