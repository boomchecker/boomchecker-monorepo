import pandas as pd
import io
import wave
import os

# Načtení parquet souboru
df = pd.read_parquet('train-00038-of-00039.parquet')

print(f"Celkový počet záznamů: {len(df)}")
print(f"Unikátní labely: {df['label'].value_counts().to_dict()}")

# Analýza prvního záznamu
first_audio_data = df.iloc[0]['audio']['bytes']
with wave.open(io.BytesIO(first_audio_data), 'rb') as f:
    sample_rate = f.getframerate()
    channels = f.getnchannels()
    width = f.getsampwidth()
    frames = f.getnframes()
    duration = frames / float(sample_rate)
    
    print(f"\nMetadata audia:")
    print(f"  Vzorkovací frekvence: {sample_rate} Hz")
    print(f"  Kanály: {channels}")
    print(f"  Šířka vzorku: {width} bytes")
    print(f"  Délka klipu: {duration:.2f} s")

# Uložení ukázek
os.makedirs('data/samples', exist_ok=True)
for i in range(min(10, len(df))):
    label = df.iloc[i]['label']
    filename = f'data/samples/sample_{i}_label_{label}.wav'
    with open(filename, 'wb') as f_out:
        f_out.write(df.iloc[i]['audio']['bytes'])
    print(f"Uloženo: {filename}")
