---
id: "002"
status: "ready"
priority: "p2"
description: "Validace parity MFCC (Python vs C)"
dependencies: ["001"]
---

# Validace parity MFCC (Python vs C)

## Cíl
Zajistit, aby výpočet MFCC v CMSIS-DSP (firmware) odpovídal výpočtu v librosa (Python).

## Úkoly
- [ ] Vytvořit testovací audio buffer s definovaným signálem.
- [ ] Spustit MFCC extrakci v Pythonu a uložit výsledek.
- [ ] Spustit stejnou extrakci v C (pomocí testovacího wrapperu) a porovnat hodnoty.
- [ ] Vyladit parametry (windowing, n_fft, hop_length) pro shodu.

## Ověření
- Maximální odchylka mezi Python a C výsledky je pod 1%.
