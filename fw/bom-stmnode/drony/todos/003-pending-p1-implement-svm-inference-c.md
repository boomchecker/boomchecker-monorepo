---
id: "003"
status: "complete"
priority: "p1"
description: "Implementace SVM inference v C"
dependencies: ["001", "002"]
---

# Implementace SVM inference v C

## Cíl
Napsat efektivní funkci v C pro klasifikaci příznaků pomocí exportovaného SVM modelu.

## Úkoly
- [x] Implementovat výpočet RBF jádra v `src/firmware/Src/svm_classifier.c`.
- [x] Implementovat rozhodovací funkci (sumace dual_coef * kernel + intercept).
- [x] Propojit výstup MFCC s touto SVM funkcí.

## Ověření
- [x] Firmware úspěšně klasifikuje testovací data v simulaci (připraven main_dsp_test.c).
