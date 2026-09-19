import os
import numpy as np
import matplotlib.pyplot as plt
from   scipy.fftpack import dct
import utils

# --- NASTAVENIA ---
input_folder  = 'nongunshots'      # ZMEŇ podľa potreby
output_folder = 'nongunshots_mfcc_audio_noise' # ZMEŇ podľa potreby

os.makedirs(output_folder, exist_ok=True)

EXPECTED_FRAMES = 58 # 58 riadkov * 12 stĺpcov = presne 696 hodnôt pre ESP32
NOISE_FACTOR    = 0.0316   # Intenzita šumu (čím vyššie číslo, tým silnejší šum)

# --- POMOCNÉ FUNKCIE ---
def add_white_noise(signal, noise_factor):
    """Primieša náhodný biely šum do surového audio signálu."""
    noise = np.random.normal(0, signal.std(), signal.size)
    return signal + noise_factor * noise

def fix_shape(mfcc_matrix):
    """Zabezpečí, že matica bude mať VŽDY presne 58 riadkov a 12 stĺpcov."""
    if mfcc_matrix.shape[0] < EXPECTED_FRAMES:
        # Ak je záznam krátky, doplníme nuly (ticho) na koniec
        pad_width = EXPECTED_FRAMES - mfcc_matrix.shape[0]
        return np.pad(mfcc_matrix, pad_width=((0, pad_width), (0, 0)), mode='constant')
    elif mfcc_matrix.shape[0] > EXPECTED_FRAMES:
        # Ak je záznam dlhý, odrežeme prebytočný koniec
        return mfcc_matrix[:EXPECTED_FRAMES, :]
    return mfcc_matrix

def compute_mfcc(signal, sampling_rate):
    """Obalená logika tvojho výpočtu pre čistejší kód."""
    peak_index = utils.find_peak(signal)
    windowed_signal = utils.extract_window(signal, sampling_rate, peak_index)
    frames = utils.framing(windowed_signal, sampling_rate)
    mag_frames, pow_frames = utils.spectrum(frames, NFFT=512)
    filter_banks = utils.mel_filterbank(pow_frames, sampling_rate, nfilt=40, NFFT=512)
    mfcc = dct(filter_banks, type=2, axis=1, norm='ortho')[:, :12]
    return fix_shape(mfcc) # Rovno vrátime opravený rozmer

# --- HLAVNÝ CYKLUS ---
def main():
    for filename in os.listdir(input_folder):
        if filename.endswith('.wav'):
            filepath = os.path.join(input_folder, filename)

            # 1. Načítanie signálu
            signal, sampling_rate = utils.load_signal(filepath)

            # 2. VÝPOČET PRE ČISTÝ SIGNÁL
            mfcc_clean = compute_mfcc(signal, sampling_rate)
            
            clean_save_path = os.path.join(output_folder, filename.replace('.wav', '_mfcc.npy'))
            np.save(clean_save_path, mfcc_clean)

            # 3. VÝPOČET PRE ZAŠUMENÝ SIGNÁL
            # Vytvoríme zašumenú kópiu signálu
            noisy_signal = add_white_noise(signal, NOISE_FACTOR)
            mfcc_noisy = compute_mfcc(noisy_signal, sampling_rate)
            
            noisy_save_path = os.path.join(output_folder, filename.replace('.wav', '_noisy_mfcc.npy'))
            np.save(noisy_save_path, mfcc_noisy)
            
            print(f"✅ Uložené čisté aj zašumené MFCC pre: {filename}")

if __name__ == '__main__':
    main()