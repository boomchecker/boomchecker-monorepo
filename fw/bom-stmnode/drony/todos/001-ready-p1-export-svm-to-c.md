---
id: "001"
status: "complete"
priority: "p1"
description: "Export SVM modelu do C hlavičky"
dependencies: []
---

# Export SVM modelu do C hlavičky

## Cíl
Převést natrénovaný model `models/drone_detector_svm.pkl` do formátu použitelného v C firmwaru (statické pole/struktury).

## Úkoly
- [x] Vytvořit exportní skript `src/analysis/export_model.py`.
- [x] Extrahovat koeficienty (dual_coef_), support vektory (support_vectors_) a bias (intercept_).
- [x] Vygenerovat soubor `src/firmware/Inc/svm_model_data.h`.

## Ověření
- [x] Soubor `.h` je vygenerován a obsahuje všechna potřebná data.
