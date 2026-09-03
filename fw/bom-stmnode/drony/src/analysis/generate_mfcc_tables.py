import numpy as np
import librosa
import scipy.fftpack
import os

def generate_tables(output_header):
    print("Generating MFCC tables for C...")
    
    # Parameters matching dsp_config.h
    fs = 16000
    n_fft = 1024
    n_mels = 20
    n_mfcc = 13
    fmin = 0.0
    fmax = 8000.0
    
    # 1. Hamming Window
    # CMSIS-DSP: window_func[i] = 0.54f - 0.46f * cosf(2.0f * PI * i / (WINDOW_SIZE - 1))
    # This is standard Hamming window.
    window = 0.54 - 0.46 * np.cos(2.0 * np.pi * np.arange(n_fft) / (n_fft - 1))
    
    # 2. Mel Filterbank (from librosa)
    # Shape is (n_mels, n_fft // 2 + 1) -> (20, 513)
    mel_fb = librosa.filters.mel(sr=fs, n_fft=n_fft, n_mels=n_mels, fmin=fmin, fmax=fmax)
    
    # CMSIS-DSP sparse filterbank representations
    filter_pos = []
    filter_lengths = []
    filter_coefs = []
    
    for i in range(n_mels):
        row = mel_fb[i]
        # Find start and end indices of non-zero elements
        non_zero_indices = np.where(row > 0)[0]
        if len(non_zero_indices) == 0:
            start_idx = 0
            length = 0
            coefs = []
        else:
            start_idx = non_zero_indices[0]
            end_idx = non_zero_indices[-1]
            length = end_idx - start_idx + 1
            coefs = row[start_idx : end_idx + 1]
            
        filter_pos.append(start_idx)
        filter_lengths.append(length)
        filter_coefs.extend(coefs)
        
    # 3. DCT Matrix (orthogonal DCT-II)
    # scipy.fftpack.dct on identity matrix is a standard way to get the DCT matrix
    dct_mat = scipy.fftpack.dct(np.eye(n_mels), type=2, axis=0, norm='ortho')[:n_mfcc]
    
    # Write to C Header
    with open(output_header, 'w') as f:
        f.write("#ifndef MFCC_TABLES_H\n")
        f.write("#define MFCC_TABLES_H\n\n")
        f.write("// Automatically generated MFCC tables matching Librosa configuration\n\n")
        
        # Window
        f.write(f"#define MFCC_WINDOW_LEN {n_fft}\n")
        f.write("static const float mfcc_window_coefs[MFCC_WINDOW_LEN] = {\n")
        f.write("    " + ", ".join([f"{v:.8e}f" for v in window]) + "\n")
        f.write("};\n\n")
        
        # Sparse Mel Filters
        f.write(f"#define MFCC_NUM_MEL_FILTERS {n_mels}\n")
        f.write(f"#define MFCC_COEFS_LEN {len(filter_coefs)}\n\n")
        
        f.write("static const uint32_t mfcc_filter_pos[MFCC_NUM_MEL_FILTERS] = {\n")
        f.write("    " + ", ".join([str(v) for v in filter_pos]) + "\n")
        f.write("};\n\n")
        
        f.write("static const uint32_t mfcc_filter_lengths[MFCC_NUM_MEL_FILTERS] = {\n")
        f.write("    " + ", ".join([str(v) for v in filter_lengths]) + "\n")
        f.write("};\n\n")
        
        f.write("static const float mfcc_filter_coefs[MFCC_COEFS_LEN] = {\n")
        f.write("    " + ", ".join([f"{v:.8e}f" for v in filter_coefs]) + "\n")
        f.write("};\n\n")
        
        # DCT Matrix
        f.write(f"#define MFCC_DCT_ROWS {n_mfcc}\n")
        f.write(f"#define MFCC_DCT_COLS {n_mels}\n\n")
        
        f.write("static const float mfcc_dct_coefs[MFCC_DCT_ROWS][MFCC_DCT_COLS] = {\n")
        for i in range(n_mfcc):
            f.write("    {")
            f.write(", ".join([f"{v:.8e}f" for v in dct_mat[i]]))
            f.write("}" + ("," if i < n_mfcc - 1 else "") + "\n")
        f.write("};\n\n")
        
        f.write("#endif // MFCC_TABLES_H\n")
        
    print(f"Successfully generated {output_header}")

if __name__ == "__main__":
    generate_tables('src/firmware/Inc/mfcc_tables.h')
