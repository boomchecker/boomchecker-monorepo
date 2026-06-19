---
id: "002"
status: "complete"
priority: "p2"
description: "Validace parity MFCC (Python vs C)"
dependencies: ["001"]
---

# Validace parity MFCC (Python vs C)

## Cíl
Zajistit, aby výpočet MFCC v CMSIS-DSP (firmware) odpovídal výpočtu v librosa (Python).

## Úkoly
- [x] Vytvořit testovací audio buffer s definovaným signálem.
- [x] Spustit MFCC extrakci v Pythonu a uložit výsledek.
- [x] Spustit stejnou extrakci v C (pomocí testovacího wrapperu) a porovnat hodnoty.
- [x] Vyladit parametry (windowing, n_fft, hop_length) pro shodu.

## Ověření
- [x] Maximální odchylka mezi Python a C výsledky je pod 1% (Dosaženo: 7.15e-07).
