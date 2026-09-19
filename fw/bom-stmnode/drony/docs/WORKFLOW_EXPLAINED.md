# Průvodce workflow: Akustická detekce dronů

Tento dokument vysvětluje logickou posloupnost skriptů a nástrojů, které jsme vytvořili pro vývoj detekčního systému na STM32H5.

---

## 🏗️ Celková architektura
Systém funguje v řetězci:
**Audio (16kHz)** → **MFCC Extrakce** (13 koeficientů) → **Agregace** (Průměr + Std) → **SVM Klasifikace** → **Rozhodnutí** (Dron/Hluk).

---

## 🐍 Fáze 1: Analýza a Trénování (Python)

V této fázi připravujeme "mozek" systému.

1.  **`mfcc_analyzer.py`**
    *   **Co dělá:** Základní třída pro výpočet MFCC.
    *   **Proč je důležitá:** Je nastavena tak, aby se přesně shodovala s parametry knihovny CMSIS-DSP ve firmwaru (FFT size 1024, Hamming window atd.).

2.  **`inspect_parquet.py`**
    *   **Co dělá:** Načte stažené `.parquet` soubory, vypíše metadata a uloží ukázky do `.wav`.
    *   **Využití:** Kontrola, zda jsou data v pořádku (16kHz, mono).

3.  **`train_svm.py`**
    *   **Co dělá:** Hlavní trénovací skript. Načte data, spočítá MFCC, agreguje je do 26-dimenzionálního vektoru a natrénuje SVM model.
    *   **Výstup:** Soubor `models/drone_detector_svm.pkl`.

4.  **`validate_on_new_data.py`**
    *   **Co dělá:** Testuje natrénovaný `.pkl` model na úplně nových datech, která nebyla při trénování.
    *   **Výsledek:** Potvrdili jsme úspěšnost > 99 %.

---

## 🌉 Fáze 2: Most mezi Pythonem a C

Převod inteligence z PC do mikrokontroléru.

5.  **`export_model.py`**
    *   **Co dělá:** "Rozebere" natrénovaný SVM model na základní čísla (support vektory, koeficienty).
    *   **Výstup:** `src/firmware/Inc/svm_model_data.h` – statická pole, která STM32 prostě načte do paměti.

6.  **`generate_parity_test.py`**
    *   **Co dělá:** Vygeneruje umělý zvuk a spočítá pro něj MFCC v Pythonu.
    *   **Výstup:** `src/firmware/Inc/mfcc_parity_data.h`. Slouží k tomu, aby firmware mohl porovnat svůj výpočet s "pravdou" z Pythonu.

---

## ⚙️ Fáze 3: Firmware a Verifikace (C)

Implementace algoritmů tak, aby běžely efektivně na čipu.

7.  **`svm_classifier.c` / `.h`**
    *   **Co dělá:** Čistá C implementace SVM algoritmu. Počítá matematickou operaci (RBF kernel), která určí, jak moc se aktuální zvuk podobá naučeným vzorům dronů.

8.  **`mfcc_processor.c` / `.h`**
    *   **Co dělá:** Wrapper nad knihovnou CMSIS-DSP. Provádí transformaci zvuku na MFCC koeficienty.

9.  **`test_svm_logic.c`**
    *   **Co dělá:** Testovací program pro PC (GCC). Vezme reálné vektory z datasetu a prožene je skrze `svm_classifier.c`.
    *   **Výsledek:** Ověřili jsme, že C verze dává stejné výsledky jako Python (10/10 PASS).

---

## 🛠️ Automatizace přes Taskfile

Místo psaní dlouhých příkazů používáme `npx @go-task/cli <task>`:

*   `setup`: Instalace Python knihoven.
*   `train`: Spustí trénování.
*   `export`: Vygeneruje C hlavičku s modelem.
*   `gen-parity`: Připraví data pro test parity.
*   `download-cmsis`: Stáhne ARM knihovny z GitHubu.

---

## 🚀 Jak to jde za sebou (Workflow)
1. **Data:** Máme Parquet soubory.
2. **Trénink:** `task train` -> vznikne model.
3. **Export:** `task export` -> model je v C hlavičce.
4. **Implementace:** Máme SVM v C, které ty váhy používá.
5. **Příprava hardwaru:** (Tvůj úkol v MX) -> Nastavení mikrofonu.
6. **Běh:** Firmware v reálném čase krmí MFCC data do SVM a rozsvěcí LED při detekci.
