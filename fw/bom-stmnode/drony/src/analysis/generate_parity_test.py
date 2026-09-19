import numpy as np
import os
from mfcc_analyzer import MFCCAnalyzer

def generate_mfcc_test_data(output_header):
    print("Generování testovacích dat pro validaci MFCC parity...")
    
    fs = 16000
    duration = 0.5 # s
    n_samples = int(fs * duration)
    
    # Vytvoření testovacího signálu: Sine sweep + šum
    t = np.linspace(0, duration, n_samples, endpoint=False)
    # Sweep od 100Hz do 4000Hz
    signal = np.sin(2 * np.pi * (100 + 3900 * t**2) * t)
    # Přidání trochy šumu
    signal += 0.1 * np.random.randn(n_samples)
    # Normalizace na rozsah int16 pro simulaci mikrofonu, pak zpět na float32
    signal_int16 = (signal * 32767).astype(np.int16)
    signal_float = signal_int16.astype(np.float32) / 32768.0

    # Výpočet MFCC v Pythonu
    analyzer = MFCCAnalyzer(sample_rate=fs)
    mfcc_ref = analyzer.extract_features(signal_float)
    
    n_mfcc, n_frames = mfcc_ref.shape
    
    with open(output_header, 'w') as f:
        f.write("#ifndef MFCC_PARITY_DATA_H\n")
        f.write("#define MFCC_PARITY_DATA_H\n\n")
        
        f.write(f"#define MFCC_TEST_SIGNAL_LEN {n_samples}\n")
        f.write(f"#define MFCC_TEST_NUM_FRAMES {n_frames}\n")
        f.write(f"#define MFCC_TEST_NUM_COEFFS {n_mfcc}\n\n")
        
        # Testovací signál
        f.write("static const float mfcc_test_signal[MFCC_TEST_SIGNAL_LEN] = {\n")
        f.write("    " + ", ".join([f"{v}f" for v in signal_float]) + "\n")
        f.write("};\n\n")
        
        # Referenční MFCC (zploštělé po rámcích: frame0[c0..c12], frame1[c0..c12], ...)
        f.write("static const float mfcc_reference_output[MFCC_TEST_NUM_FRAMES][MFCC_TEST_NUM_COEFFS] = {\n")
        for frame in range(n_frames):
            f.write("    {")
            f.write(", ".join([f"{v}f" for v in mfcc_ref[:, frame]]))
            f.write("}" + ("," if frame < n_frames - 1 else "") + "\n")
        f.write("};\n\n")
        
        f.write("#endif // MFCC_PARITY_DATA_H\n")
        
    print(f"Testovací data uložena do: {output_header}")
    print(f"Počet rámců: {n_frames}, MFCC koeficientů: {n_mfcc}")

if __name__ == "__main__":
    header_path = 'src/firmware/Inc/mfcc_parity_data.h'
    os.makedirs(os.path.dirname(header_path), exist_ok=True)
    generate_mfcc_test_data(header_path)
