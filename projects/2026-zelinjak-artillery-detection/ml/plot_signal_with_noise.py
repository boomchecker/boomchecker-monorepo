import numpy as np
import matplotlib.pyplot as plt
from scipy.io import wavfile

def add_noise_time_domain(signal, snr_db):
    """
    Pridá biely Gaussov šum do signálu v časovej oblasti na základe SNR.
    """
    signal_power = np.mean(signal ** 2)
    noise_power = signal_power / (10 ** (snr_db / 10))
    noise = np.random.normal(0, np.sqrt(noise_power), len(signal))
    return signal + noise

# ==========================================
# 1. Načítanie a segmentácia REÁLNEHO .wav súboru
# ==========================================
# TU ZADAJ CESTU K TVOJMU .WAV SÚBORU
wav_path = 'gunshot.wav'

# Načítanie vzorkovacej frekvencie a dát
fs, data = wavfile.read(wav_path)
data = data.astype(np.float32)

# Implementácia segmentácie podľa Kapitoly 3.1
# Hľadáme Peak (špičku) a vystrihneme okno 60 ms (40 % pred, 60 % po)
window_len = int(0.06 * fs)  # Počet vzoriek pre 60 ms
pre_trigger = int(0.4 * window_len)

# Nájdenie indexu maximálnej amplitúdy (špičky výstrelu)
peak_idx = np.argmax(np.abs(data))

# Ošetrenie okrajov (aby sme nevystrihli mimo poľa)
start_idx = max(0, peak_idx - pre_trigger)
end_idx = min(len(data), start_idx + window_len)

# Náš čistý reálny signál (výsek 60 ms)
clean_signal = data
t = np.arange(len(clean_signal)) / fs  # Časová os pre graf

# ==========================================
# 2. Aplikácia šumu (Záťažový test)
# ==========================================
snr_levels = [20, 5, 0]
noisy_signals = [add_noise_time_domain(clean_signal, snr) for snr in snr_levels]

# ==========================================
# 3. Vykreslenie grafov
# ==========================================
fig, axes = plt.subplots(4, 1, figsize=(10, 8), sharex=True, sharey=True)

ylim_max = max(np.max(np.abs(noisy_signals[-1])), np.max(np.abs(clean_signal))) * 1.1

# Vykreslenie originálu
axes[0].plot(t * 1000, clean_signal, color='#1f77b4', linewidth=1.2)
axes[0].set_title('Nezašumený signál', fontweight='bold')
axes[0].set_ylabel('Amplitúda')
axes[0].set_ylim(-ylim_max, ylim_max)
axes[0].grid(True, linestyle='--', alpha=0.6)

# Vykreslenie zašumených verzií
colors = ['#ff7f0e', '#2ca02c', '#d62728']
for i, snr in enumerate(snr_levels):
    axes[i+1].plot(t * 1000, noisy_signals[i], color=colors[i], linewidth=1.0, alpha=0.8)
    axes[i+1].set_title(f'Zašumený signál (SNR = {snr} dB)', fontweight='bold')
    axes[i+1].set_ylabel('Amplitúda')
    axes[i+1].grid(True, linestyle='--', alpha=0.6)

axes[-1].set_xlabel('Čas [ms]')

plt.tight_layout()
plt.savefig('realny_vystrel_snr_cas.png', dpi=300, bbox_inches='tight')
print("Graf bol úspešne vygenerovaný a uložený ako 'realny_vystrel_snr_cas.png'")
plt.show()