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
- [x] Implementovat StandardScaler škálování v `src/firmware/Src/svm_classifier.c`.
- [x] Implementovat lineární rozhodovací funkci (skalární součin w * x + b).
- [x] Propojit výstup MFCC s touto SVM funkcí.

## Ověření
- [x] C model dává identické predikce jako Python model pro testovací vektory (10/10 PASS).
- [x] Celý C pipeline úspěšně klasifikuje testovací data v simulaci (spuštěno dsp_test.exe).
