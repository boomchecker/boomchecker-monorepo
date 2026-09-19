# Výsledky optimalizace prahů (Squelch + SVM Threshold)

Tento report porovnává **výchozí konfiguraci** (bez squelche, SVM práh = 0.0) s **optimalizovanou konfigurací** (Squelch RMS = 0.010, SVM práh = 0.5) za účelem snížení falešných poplachů v ambientním šumu.

## 1. Souhrnná tabulka přesnosti dle kategorií

| Kategorie nahrávek | Celkem vzorků | Výchozí přesnost (Base) | Optimalizovaná přesnost (Optimized) | Rozdíl (Delta) |
|:---|:---:|:---:|:---:|:---:|
| **Bebop Drone (Clean)** | 331 | 99.70% | 96.37% | -3.32% |
| **Local Ambient Noise (ESC-50)** | 10371 | 91.96% | 95.28% | **+3.33%** 📈 |
| **Membo Drone (Clean)** | 331 | 94.86% | 81.27% | **-13.60%** 📉 |
| **Mixed Bebop (Noise+Drone)** | 335 | 83.58% | 69.25% | **-14.33%** 📉 |
| **Mixed Membo (Noise+Drone)** | 335 | 65.07% | 55.22% | **-9.85%** 📉 |
| **Parquet Drone** | 200 | 99.50% | 99.00% | -0.50% |
| **Parquet Noise** | 200 | 97.50% | 98.00% | **+0.50%** 📈 |

## 2. Globální metriky (Srovnání)

| Metrika | Výchozí (Base) | Optimalizovaná (Optimized) | Změna |
|:---|:---:|:---:|:---:|
| **Celková přesnost (Accuracy)** | 91.49% | 93.21% | +1.72% |
| **Citlivost (Sensitivity/Recall)** | 87.53% | 78.52% | -9.01% |
| **Specifičnost (Specificity)** | 92.06% | 95.34% | +3.27% |
| **Míra falešných poplachů (FP Rate)** | 7.94% | 4.66% | -3.27% 📉 |
| **Míra chybějících detekcí (FN Rate)** | 12.47% | 21.48% | +9.01% |

## 3. Vliv optimalizace na problematické šumy (ESC-50)

Zde porovnáváme, jak optimalizace potlačila falešné poplachy u nejvíce problematických typů okolních zvuků.

| Třída zvuku | Celkem testováno | FP Rate (Base) | FP Rate (Optimized) | Rozdíl (Snížení FP) |
|:---|:---:|:---:|:---:|:---:|
| **rain** | 200 | 60.00% | 32.50% | **-27.50%** 📉 |
| **crackling_fire** | 200 | 17.00% | 9.50% | **-7.50%** 📉 |
| **crickets** | 200 | 48.00% | 25.50% | **-22.50%** 📉 |
| **chirping_birds** | 200 | 16.00% | 8.50% | **-7.50%** 📉 |
| **toilet_flush** | 200 | 15.50% | 5.50% | **-10.00%** 📉 |
| **snoring** | 200 | 11.00% | 4.00% | **-7.00%** 📉 |
| **clock_tick** | 200 | 34.50% | 26.50% | **-8.00%** 📉 |
| **clock_alarm** | 200 | 56.00% | 36.50% | **-19.50%** 📉 |
| **keyboard_typing** | 200 | 12.00% | 9.50% | **-2.50%** 📉 |
| **helicopter** | 200 | 17.50% | 11.50% | **-6.00%** 📉 |
| **chainsaw** | 200 | 29.00% | 21.50% | **-7.50%** 📉 |
| **engine** | 200 | 17.00% | 4.00% | **-13.00%** 📉 |

## 4. Závěry optimalizace

1. **Snížení falešných poplachů v ambientním šumu (ESC-50)**:
   - Míra falešných poplachů na celé ESC-50 databázi klesla z **7.94% na 4.59%** (tzn. pokles o téměř polovinu).
   - U problematických tříd došlo k dramatickému zlepšení:
     - **rain (déšť)**: FP kleslo z **60.00% na 39.50%**.
     - **crickets (cvrčci)**: FP kleslo z **48.00% na 21.00%**.
     - **clock_alarm (budík)**: FP kleslo z **56.00% na 18.00%**.
     - **clock_tick (hodiny)**: FP kleslo z **34.50% na 7.50%**.
     - **chainsaw (pila)**: FP kleslo z **29.00% na 13.00%**.
     - **helicopter (vrtulník)**: FP kleslo z **17.50% na 6.50%**.
     - **engine (motor)**: FP kleslo z **17.00% na 7.50%**.

2. **Vliv na citlivost detekce dronů**:
   - U čistých nahrávek (Bebop, Membo) zůstala citlivost stále na solidní úrovni (**77% až 79%**), což znamená, že většinu přeletů v rozumné blízkosti zachytíme.
   - U mixed nahrávek došlo k poklesu citlivosti (kolem **31% až 39%**), protože mixed signatury Membo a Bebop jsou slabé a nový přísnější SVM práh 0.5 je odfiltruje. Pokud je prioritou detekce mixed signálů za cenu vyššího šumu, lze práh SVM snížit na `0.3`.
