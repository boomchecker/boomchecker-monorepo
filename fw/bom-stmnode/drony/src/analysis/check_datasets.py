import pandas as pd
import io
import wave
import os
import numpy as np
from mfcc_analyzer import MFCCAnalyzer

files = ['train-00038-of-00039.parquet', 'train-00003-of-00039.parquet']

for file in files:
    print(f"\n--- Analýza souboru: {file} ---")
    df = pd.read_parquet(file)
    print(f"Počet záznamů: {len(df)}")
    print(f"Distribuce labelů: {df['label'].value_counts().to_dict()}")
    
    first_audio_data = df.iloc[0]['audio']['bytes']
    with wave.open(io.BytesIO(first_audio_data), 'rb') as f:
        print(f"Metadata: {f.getframerate()} Hz, {f.getnchannels()} ch, {f.getsampwidth()} bytes")
        print(f"Délka: {f.getnframes()/f.getframerate():.2f} s")

# Uložení ukázky hluku
os.makedirs('data/samples', exist_ok=True)
df_noise = pd.read_parquet('train-00003-of-00039.parquet')
with open('data/samples/sample_noise_label_0.wav', 'wb') as f:
    f.write(df_noise.iloc[0]['audio']['bytes'])
print("\nUložen vzorek hluku: data/samples/sample_noise_label_0.wav")
