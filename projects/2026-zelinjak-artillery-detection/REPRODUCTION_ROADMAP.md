# Roadmapa reprodukce: BEC2026 paper 53 (Artillery Launch Detection)

Projekt: `2026-zelinjak-artillery-detection/`
Vytvořeno: 2026-08-14 | Deadline camera-ready: **2026-08-31**

**Jak s tímto souborem pracovat:** Postupujeme striktně po milnících M0 až M6. Nezačínáme další milník, dokud předchozí nemá splněná všechna akceptační kritéria. Stav se značí v přehledové tabulce a u každého milníku (`NEZAHÁJENO` / `PROBÍHÁ` / `HOTOVO` / `BLOKOVÁNO`). Poznámky, zjištění a odchylky se zapisují do sekce "Pracovní log" na konci souboru — s datem, milníkem a závěrem.

---

## Přehled stavu

| Milník | Název | Stav | Závisí na | Odhad |
|---|---|---|---|---|
| M0 | Prostředí a základna | HOTOVO | — | 0,5 h |
| M1 | Proveniénce vah (h5 vs tflite) | HOTOVO — potvrzena větev (b), různé váhy | M0 | 1–2 h |
| M2 | Úpravy evaluačních skriptů | HOTOVO | M0 | 1–2 h |
| M3 | Multi-seed reprodukční běh | HOTOVO | M1, M2 | 2–4 h |
| M4 | Legacy proveniénční běh | HOTOVO — hypotéza bugu vyvrácena | M2 | 2–3 h |
| M5 | Deliverables | HOTOVO | M3, M4 | 3–4 h |
| M6 | ESP32-S3 hardware (volitelné) | HOTOVO — per-sample shoda <=1 LSB, Release vs Debug 0,1 % rozdíl, Flash/RAM/ops zdokumentováno | M5 + deska | 4–8 h |

Kritická cesta M0–M5: cca 10–15 h čisté práce. M1 a M2 lze dělat paralelně.

---

## 1. Kontext a cíl

### 1.1 Článek

*"A Lightweight and Robust Two-Stage Acoustic Pipeline for Embedded Artillery Launch Detection"* — **přijat** na BEC2026 (Tallinn, 6.–8. 10. 2026). Skóre recenzí: R1 accept (2), R2 weak accept (1), R3 accept (2), R4 weak accept (1, doporučuje major revision). Camera-ready verze musí zapracovat výtky recenzentů, termín 2026-08-31.

Zdroje článku: `BEC/article/article_main.tex`, PDF `BEC/BEC2026_paper_53.pdf`. Recenze verbatim: `BEC/.agents/memories/2026-07-21_bec2026_reviews_camera_ready.md`. Camera-ready plán psaní: `BEC/.agents/memories/2026-07-21_camera_ready_plan.md`.

### 1.2 Co článek měří (experimenty k replikaci)

Dvoustupňový akustický pipeline; článek evaluuje pouze druhý stupeň — **post-trigger binární klasifikátor** (výstřel z děla vs. jiný impulzní zvuk):

- **Vstup:** 60ms segment centrovaný na peak (40 % před, 60 % za peakem), 22,05 kHz.
- **Featury:** MFCC matice 58 x 12 (rámec 2,5 ms, hop 1,0 ms, Hamming, NFFT 512, 40 mel filtrů, prvních 12 DCT koeficientů).
- **Model:** LeNet-like CNN, 72 193 parametrů: Conv2D(32,3x3) + MaxPool + Conv2D(64,3x3) + MaxPool + Flatten + Dense(64, dropout 0.5) + Dense(1, sigmoid). Threshold 0,5.
- **Robustnostní protokol:** aditivní Gaussův šum ve waveform doméně (před MFCC extrakcí) na úrovních SNR 30/20/10/5 dB; metriky Accuracy, Precision, Recall, F1, MCC.
- **Tři inferenční režimy:** desktop float32 (Keras), PC int8 (TFLite), ESP32-S3 int8 (TFLite Micro, tensor arena 80 KiB, latence cca 32 ms/segment).

### 1.3 Publikovaná čísla (referenční hodnoty pro srovnání)

Tabulka II — desktop float32 (plný korpus):

| SNR (dB) | Acc (%) | Prec | Rec | F1 | MCC |
|---|---|---|---|---|---|
| 30 | 84,41 | 0,35 | 1,00 | 0,52 | 0,54 |
| 20 | 82,86 | 0,33 | 1,00 | 0,49 | 0,54 |
| 10 | 81,12 | 0,31 | 0,99 | 0,47 | 0,49 |
| 5 | 80,44 | 0,29 | 0,93 | 0,45 | 0,46 |

Tabulka III — PC int8:

| SNR (dB) | Acc (%) | Prec | Rec | F1 | MCC |
|---|---|---|---|---|---|
| 30 | 98,66 | 0,86 | 1,00 | 0,93 | 0,92 |
| 20 | 96,77 | 0,72 | 1,00 | 0,84 | 0,83 |
| 10 | 92,74 | 0,54 | 0,97 | 0,69 | 0,69 |
| 5 | 92,41 | 0,53 | 0,90 | 0,66 | 0,65 |

Tabulka III — ESP32-S3 int8:

| SNR (dB) | Acc (%) | Prec | Rec | F1 | MCC |
|---|---|---|---|---|---|
| 30 | 99,86 | 1,00 | 0,98 | 0,99 | 0,99 |
| 20 | 99,73 | 0,98 | 0,98 | 0,98 | 0,98 |
| 10 | 97,78 | 0,86 | 0,87 | 0,87 | 0,86 |
| 5 | 97,18 | 0,95 | 0,70 | 0,81 | 0,80 |

### 1.4 Známé problémy (z REPRODUCTION_NOTES.md a auditu 2026-07-21)

1. **Počet eventů:** článek uvádí 706 eventů (62 launch / 644 non-launch); manifest v repu má **854** (63 `dana_artillery` / 85 `other_gunshot` / 706 `impulse_noise`). Číslo 706 v článku odpovídá pouze počtu `impulse_noise`. Korpus 854 je referenční.
2. **Vyvráceno v M4 (2026-08-14): legacy bug NENÍ příčinou publikovaných PC-int8 čísel.** Legacy skript `ml/eval_tflite_pc.py:68` dequantizuje `(output - zero_point) * scale` bez rozšíření typu; pod numpy 2.x (NEP 50) int8 aritmetika přetéká. Pro `zero_point=-128` tohoto modelu bug matematicky způsobuje recall 0,00 na všech úrovních šumu (empiricky potvrzeno) — to je v přímém rozporu s publikovaným recall 0,90–1,00. Pokud by Tabulka III vznikla tímto skriptem v jeho aktuální podobě, čísla by vypadala úplně jinak. Hypotéza "PC vs ESP diskrepance = artefakt tohoto bugu" padá; zbývá M1 nález (různé váhy) jako hlavní vysvětlení. Detaily: `generated/reports/provenance_table3.md`.
3. **Potvrzeno v M1 (2026-08-14): různé váhy, mechanismus identifikován.** Firmware `model_data.h` (81 008 B, bit-identický s `archive/models/model.tflite`) **nevznikl** kvantizací `archive/models/najlepsi_model.h5`. Dekvantizované Conv2D/Dense kernely `model.tflite` se od float32 vah v `najlepsi_model.h5` liší o desítky až stovky kvantizačních kroků (max diff 0,40–1,25 při scale ~0,0008–0,0022) — o dva až tři řády víc než u kontrolního páru (rekonverze `najlepsi_model.h5` sama proti sobě: max diff 0,001–0,004). 15 % vzorků (127/854) má přehozenou predikci mezi oběma `.tflite` soubory. Jde o dva různé natrénované modely stejné architektury, ne o kvantizační artefakt. **Příčina:** originální trénovací skript `ml/main_cnn.py` nikde nevolá `tf.random.set_seed()` (seedovaný je jen `train_test_split`) a na konci bezpodmínečně přepisuje `najlepsi_model.h5` (`model.save(...)`, řádek 66) — dva různé běhy tohoto skriptu na identických datech tedy dají různé váhy. Že je tato citlivost velká, dokládá nezávisle i vlastní retrénink v `REPRODUCTION_NOTES.md` sekce 4 (val_accuracy 0,795 vs. 1,000 při změně jen seedu inicializace). Nejpravděpodobněji: jeden běh `main_cnn.py` dal váhy zapečené do firmware, druhý (pozdější) běh přepsal `najlepsi_model.h5` bez rekvantizace. Detaily a plný postup: `generated/reports/weights_provenance.md`.
4. **Eval na trénovacích datech:** robustnostní sweep běžel na celém korpusu (většina vzorků viděna při tréninku). Split existuje (80/20 stratified, seed 42, `datasets/recordings/splits.csv`), ale test split má jen **13 launch eventů** — held-out čísla budou statisticky slabá, nutný caveat.
5. Předchozí reprodukce (archivní model, korpus 854, opravený eval) dává stejné trendy, ale čísla o něco nižší než v článku — např. int8 full-corpus: 30 dB Acc 97,78 % / MCC 0,86; 5 dB Acc 85,71 % / MCC 0,42.

### 1.5 Výtky recenzentů a kde je řešíme

| Výtka | Recenzenti | Milník |
|---|---|---|
| PC-int8 vs ESP32-int8 diskrepance stejného modelu | R2#6, R3, R4-major1, R4-Q1 | M1 (váhy — hlavní vysvětlení), M4 (bug — vyvráceno), M6 (HW důkaz, chybí) |
| "Kvantizace zlepšuje robustnost" neprokázáno, možný artefakt preprocessingu | R3, R4-major2, R4-Q2 | M1 + M3 + M4 |
| Eval na trénovacích datech, chybí held-out výsledky | R3, R4-major3, R4-Q3 | M2 + M3 |
| Definice "acoustic SNR" | R4-Q4 | M5 (vzorec z `ml/prepare_features.py:31-36`) |
| Model size v bytech, Flash, RAM, podpora ops v TFLM | R1 III.C | M5 (z PC), M6 (z HW) |
| Latence měřena s Debug buildem, jen Invoke() | audit | M6 (Release build) |

### 1.6 Cíl této roadmapy

1. **Replikovatelnost:** celý PC experiment spustitelný jedním příkazem z čistého prostředí, deterministicky (fixní seedy, pinované závislosti).
2. **Proveniénce:** doložit, odkud se vzala publikovaná čísla (bug/váhy), jako podklad pro odpověď recenzentům.
3. **Nová camera-ready čísla:** sjednocený eval (stejný model, stejný preprocessing, opravená dequantizace), full-corpus i held-out, multi-seed mean ± std.
4. **Podklady:** LaTeX tabulky, reviewer-response dokument, HTML přehled pro spoluautory.

---

## 2. Uzamčená rozhodnutí

Dohodnuto 2026-08-14 (grill session):

| # | Otázka | Rozhodnutí | Zdůvodnění |
|---|---|---|---|
| 1 | Rozsah | PC teď; ESP32-S3 HW jako oddělený volitelný milník M6 | Deska není připojena; PC část je plně proveditelná hned |
| 2 | Model | Kanonický **archivní**: `archive/models/najlepsi_model.h5` (float32) + `archive/models/model.tflite` (int8). **Neretrénovat.** | Retrénink mění všechna publikovaná čísla = přepis celých Results; retrénovaný model je navíc méně robustní a citlivý na seed (viz REPRODUCTION_NOTES sekce 4) |
| 3 | Cílová čísla | **Obojí** — reprodukce původních publikovaných (proveniénce) i nová sjednocená camera-ready | Odpověď recenzentům potřebuje vysvětlit příčinu diskrepance, ne jen nová čísla |
| 4 | Šum / seedy | Multi-seed **42–46**, report mean ± std | Test split má jen 13 launch eventů; jedna realizace šumu je statisticky slabá |
| 5 | Prostředí | Fresh venv přes `task setup` (PyPI dostupnost ověřena) | Stávající venv měl rozbitý pip; čistá instalace z pinovaného requirements.txt |
| 6 | Deliverables | One-command harness + reviewer-response MD + LaTeX tabulky + HTML artifact | Vše čtyři potvrzeno uživatelem |

---

## 3. Inventář (ověřeno 2026-08-14)

### 3.1 Modely

| Soubor | Role | Poznámka |
|---|---|---|
| `archive/models/najlepsi_model.h5` | Kanonický float32 model (907 KB, 72 193 parametrů) | Z diplomky; reprodukuje čísla článku nejblíže |
| `archive/models/model.tflite` | Kanonický int8 model (81 008 B) | Bit-identický s firmware `model_data.h` |
| `archive/models/model_data.h` | C header pro firmware (500 KB) | Zdroj pravdy pro ESP32 |

### 3.2 Data

| Cesta | Obsah |
|---|---|
| `datasets/recordings/audio/` | 854 WAV (48 kHz nativně, mono, cca 50 MB) |
| `datasets/recordings/metadata/original/` | 6 parquet souborů (zdroj labelů) |
| `datasets/recordings/manifest.csv` | 854 řádků, normalizovaný manifest, commitnutý a čistý v gitu |
| `datasets/recordings/splits.csv` | Kanonický split 80/20 stratified, seed 42 (683 train / 171 test; test: 13 launch / 158 non-launch) |

### 3.3 Klíčové skripty

| Skript | Role | Stav |
|---|---|---|
| `ml/generate_manifest.py` | Parquet metadata -> manifest.csv + splits.csv (random_state=42) | Funkční |
| `ml/prepare_features.py` | WAV -> MFCC 58x12 .npy; volby `--include-noisy --snr-db 30 20 10 5 --seed N --output DIR`; SNR vzorec na řádcích 31–36 | Funkční |
| `ml/evaluate_pc.py` | Eval Keras/TFLite; `predict_tflite()` s **opravenou** dequantizací (ř. 49–53) | Funkční, referenční správná implementace |
| `ml/evaluate_robustness.py` | SNR sweep (Table II/III styl); běží na **celém korpusu** (ignoruje split, ř. 22–36); FEATURE_ROOT natvrdo (ř. 15) | Funkční, potřebuje úpravy (M2) |
| `ml/eval_tflite_pc.py` | Legacy PC-int8 eval s **bugem** v dequantizaci (ř. 68, bez rozšíření typu) | Nefunkční (hardcoded složky); referenční pro M4 |
| `ml/convert_model.py` | Keras -> int8 TFLite (repr. dataset = prvních 100 train MFCC) + C header | Funkční |
| `ml/validate_esp_uart.py` | UART validace na ESP32; threshold `>` (ř. 69) — nekonzistence s PC (`>=`) | Pro M6 |

### 3.4 Existující výstupy předchozího běhu (2026-07-21) — referenční data

| Cesta | Obsah |
|---|---|
| `generated/features_seed42/` až `features_seed46/` | MFCC featury 5 seedů (5 variant x 2 splity x 2 třídy); `generated/features` je symlink na `features_seed42` |
| `generated/results/metrics_seed42..46.csv` | Agregované metriky per seed |
| `generated/results/per_sample_seed42..46.csv` | Per-sample predikce |
| `generated/results/per_sample_legacy_seed42.csv` | Předchozí legacy běh |
| `generated/results/per_seed_metrics.csv` | Agregát napříč seedy |

Tyto výstupy slouží jako **cross-check determinismu** nového harnessu — nesmí být přepsány, dokud nejsou porovnány (M0 krok 1, M3 krok 2).

### 3.5 Prostředí

- Python 3.12, závislosti pinované v `requirements.txt` (TF 2.21.0, librosa 0.11.0, sklearn 1.9.0, numpy 2.4.6, pandas 3.0.3).
- Task runner: `task` (go-task), definice v `Taskfile.yml`.
- PyPI dostupné (ověřeno 2026-08-14).
- Nalezeno `/opt/esp/python_env/idf5.4_py3.12_env` — v prostředí je pravděpodobně ESP-IDF v5.4 (relevantní pro M6, build možná půjde i bez desky).

---

## M0 — Prostředí a základna (repeatable baseline)

**Stav:** HOTOVO

**Cíl:** Deterministické, od nuly znovu-vytvořitelné prostředí a ochrana referenčních dat z běhu 2026-07-21.

**Proč:** Bez čistého prostředí není splněn požadavek "repeatable". Bez zálohy referenčních výsledků ztratíme jedinou možnost ověřit, že nový harness počítá stejně jako předchozí běh.

**Metoda:** Fresh venv z pinovaného `requirements.txt`. Determinismus manifestu se ověří přes `git diff` — `manifest.csv` a `splits.csv` jsou commitnuté a čisté, takže regenerace musí vyjít beze změn.

**Kroky:**

1. Záloha referenčních výsledků:
   ```
   cp -r generated/results generated/results_ref_20260721
   ```
   Obsah: `metrics_seed42..46.csv`, `per_sample_seed42..46.csv`, `per_sample_legacy_seed42.csv`, `per_seed_metrics.csv`. Od této chvíle read-only.
2. Čistý venv:
   ```
   rm -rf venv && task setup
   ```
   (Poznámka: provedeno již 2026-08-14 v rámci přerušeného pokusu, `task setup` skončil exit 0 — viz Pracovní log. Při navázání ověřit krokem 3, případně zopakovat.)
3. Smoke test:
   ```
   venv/bin/python -c "import tensorflow, librosa, sklearn, pandas, numpy; print('OK')"
   ```
4. Regenerace manifestu:
   ```
   task manifest
   ```
5. Determinismus manifestu:
   ```
   git diff --exit-code datasets/recordings/manifest.csv datasets/recordings/splits.csv
   ```
   Musí projít beze změn (exit 0).

**Výstupy:**
- Funkční venv se všemi závislostmi.
- `generated/results_ref_20260721/` (referenční záloha).
- Potvrzený deterministický manifest: 854 eventů (63 launch / 791 non-launch), split 80/20 seed 42.

**Akceptační kritéria:**
- [x] Importy TF/librosa/sklearn/pandas/numpy projdou v novém venv.
- [x] `git diff` manifestu a splits je prázdný.
- [x] Referenční výsledky zálohované; žádný další milník do nich nezapisuje.

**Odhad:** cca 30 min (většinu času zabere pip install TensorFlow).

---

## M1 — Proveniénce vah (je float32 .h5 a int8 .tflite tentýž model?)

**Stav:** HOTOVO — potvrzena **větev (b)**: různé váhy.

**Cíl:** Doložit, zda `archive/models/model.tflite` (= model nasazený ve firmware) vznikl kvantizací `archive/models/najlepsi_model.h5`.

**Proč:** Audit z 2026-07-21 uvádí jako podezřelého č. 2 pro PC/ESP diskrepanci "různé váhy": firmware model má 81 008 B, ale rekonverze z .h5 dává cca 81 400 B. Pokud float32 a int8 čísla v článku pocházejí z **různých modelů**, je celé srovnání float32 vs int8 (výtka R4-major2 "kvantizace zlepšuje robustnost") nevalidní. Tento milník rozhoduje narativ odpovědi na R4-major1 i R4-major2 — proto jde před hlavním během M3.

**Metoda:** Rekonverze .h5 do int8 TFLite stejným skriptem a stejným representative datasetem jako původně (`ml/convert_model.py`, prvních 100 train MFCC z clean variantu). Dva nezávislé testy: (1) per-sample skóre obou .tflite na clean featurách; (2) přímé dekvantizování Conv2D/Dense kernelů obou .tflite a srovnání s float32 vahami načtenými přímo z `najlepsi_model.h5` — tento test je necitlivý na kalibraci aktivací a měří shodu vah samotných.

**Kroky (provedeno):**

1. CLI `ml/convert_model.py` již podporovalo `--output`/`--header` bez úprav.
2. Rekonverze:
   ```
   venv/bin/python ml/convert_model.py --model archive/models/najlepsi_model.h5 \
       --output generated/models/reconverted.tflite --header generated/models/reconverted_model_data.h
   ```
   Výsledek: 81 400 B (archivní `model.tflite` má 81 008 B; rozdíl velikosti sám o sobě needůkazný).
3. Per-sample inference obou .tflite na `generated/features/clean/**` (854 vzorků, seed 42) — nový skript `ml/compare_tflite_weights.py` (používá `evaluate_pc.predict_tflite`, opravená dequantizace):
   ```
   venv/bin/python ml/compare_tflite_weights.py --model-a archive/models/model.tflite \
       --model-b generated/models/reconverted.tflite --output-csv generated/reports/weights_provenance_per_sample.csv
   ```
   Výsledek: max |delta skóre| = 0,50; shoda skóre jen 657/854 (77 %); **127/854 (15 %) přehozených predikcí**.
4. Přímé srovnání vah — nový skript `ml/compare_weights.py` (dekvantizuje int8 kernely a porovná s `layer.get_weights()` z h5):
   ```
   venv/bin/python ml/compare_weights.py --keras-model archive/models/najlepsi_model.h5 \
       --tflite-model archive/models/model.tflite --output-csv generated/reports/weights_layers_archive_vs_h5.csv
   venv/bin/python ml/compare_weights.py --keras-model archive/models/najlepsi_model.h5 \
       --tflite-model generated/models/reconverted.tflite --output-csv generated/reports/weights_layers_reconverted_vs_h5.csv
   ```
   Výsledek: `reconverted` vs h5 max diff 0,001–0,004 (řádově kvantizační krok, scale ~0,0017–0,0036 — kontrolní pár se chová jak se čeká). `archive` vs h5 max diff 0,40–1,25 při scale ~0,0008–0,0022, tj. **stovky kvantizačních kroků** — o dva až tři řády víc, žádná kvantizace to nevysvětlí.
5. Závěr zapsán do `generated/reports/weights_provenance.md`:
   - **Potvrzena větev (b):** `archive/models/model.tflite` nevznikl z `najlepsi_model.h5`. Jde o dva různé natrénované modely stejné architektury (72 193 parametrů). Do reviewer-response (M5) jde jako přímý podklad pro R4-major2 — silnější, než audit předpokládal (ne artefakt preprocessingu, ale nesrovnatelné modely).
6. **Nezávislý dvojitý review (2026-08-14, dva Opus agenti — metodologický + engineering).** Potvrdil závěr a doplnil: (a) korelace dekvantizovaných vah `archive/model.tflite` vs. h5 je -0,02 až 0,09 (prakticky nula) na všech 4 vrstvách, vs. 0,9999–1,0000 u kontrolního páru — silnější důkaz než jen počet kvantizačních kroků, vylučuje vysvětlení "stejné váhy, jen artefakt verze konvertoru"; (b) objevil latentní bug v `ml/compare_tflite_weights.py` — `load_clean()` četla `.npy` z natvrdo zapsané `FEATURE_ROOT` konstanty místo z `feature_index.parent` (stejná chyba, jakou M2 opravilo jinde); neškodilo to nahlášenému výsledku (clean varianta je bit-identická napříč seedy), ale bylo to nášlapná mina pro M3 — opraveno; (c) doporučil povýšit dosud "volitelný" experiment (float32 `najlepsi_model.h5` vs. skutečně kvantizovaný `reconverted.tflite`, tj. opravdový float32/int8 pár téhož modelu) na klíčové zjištění M1, protože rozhoduje R4-major2 přímo a levněji než celý plán M3/M4.
7. **Rozhodující experiment doběhnut (jeden seed):** `ml/evaluate_robustness.py --model-keras archive/models/najlepsi_model.h5 --model-tflite generated/models/reconverted.tflite --include-clean` — kvantizace na stejném modelu změnila MCC jen o ±0,01–0,03 bez konzistentního směru (30 dB: 0,52→0,51; 5 dB: 0,36→0,39), nic podobného publikovanému skoku 0,54→0,92.
8. **Druhý dvojitý review (M3, 2026-08-14)** doporučil ověřit rozhodující experiment i multi-seedově, ne jen na seedu 42 — na žádost uživatele provedeno. `ml/reproduce.py` parametrizován (`--model-keras/--model-tflite/--keras-label/--tflite-label/--results-root/--no-legacy-csv`) a spuštěn na páru `najlepsi_model.h5` vs. `reconverted.tflite` přes seedy 42–46, výstup do `generated/reports/quantization_effect_multiseed/` (mimo `generated/results/`, aby se nepřepsaly ověřené M3 výstupy). **Výsledek je jasnější, než jednoseedový:** kvantizace je lepší jen v 1 z 10 kombinací varianta×scope, horší v 9 z 10; průměrný efekt přes všechny kombinace MCC −0,0115 (std 0,013) — kvantizace na stejném modelu robustnost v průměru mírně **zhoršuje**, nikoli zlepšuje. **Potvrzeno na 5 seedech: "kvantizace zlepšuje robustnost" je artefakt srovnávání dvou různých modelů, ne reálný efekt kvantizace.** Nahrazuje "artefakt preprocessingu" jako pravděpodobné vysvětlení z auditu 21. 7. — jde o silnější, přímo měřenou a nyní i multi-seedově ověřenou příčinu.

**Výstupy:**
- `generated/reports/weights_provenance.md` (plný závěr, tabulky včetně korelace, rozhodující experiment na 1 i 5 seedech, reprodukční příkazy).
- `generated/reports/weights_provenance_per_sample.csv` (854 řádků, skóre obou modelů vedle sebe).
- `generated/reports/weights_layers_archive_vs_h5.csv`, `weights_layers_reconverted_vs_h5.csv` (per-vrstva diff + korelace).
- `generated/reports/quantization_effect_same_model.csv`, `quantization_effect_same_model_per_sample.csv` (rozhodující experiment, seed 42).
- `generated/reports/quantization_effect_multiseed/{per_seed_metrics.csv,summary_mean_std.csv,per_sample_seed*.csv}` (rozhodující experiment, seedy 42–46).
- Nové skripty: `ml/compare_tflite_weights.py`, `ml/compare_weights.py` (obojí opraveno/rozšířeno po review); `ml/reproduce.py` parametrizován pro libovolný pár modelů (rozšíření z M3).
- `generated/models/reconverted.tflite`, `reconverted_model_data.h` (gitignored, reprodukovatelné z `najlepsi_model.h5`).

**Akceptační kritéria:**
- [x] Jednoznačný závěr "stejné váhy ano/ne" podložený per-sample statistikou (max delta, počet flipů) i přímým srovnáním vah (rozšířeno o korelaci).
- [x] Závěr formulovaný tak, aby šel přímo citovat v odpovědi recenzentům (`generated/reports/weights_provenance.md`, sekce "Důsledky pro camera-ready a odpověď recenzentům").
- [x] Nezávisle ověřeno dvěma review agenty; nálezy zapracovány (bug oprava, korelace, rozhodující experiment).

**Odhad:** 1–2 h. Skutečnost: cca 1 h + dodatečná 1 h po review.

---

## M2 — Úpravy evaluačních skriptů (held-out, multi-seed, per-sample audit)

**Stav:** HOTOVO

**Cíl:** Rozšířit `ml/evaluate_robustness.py` o held-out eval, podporu multi-seed feature rootů a per-sample výstup — při zachování stoprocentní zpětné kompatibility (stávající Taskfile tasky se nesmí změnit chováním).

**Proč:**
- Held-out čísla (`--split test`) jsou přímá odpověď na R3 a R4-major3 ("results on data not used in training").
- Per-sample CSV je nutný předpoklad pro porovnání PC vs ESP32 s tolerancí 1 LSB v M6 a pro jakýkoli audit jednotlivých predikcí.
- Konfigurovatelný feature root je předpoklad multi-seed smyčky v M3 (dnes je `FEATURE_ROOT` konstanta na řádku 15).

**Metoda:** Minimální, zpětně kompatibilní změny — všechny nové parametry mají defaulty odpovídající dnešnímu chování.

**Kroky (provedeno):**

1. `ml/evaluate_robustness.py`:
   - Nový argument `--split {all,train,test}` (default `all`). `load_variant()` nyní filtruje `rows[rows["split"] == split]` pro `split != "all"` a navíc vrací seznam `split` per vzorek (pro audit při `split=all`).
   - `feature_root` se odvozuje uvnitř `load_variant()` jako `feature_index.parent` místo modulové konstanty `FEATURE_ROOT` — cesty `feature_path` v manifestu jsou relativní k němu; funguje tak jak s výchozím `generated/features/`, tak s `generated/features_seedN/`.
   - Nový argument `--per-sample-csv PATH`: `run_regime()` vrací kromě metrik i `per_sample` (recording_id, split, class_id, score, pred); `main()` je sbírá přes všechny varianty/režimy do `per_sample_rows` se sloupci `mode, variant, split, recording_id, class_id, score, pred`.
2. Regresní kontrola: běh bez nových flagů (`--model-tflite archive/models/model.tflite --include-clean`) dal **bitově shodná čísla** s referencí `generated/results_ref_20260721/metrics_seed42.csv` (např. 30 dB accuracy 0,977751756440281, MCC 0,8624502463394716) i s `REPRODUCTION_NOTES.md` (30 dB Acc 97,78 % / MCC 0,86; 5 dB Acc 85,71 % / MCC 0,42) — žádná regrese.
3. Sanity kontrola held-out: `--split test` na clean variantě vrátila přesně **171 vzorků** (odpovídá `splits.csv`: 13 launch / 158 non-launch v test splitu).
4. Sanity kontrola per-sample CSV: běh se 2 režimy (keras+tflite) × 5 variant × `--split test` dal **1710 řádků** (= 10 × 171), sloupce `split` obsahují jen `test` (žádný leak z train).
5. Sanity kontrola multi-seed feature rootu: `--features generated/features_seed43/features_manifest.csv` načetl featury ze správného adresáře (jiná čísla než seed 42 na stejné variantě, jak se čeká u jiné realizace šumu) — potvrzuje, že smyčka přes seedy v M3 může fungovat beze zvláštní úpravy.

**Výstupy:** Upravený `ml/evaluate_robustness.py` (zpětně kompatibilní, commitnuto).

**Akceptační kritéria:**
- [x] Bez nových flagů je výstup bitově shodný s chováním před úpravou (žádná regrese).
- [x] `--split test` vrací správně filtrované počty vzorků.
- [x] Per-sample CSV obsahuje řádek pro každý vzorek každého variantu.

**Odhad:** 1–2 h. Skutečnost: cca 45 min.

---

## M3 — Multi-seed reprodukční běh (nová sjednocená camera-ready čísla)

**Stav:** HOTOVO

**Cíl:** Kompletní robustnostní sweep: {clean, 30, 20, 10, 5 dB} x {float32, int8} x {full-corpus, held-out test} x {seed 42–46}, agregovaný na mean ± std. Toto jsou čísla pro novou Tabulku II/III camera-ready verze.

**Proč:** Sjednocený eval (jeden model, jeden preprocessing, opravená dequantizace, jednotný threshold `>=`) odstraňuje všechny známé zdroje nekonzistence mezi režimy — přímá odpověď na R4-major2. Held-out sloupec odpovídá R3/R4-major3. Multi-seed kvantifikuje varianci šumu — nutné, protože test split má jen 13 launch eventů a jedna realizace šumu je statisticky slabá.

**Metoda:** Nový orchestrátor `ml/reproduce.py` (čisté CLI bez config souboru — konzistentní se stylem repa), volá `run_regime()` z `ml/evaluate_robustness.py` přímo (import, ne subprocess) — využívá přesně refaktoring z M2. Featury per seed se regenerují deterministicky (`np.random.seed(seed)` v `prepare_features.py`, ř. 65; pořadí dané manifestem); všech 5 seed adresářů již existovalo z předchozí session, takže se generování featur přeskočilo (`--force-features` by je vynutilo znovu). Kanonické modely z `archive/models/` (rozhodnutí #2) — vědomě "as-shipped" pár, ne tvrzení o identitě vah (M1, větev b).

**Kroky (provedeno):**

1. Napsán `ml/reproduce.py`:
   - Pro každý seed z {42, 43, 44, 45, 46}: `prepare_features_for_seed()` přeskočí generování, pokud `generated/features_seedN/features_manifest.csv` existuje (bylo tomu tak u všech 5).
   - Pro každý seed 4 kombinace evalu přes přímo importovaný `run_regime()`: `{keras najlepsi_model.h5, tflite archive/model.tflite}` x `{split=all, split=test}`, vždy přes všech 5 variant (clean + 4 SNR).
   - Zapisuje **dva formáty současně**: (a) `metrics_seedN.csv` v přesném schématu původního (pre-M2) `evaluate_robustness.py --output-csv` (`mode,snr_db,accuracy,precision,recall,f1,mcc,samples`, jen scope=full) — pro přímý cross-check proti referenci; (b) `per_seed_metrics.csv` v schématu `mode,variant,seed,scope,n,accuracy,precision,recall,f1,mcc` (objeveno již existující v `generated/results_ref_20260721/per_seed_metrics.csv` z 21. 7. — ukázalo se, že toto přesné schéma tam již bylo připravené z dřívější, jinak nezachované session).
   - Agregace: `summarize()` (`groupby(mode,variant,scope).agg(mean,std)`) → `generated/results/summary_mean_std.csv`.
2. **Cross-check determinismu (opraveno po review, viz Pracovní log):** `diff generated/results/metrics_seedN.csv generated/results_ref_20260721/metrics_seedN.csv` pro N=42..46 — **všech 5 bitově identických** (`diff` bez výstupu). Agregát `per_seed_metrics.csv` (100 řádků) je proti referenci z 21. 7. **hodnotově identický** (nulový absolutní rozdíl ve všech metrikách `n,accuracy,precision,recall,f1,mcc`), ale **ne bitově** — pořadí řádků a konce řádků (CRLF vs LF) se liší; kdo by na tento soubor pustil `diff`/`cmp` napřímo, uvidí rozdíl, i když jsou hodnoty totožné. **Důležité omezení tohoto cross-checku:** featury pro seedy 42–46 už existovaly na disku a nebyly přegenerovány (`prepare_features` se přeskočilo) — test tedy ověřuje jen "deterministická inference nad stejnými cache soubory na stejném stroji dvakrát", ne přenositelnost mezi prostředími ani determinismus samotné extrakce MFCC/generování šumu (ta cesta kódu se tímto během neprocvičila). Pro skutečné ověření determinismu regenerace featur by bylo nutné spustit `--force-features` a znovu diffnout — neprovedeno (cca +1 h výpočtu), zůstává jako mezera pro M5/budoucí re-run.
3. **Sanity kontrola (opravena po review — původní formulace "v rámci 1 std" byla nepřesná):** čísla v `REPRODUCTION_NOTES.md` JSOU přímo realizace seedu 42 (ne nezávislý zdroj) — jejich shoda se seedem 42 uvnitř `per_seed_metrics.csv` je už dokázaná bitovou identitou v kroku 2. Průměr přes 5 seedů se od hodnoty jednoho konkrétního seedu (42) přirozeně liší o řád srovnatelný s měřeným std — to je čekané chování průměrování přes různé realizace šumu, ne anomálie a ne nezávislé externí ověření: např. int8 MCC @ 30 dB: seed 42 samotný = 0,8625, mean 5 seedů = 0,8685 ± 0,0061 (rozdíl = přesně 1,00 std); @ 5 dB: seed 42 = 0,4250, mean = 0,4368 ± 0,0142 (0,84 std); float32 @ 30 dB: seed 42 = mean = 0,5171 (shoda, ale jen protože std je tam velmi malé, 0,0021 — nízká citlivost float32 modelu na šum při 30 dB, ne obecná vlastnost). Float32 clean/30dB `std=0` napříč seedy (clean varianta je bez šumu, seed nemá vliv) — očekávané chování, ne bug.
4. Opakovatelnost: druhý běh `--seeds 42` dal bitově identický `metrics_seed42.csv` jako první běh (`diff` bez výstupu).
5. **Caveat ke statistice (doplněno po review):** std přes 5 seedů zachycuje jen varianci **realizace šumu** na fixním test splitu, ne nejistotu ze vzorkování/malého počtu launch eventů (13 v test splitu) samotného — to je jiný zdroj nejistoty, který multi-seed přes stejný split nepokrývá. Ilustrace: held-out int8 @ 30 dB má `std` přesně 0 (žádný z 5 šumových seedů nezměnil predikci na 171 testovacích vzorcích) — to by se dalo mylně číst jako "nulová nejistota", ale ve skutečnosti to jen znamená, že v tomto konkrétním bodě šum nezpůsobil žádný flip, ne že je measurement bezpečný napříč jinými zdroji nejistoty. Pro M5/reviewer-response text: prezentovat mean ± std jako "variance šumové realizace", a samostatně explicitně uvádět caveat "13 launch eventů v test splitu" jako jiný, nepokrytý zdroj nejistoty — ne slučovat obojí do jednoho čísla.

**Výstupy:**
- Nový skript `ml/reproduce.py` (orchestrátor, commitnuto).
- `generated/features_seed42..46/` (beze změny, již existovaly).
- `generated/results/metrics_seed42..46.csv` (schéma kompatibilní s referencí, pro cross-check), `per_sample_seed42..46.csv` (10 250 řádků na seed = 2 režimy x 2 scopes x 5 variant x [854 nebo 171 vzorků]). Pozor: schéma `per_sample_seedN.csv` je nové (`mode,variant,seed,scope,recording_id,split,class_id,score,pred`), liší se od staršího referenčního `per_sample_seed42.csv` (`mode,snr_db,recording_id,variant,split,class_id,score,pred`, jen scope=test) — pokud M5 čte per-sample data, použít nové soubory, ne referenční.
- `generated/results/per_seed_metrics.csv` — 100 řádků (5 seedů x 2 režimy x 2 scopes x 5 variant), hlavní zdroj čísel pro M5.
- `generated/results/summary_mean_std.csv` — 20 řádků (2 režimy x 2 scopes x 5 variant), mean ± std přes 5 seedů, `n_seeds=5` u všech.

**Akceptační kritéria:**
- [x] Kompletních 5 seedů x 2 režimy x 2 scopes x 5 variant = 100 řádků metrik.
- [x] Dvojí běh stejného seedu = bitově identické CSV.
- [x] Shoda s referencí 2026-07-21 potvrzena (5 per-seed CSV bitově, agregát hodnotově — viz krok 2, upřesněno po review).
- [x] Sanity kontrola provedena a formulována přesně (rozdíl seed-vs-mean je řádu měřeného std, jak se čeká — viz krok 3, opraveno po review).

**Odhad:** 2–4 h. Skutečnost: cca 1 h (generování featur nebylo potřeba, jen psaní orchestrátoru + běh inference přes existující featury).

---

## M4 — Legacy proveniénční běh (původ publikovaných čísel)

**Stav:** HOTOVO — potvrzena **větev (b)**: hypotéza bugu vyvrácena.

**Cíl:** Rozhodnout, zda publikovaná PC-int8 čísla (Tabulka III) jsou artefakt int8 overflow bugu v legacy evaluačním skriptu.

**Proč:** Nejtvrdší výtka recenzí (R2#6, R4-major1): tři inferenční režimy si odporují, ačkoli stejný int8 model na stejných vstupech musí dávat prakticky identická čísla. Kandidátní mechanismus: `ml/eval_tflite_pc.py:68` — `pred_prob = (output_data[0][0] - zero_point) * scale` s úzkými int8 operandy; pod numpy 2.x (NEP 50) rozdíl přetéká (např. 127 − (−128) = wrap místo 255). ESP32 C kód (`firmware/esp32s3/main/main.cpp:160-164`) typ rozšiřuje správně — bug se týká jen PC strany. Pokud legacy běh reprodukuje publikovaná PC-int8 čísla a opravený běh dává čísla blízká ESP32, je diskrepance vysvětlena mechanicky a "quantization improves robustness" se reframuje.

**Metoda:** Nový `ml/reproduce_legacy.py` — věrně replikuje legacy dequantizaci (úzké int8 odečítání, NEP 50 chování), ale načítá featury z manifest-based struktury (legacy hardcoded složky `gunshots_mfcc_audio_noise` atd. neexistují). Full-corpus, featury seed 42, model `archive/models/model.tflite`. Vedle toho srovnání s opravenou dequantizací (z M3, seed 42, full-corpus) pro přímý diff.

**Kroky (provedeno):**

1. Napsán `ml/reproduce_legacy.py`; bug replikován přesně (datové typy, pořadí operací dle `eval_tflite_pc.py:54-70`); zbytek pipeline (kvantizace vstupu, threshold `>=0.5`, metriky) shodný s opraveným evalem, aby jediný rozdíl byla dequantizace výstupu.
2. **Předběžná matematická analýza před během:** kvantizace výstupu tohoto modelu má `zero_point=-128` (z M1). Pro tuto hodnotu je `raw - (-128) = raw + 128`; pro `raw` odpovídající pravděpodobnosti `>= 0.5` (tj. `raw ∈ [0,127]`) dá `raw+128 ∈ [128,255]`, což **vždy** přeteče int8 rozsah a wrapne na záporné číslo — bez ohledu na jistotu modelu. Predikce: bug způsobí **recall 0,00 na všech úrovních šumu** pro tento konkrétní model.
3. Běh: `venv/bin/python ml/reproduce_legacy.py --output-csv generated/reports/legacy_bug_metrics.csv --per-sample-csv generated/reports/legacy_bug_per_sample.csv` na `generated/features_seed42`, full-corpus, `archive/models/model.tflite`, všechny varianty (clean + 4 SNR). **Predikce z kroku 2 se potvrdila přesně:** recall 0,00 na všech 5 variantách, accuracy konstantní 92,62 % (= přesně podíl non-launch vzorků 791/854 — model "predikuje" úplně všechno jako třídu 0).
4. Srovnávací tabulka per SNR: publikovaná Tabulka III (PC int8) vs. legacy-bug běh vs. opravený běh (M3, seed 42, full-corpus) — viz `generated/reports/provenance_table3.md`. Legacy-bug MCC/recall je nulové na všech úrovních, publikovaná i opravená čísla jsou v rozsahu 0,65–0,92 / 0,90–1,00 — naprostý nesoulad.
5. Závěr zapsán do `generated/reports/provenance_table3.md`:
   - **Potvrzena větev (b):** legacy bug != publikovaná čísla. Hypotéza "PC vs ESP diskrepance je artefakt tohoto bugu" padá — pokud by Tabulka III vznikla tímto skriptem v jeho aktuální podobě, recall by byl nulový, ne 0,90–1,00. Publikovaná čísla tedy vznikla jinak (opravenou verzí kódu, nebo skriptem, který se nezachoval). Zbývající vysvětlení PC-vs-ESP diskrepance: M1 nález (různé váhy) jako hlavní kandidát; HW rozdíly (M6) jako druhý, dosud neprověřený.

**Výstupy:**
- `generated/reports/provenance_table3.md` (plný závěr, srovnávací tabulky, matematická predikce vs. empirie).
- `generated/reports/legacy_bug_metrics.csv`, `legacy_bug_per_sample.csv`.
- Nový skript `ml/reproduce_legacy.py` (commitnuto).

**Akceptační kritéria:**
- [x] Tři sady čísel (publikovaná / legacy-bug / opravená) vedle sebe per SNR.
- [x] Jednoznačný závěr větve (a)/(b), formulovaný pro přímé použití v Section V/VI článku a v odpovědi recenzentům (`provenance_table3.md`, sekce "Závěr: větev (b)").

**Odhad:** 2–3 h. Skutečnost: cca 45 min (matematická predikce zúžila potřebu experimentování).

---

## M5 — Deliverables (harness, reviewer response, LaTeX, HTML)

**Stav:** HOTOVO

**Cíl:** Zabalit výsledky M1–M4 do čtyř dohodnutých výstupů; celá PC reprodukce spustitelná jedním příkazem.

**Metoda:** Nový `ml/make_deliverables.py` (čte agregované CSV z M1/M3/M4, generuje texty) + dva nové Taskfile tasky. Žádná ruční čísla — vše generované ze zdrojových CSV, aby regenerace po případné změně byla triviální.

**Kroky (provedeno):**

1. `Taskfile.yml` — nové tasky:
   - `reproduce:pc` — end-to-end řetěz: `ml/reproduce.py` (M3, as-shipped pár) -> `ml/convert_model.py` + `ml/compare_tflite_weights.py` + `ml/compare_weights.py` ×2 + `ml/evaluate_robustness.py` + `ml/reproduce.py` s parametrizací (M1, proveniénce vah a rozhodující kvantizační experiment na 1 i 5 seedech) -> `ml/reproduce_legacy.py` (M4) -> `ml/make_deliverables.py`. **Rozšířeno oproti původnímu plánu** — zahrnuje i M1 kroky, ne jen M3/M4, aby byl harness skutečně kompletní.
   - `reproduce:clean` — smaže `generated/features_seed*`, `generated/results/`, `generated/models/`; v `generated/reports/` smaže vše **kromě** `weights_provenance.md` a `provenance_table3.md` (ručně psané reporty s analýzou, které žádný skript nerekonstruuje — jen podkladová CSV k nim se regenerují). **Nikdy** nesahá na `generated/results_ref_20260721/`.
2. `ml/make_deliverables.py` generuje (vše z CSV, žádná ruční čísla):
   - `generated/reports/table2_float32.tex` a `table3_int8.tex` — IEEE booktabs formát shodný se stylem `BEC/article/article_main.tex` (`\resizebox`, `\toprule/\midrule/\bottomrule`), sloupec Scope (Full corpus / Held-out test) × SNR, hodnoty mean ± std přes 5 seedů.
   - `generated/reports/reviewer_response.md` — 5 sekcí: výtka -> důkaz (čísla natažená z CSV) -> navrhovaná formulace do článku. Pokrývá R2#6/R3/R4-major1/R4-Q1 (M1+M4), R3/R4-major2/R4-Q2 (M1 rozhodující experiment), R3/R4-major3/R4-Q3 (M2+M3 held-out), R4-Q4 (SNR vzorec), R1 III.C (velikost modelu; Flash/RAM/ops označeno jako závislé na M6).
   - `generated/reports/deliverables_data.json` — konsolidovaný podklad pro HTML artifact (publikovaná čísla, reprodukovaný souhrn, kvantizační efekt, proveniénce vah, legacy bug).
3. **HTML artifact vytvořen** (nástroj Artifact, po skillu `artifact-design`): jednostránkový "evidence review" — přehledový pruh 5 verdiktů (confirmed/refuted/measured/partial) s odkazy na detailní sekce, pak po sekcích: proveniénce vah (korelační stupnice bipolárního rozsahu −1..+1, marker na ose místo plněného pruhu, aby blízká-nule hodnota nevypadala jako "poloviční"), kvantizační efekt (divergentní bar chart 10 kombinací, červená/zelená), held-out vs. full-corpus (skupinové pruhy), legacy bug (tabulka predikce vs. měření vs. publikováno), a fakta/mezery pro M6. Publikováno: <https://claude.ai/code/artifact/b0cf34ae-c933-40fd-9292-b51e0c44360b>.
4. `REPRODUCTION_NOTES.md` aktualizován: nová úvodní poznámka s odkazem na roadmapu a `task reproduce:pc`; sekce 9 ("Out of scope") rozdělena na skutečně zbývající položky (HW, dataset licence, ...) a nově vyřešené (PC/ESP diskrepance, kvantizační tvrzení, held-out) s odkazem na M1–M4.

**Ověření LaTeX (částečné — chybí toolchain):** `pdflatex`/`xelatex`/`lualatex` nejsou v tomto prostředí dostupné, takže skutečný `\input` do `article_main.tex` a build PDF nebylo možné provést. Ověřeno staticky: počet `{`/`}` vyvážený, `\begin`/`\end` páry sedí, balíčky které tabulky používají (`graphicx` pro `\resizebox`, `booktabs` pro `\toprule`/`\addlinespace`) jsou už v preambuli článku. Skutečný build je nutné provést v prostředí s LaTeX distribucí (stejná kategorie omezení jako ESP-IDF u M6).

**Ověření end-to-end (kompletní, na žádost uživatele):** spuštěno skutečné `task reproduce:clean && task reproduce:pc` od nuly (ne jen s cache featurami) — smazáno a přegenerováno vše včetně featur pro všech 5 seedů (854 WAV × 5 variant × 5 seedů). Před spuštěním zálohováno `generated/{results,reports,models}` do scratchpadu pro jistotu. Výsledek:
- Všech 5 `metrics_seedN.csv` bitově identických s `generated/results_ref_20260721/` — **tentokrát včetně regenerace featur**, ne jen cache inference (uzavírá mezeru, kterou flagl review M3).
- `per_seed_metrics.csv` (100 řádků) hodnotově identický s referencí (nulový rozdíl ve všech metrikách).
- M1 výstupy (`weights_layers_archive_vs_h5.csv`, `quantization_effect_multiseed/summary_mean_std.csv`) i M4 (`legacy_bug_metrics.csv`) i deliverables (`table3_int8.tex`, `reviewer_response.md`) — všechny identické s předchozím (cache) během, potvrzující plný end-to-end determinismus.
- Oba hand-authored reporty (`weights_provenance.md`, `provenance_table3.md`) přežily `reproduce:clean` nedotčené.

**Výstupy:**
- Funkční `task reproduce:pc` a `task reproduce:clean`, ověřené end-to-end od nuly.
- `generated/reports/reviewer_response.md`.
- `generated/reports/table2_float32.tex`, `table3_int8.tex`.
- `generated/reports/deliverables_data.json`.
- HTML artifact: <https://claude.ai/code/artifact/b0cf34ae-c933-40fd-9292-b51e0c44360b>.

**Akceptační kritéria:**
- [x] `task reproduce:clean && task reproduce:pc` doběhne od nuly bez ručních zásahů.
- [x] .tex fragmenty staticky validní (vyvážené závorky, existující balíčky) — **skutečný pdflatex build neproveden, chybí toolchain v tomto prostředí.**
- [x] reviewer_response.md pokrývá všechny hlavní výtky + R4-Q4.
- [x] Artifact vygenerován a sdílen.

**Odhad:** 3–4 h. Skutečnost: cca 2,5 h.

---

## M6 — ESP32-S3 hardware (volitelné; vyžaduje desku)

**Stav:** HOTOVO — build, flash, živá UART validace (per-sample shoda <= 1 LSB, všech 5 hladin šumu) a Release/Flash/RAM/ops měření (krok 6) dokončeny.

**Cíl:** Reálná embedded čísla pro Tabulku III a definitivní uzavření PC vs ESP diskrepance per-sample důkazem; Release-build latence a Flash/RAM čísla pro R1 III.C.

**Známé blokery a fakta:**
- `.devcontainer/fw-devcontainer/` referencovaný v REPRODUCTION_NOTES neexistuje v repu.
- Nekonzistentní ESP-IDF verze v repu: `firmware/esp32s3/main/idf_component.yml` deklaruje `>=4.1.0`, `dependencies.lock` je resolvnutý proti 5.2.1, poznámky zmiňují Docker s v5.4. **Vyřešeno empiricky (2026-08-14): `/opt/esp` prostředí s v5.4 firmware sestavilo bez úprav** — verze inkonzistence v deklaracích se v praxi neprojevila jako blokující.
- Firmware je aktuálně Debug build (`CONFIG_COMPILER_OPTIMIZATION_DEBUG=y`) — publikovaná latence cca 32 ms je měřena v Debug režimu a jen kolem `Invoke()`.
- Firmware `model_data.h` je bit-identický s `archive/models/model_data.h` — na desce běží kanonický archivní model, žádná výměna není potřeba.
- Deska je připojena přes USB/IP passthrough (typicky WSL2 ↔ Windows host) — spojení bylo zpočátku nestabilní: kontrolní operace (`esptool` flash, krátké request/response sekvence) fungovaly spolehlivě, ale udržovaný obousměrný datový tok (UART app data, boot log) nefungoval vůbec — nula bajtů oběma směry i po opakovaných pokusech (reset přes RTS, čerstvé otevření portu, přepnutí appky na jiný port). **Přesná příčina zůstává nejistá** — v `dmesg` se v čase testů objevil `vhci_hcd: urb->status -104` (ECONNRESET), ale tentýž vzor se objevoval i **během pozdějšího úspěšného běhu** (viz krok 5), takže korelace sama neprokazuje kauzalitu. Po rebuildu kontejneru a přepnutí desky na jiný fyzický USB-C port (viz krok 5) začalo spojení fungovat bez chyby — nelze s jistotou určit, zda to způsobil rebuild, změna portu, nebo náhodná stabilizace tunelu.

**Kroky (checklist, stav k 2026-08-14):**

1. [x] Zprovoznit ESP-IDF toolchain — `/opt/esp/idf` v5.4 aktivován přes `source /opt/esp/idf/export.sh`, bez úprav funguje.
2. [x] `task firmware:build` — **úspěšný**, binárka 0x65ef0 B (60 % volno v 0x100000 B app partition), žádné chyby, jen jedno neškodné compiler warning (`uart_config_t::flags` neinicializováno).
3. [x] `task firmware:flash ESPPORT=/dev/ttyACM0` (přímo přes `idf.py -p /dev/ttyACM0 flash`) — **úspěšný**. Deska se identifikovala jako reálný **ESP32-S3 (QFN56, revize v0.2), WiFi/BLE, 8 MB embedded PSRAM, MAC `98:3d:ae:61:1d:18`**. Bootloader (21 008 B), app (417 520 B) a partition table (3 072 B) nahrány a hash ověřen.
4. [x] `ml/validate_esp_uart.py` upraven: threshold `>` -> `>=` (sjednocení s PC evalem — potvrzeno v `firmware/esp32s3/main/main.cpp:164`, že C++ dequantizace `(out_val - zero_point) * scale` je bezpečná, `int8_t` se automaticky promuje na `int32_t`, takže na desce žádný overflow bug z M4 nehrozí); doplněno `--per-sample-csv` pro log raw skóre.
5. [x] **Vyřešeno.** Průběh: smoke test (3 vzorky) i passivní poslech nejdřív vrátily 0 bajtů (appka na `UART_NUM_0` mluvila na jiný fyzický port, než byla připojená deska — potvrzeno v `sdkconfig`: `CONFIG_ESP_CONSOLE_UART_NUM=0`). **Firmware upraven** (`main.cpp`, `Kconfig.projbuild`, `CMakeLists.txt`): nový Kconfig přepínač `CONFIG_APP_DATA_LINK_USB_SERIAL_JTAG` (default `n`, zachovává původní chování), který při `y` přesměruje datový spoj na nativní USB-Serial/JTAG. Po přepnutí byla odpověď stále nulová. **Poznámka po review (2026-08-14):** vyloučení "port byl příčinou" stálo jen na ověření přes `nm` (že se zkompilovala správná cesta kódu), ne na funkčním přenosu s touto konfigurací — důkaz je tedy slabší, než jak byl prezentován, byť závěr (že šlo o tunel, ne o kód) se nakonec ukázal správný. **Uživatel rebuildnul kontejner a přepojil desku na druhý USB-C port** (fyzický UART-to-TTL převodník, ne nativní USB-Serial/JTAG) — flash i UART data tentokrát **prošly bez jediné chyby**. Přesnou příčinu předchozího selhání (tunel vs. port vs. náhoda) nelze zpětně s jistotou určit — viz upravená poznámka v blokerech výše.
   - Build s výchozím (UART0) nastavením přeflashován na `/dev/ttyACM0`. Smoke test (3 vzorky): reálné odpovědi, `latency_ms=32` — přesně publikovaná hodnota z článku.
   - **Plný test split (171 vzorků) na všech 5 variantách** (`clean`, `noise_snr30db`, `noise_snr20db`, `noise_snr10db`, `noise_snr5db`) — **0 chyb/no-response ze všech 855 volání** (rozšířeno z původních 3 variant/513 vzorků po review, který upozornil na neúplné pokrytí — 20 dB a 10 dB doplněny dodatečně).
   - **Per-sample srovnání ESP32-int8 vs. opravený PC-int8 (seed 42, `generated/results/per_sample_seed42.csv`):**

     | Varianta | n | Max \|delta\| | Mean \|delta\| | Medián \|delta\| | Přehozené predikce |
     |---|---|---|---|---|---|
     | clean | 171 | 0,000006 | 0,000000 | 0,000000 | 0 |
     | 30 dB | 171 | 0,003906 | 0,000023 | 0,000000 | 0 |
     | 20 dB | 171 | 0,003906 | 0,000092 | 0,000000 | 0 |
     | 10 dB | 171 | 0,003906 | 0,000183 | 0,000000 | 0 |
     | 5 dB | 171 | 0,003906 | 0,000274 | 0,000000 | 0 |

     `0,003906 = 1/256` = přesně **jeden kvantizační krok (1 LSB)** výstupu tohoto modelu (scale `0,00390625`, zjištěno v M1). Delta u clean (0,000006) je čistě artefakt zaokrouhlení na 4 des. místa v odpovědi z desky (`%.4f`), ne skutečný rozdíl. Medián 0,000000 na všech úrovních znamená, že drtivá většina vzorků se shoduje bit-přesně; jen menšina se liší o přesně 1 LSB (zaokrouhlovací hranice kvantizace), ne systematický posun. **Přes všech 855 testovaných vzorků (5 hladin šumu) nula přehozených predikcí.**
   - **Důsledek (formulace upravena po review — původní verze tvrdila víc, než experiment prokazuje):** tento test dokazuje, že **na stejném kanonickém modelu** se PC-int8 (opravená dequantizace, M4) a ESP32-int8 shodují v rámci 1 LSB — přesně cíl stanovený v akceptačních kritériích. To je silný **eliminační argument**: mezi platformami (PC vs. ESP32) samotnými není inherentní rozdíl v int8 aritmetice, který by mohl způsobovat rozdílné výsledky. Test ale **neprokazuje přímo**, proč se lišila publikovaná Tabulka III — použil opravenou PC metodiku a aktuální firmware, ne původní PC skript ani nutně stejné vstupy jako při přípravě článku. Správná formulace: "rozdíl platforem je vyloučen jako vysvětlení PC-vs-ESP diskrepance; ve spojení s M1 nálezem (dva různé natrénované modely) je pravděpodobnější příčinou nekonzistentní model/metodika při přípravě původních čísel, ne kauzálně prokázáno tímto testem samotným."
   - Výstupy: `hardware_validation/esp32s3/esp32_per_sample_{clean,snr30,snr20,snr10,snr5}_test.csv` (přesunuto z `generated/reports/` a commitnuto do gitu po review — jde o nenahraditelná HW data, na rozdíl od zbytku `generated/`, které je regenerovatelné skriptem).
6. [x] **Hotovo.** Postup a zjištění:
   - **Přesnost měření latence zvýšena na mikrosekundy** (`main.cpp`): `int infer_time_ms = (end_time-start_time)/1000` zaokrouhlovalo na celé ms, takže by rozdíl Debug/Release mohl zůstat skrytý pod rozlišením. Změněno na `%d us` v odpovědi (a odpovídající úprava regexu/sloupce v `ml/validate_esp_uart.py`: `latency_ms` (int) -> `latency_us` (int), výstup v `ms` s 3 des. místy pro čitelnost).
   - **Release build** (`CONFIG_COMPILER_OPTIMIZATION_PERF`): sestaven, naflashován, změřeno 30 vzorků (clean/test) — **32,777 ms** (rozsah 32,759–32,908 ms).
   - **Debug build** (původní, `CONFIG_COMPILER_OPTIMIZATION_DEBUG`, se stejným mikrosekundovým firmwarem pro přímé srovnání): 30 vzorků — **32,818 ms** (rozsah 32,799–32,916 ms).
   - **Rozdíl Debug vs. Release: ~0,04 ms (0,1 %) — zanedbatelný.** Publikovaná latence ~32 ms tedy **není artefaktem Debug buildu** — na tomto modelu/hardwaru je téměř identická nezávisle na úrovni optimalizace kompilátoru. Pravděpodobné vysvětlení: výpočetně náročné operace běží přes ručně optimalizované ESP-NN kernely (viz níže), ne přes kód generovaný kompilátorem, takže úroveň optimalizace C++ na ně má malý vliv.
   - **`idf.py size` (Flash/RAM, Release build):**

     | Oblast | Použito | Celkem | % |
     |---|---|---|---|
     | Flash Code (`.text`) | 197 872 B | — | — |
     | Flash Data (`.rodata`+`.appdesc`) | 142 272 B | — | — |
     | DIRAM (běhová SRAM: `.bss`+`.text`+`.data`) | 140 019 B | 341 760 B | 40,97 % |
     | IRAM | 16 383 B | 16 384 B | **99,99 %** |
     | RTC FAST | 52 B | 8 192 B | 0,63 % |
     | Celková velikost image | 408 582 B | 1 048 576 B (app partition) | 39 % (61 % volno) |

     Pozn.: `tensor_arena` (80 KiB, `uint8_t tensor_arena[80*1024]`) tvoří ~93 % `.bss` segmentu v DIRAM — dominantní spotřebitel RAM, jak se čekalo. **IRAM je téměř úplně plná (99,99 %)** — zajímavý fakt pro článek, byť nejde o problém (firmware se vejde), spíš o poznámku, že model je blízko limitu instrukční RAM na této platformě.
   - **Nativní podpora operací (R1 III.C):** `managed_components/espressif__esp-nn` je deklarovaná závislost `espressif__esp-tflite-micro` (`idf_component.yml`: `espressif/esp-nn: '>=1.1.1'`) a je fyzicky přítomná v build stromu. ESP-NN poskytuje akcelerované kernely přesně pro kategorie operací, které tento model používá: `convolution`, `fully_connected`, `pooling`, `activation_functions`, `softmax`. Zbylé registrované ops (`Reshape`, `Shape`, `StridedSlice`, `Pack`) jsou lehké operace přesunu dat/metadat, akceleraci nepotřebují (nejsou výpočetně náročné). **Všechny výpočetně náročné operace tohoto modelu tedy běží nativně akcelerované na ESP32-S3.**
   - Repo/deska vráceny do výchozího stavu (`CONFIG_COMPILER_OPTIMIZATION_DEBUG=y`, jak bylo commitnuté) po dokončení srovnání — `sdkconfig` diff je po návratu prázdný, jediná trvalá změna je mikrosekundová přesnost v `main.cpp`/`validate_esp_uart.py`.

**Akceptační kritéria:**
- [x] Per-sample shoda <= 1 LSB — **splněno přesně** (max delta = 1 LSB, 0 přehozených predikcí, 855 vzorků přes všech 5 hladin šumu).
- [x] Release latence + Flash/RAM čísla připravená pro článek — **hotovo** (viz krok 6 výše; Debug vs. Release rozdíl 0,1 %, Flash 39 % app partition, DIRAM 41 %, IRAM 99,99 %, ESP-NN akcelerace potvrzena).

**Odhad:** 4–8 h. Skutečnost dosud: cca 3,5 h (build + flash ×3 + firmware úprava + debug + live validace).

---

## Rizika a protistrategie

| Riziko | Dopad | Protistrategie |
|---|---|---|
| Nová čísla se výrazně liší od publikovaných | Větší přepis Results v článku | Transparentní report (M4/M5); čísla se nepřizpůsobují, přepis textu kryje camera-ready plán (dny 3–6) |
| Legacy hypotéza (M4) proveniénci nepotvrdí | Diskrepance zůstane nevysvětlená | Poctivý reframe jako "open observation" — dle plánu z 21. 7. pro camera-ready přijatelné; zbylí podezřelí: váhy (M1), HW (M6) |
| Determinismus napříč prostředími (jiný CPU/BLAS -> drobné float32 rozdíly) | Cross-check vs referenci nevyjde bitově | Porovnávat na úrovni metrik (4 desetinná místa); mean ± std místo jediného čísla; odchylky do logu |
| 13 launch eventů v test splitu | Held-out čísla se širokými intervaly | Multi-seed mean ± std + explicitní caveat v článku; žádná statistická kosmetika |
| Kapacita do 31. 8. | Nestihne se M6 / přepis textu | Kritická cesta M0–M5 je jen cca 10–15 h; M6 je volitelné; přepis textu má vlastní plán (`BEC/.agents/memories/2026-07-21_camera_ready_plan.md`, dny 4–6) |

---

## Pracovní log

Formát záznamu: `YYYY-MM-DD | milník | co se stalo / zjištění / rozhodnutí`

- 2026-08-14 | příprava | Roadmapa vytvořena (průzkum repa, recenzí a kódu; rozhodnutí uzamčena po grill session). Ověřeno: PyPI dostupné; `generated/features` je symlink na `features_seed42`; manifest a splits čisté v gitu; nalezeno `/opt/esp` ESP-IDF prostředí (relevantní pro M6).
- 2026-08-14 | M0 (částečně, mimo pořadí) | Při přerušeném pokusu o implementaci byl smazán starý venv (měl rozbitý pip) a `task setup` doběhl s exit 0 — fresh venv existuje, ale smoke test (M0 krok 3) zatím neproveden. Záloha `results_ref_20260721` byla vytvořena a následně odstraněna při návratu změn — krok 1 je potřeba provést znovu.
- 2026-08-14 | M0 (dokončeno) | Záloha `generated/results_ref_20260721/` vytvořena znovu (7 CSV, shodné s `generated/results/`). Smoke test na venv prošel: tensorflow 2.21.0, librosa 0.11.0, sklearn 1.9.0, pandas 3.0.3, numpy 2.4.6 — přesně odpovídá pinovaným verzím v `requirements.txt`. Venv byl přebudován (`rm -rf venv && task setup`) už ráno v rámci přerušeného pokusu; smoke test tuto instalaci jen potvrdil, další rebuild nebyl potřeba. `task manifest` regeneroval 854 řádků (63 launch: 13 dana_artillery test + 50 train, plus other_gunshot 18 test/67 train; impulse_noise 140 test/566 train) a `splits.csv`; `git diff --exit-code` na obou souborech prošel s exit 0. Pozor: tento determinismus dokazuje reprodukovatelnost na tomto stroji a této verzi sklearn/knihoven (fixní `random_state=42` ve `generate_manifest.py`), nikoli přenositelnost mezi prostředími — to se ověřuje až v M3 cross-checkem proti `results_ref_20260721`. Nezávisle zkontrolováno review agentem (Opus): žádný blokující nález, plná shoda s `requirements.txt` (9/9 pinovaných balíčků), záloha bit-identická, manifest nezávisle reprodukován. Všechna akceptační kritéria M0 splněna, milník HOTOVO.
- 2026-08-14 | M1 (dokončeno) | **Zásadní zjištění: `archive/models/model.tflite` (firmware) nevznikl z `archive/models/najlepsi_model.h5`.** Rekonverze h5 stejným skriptem/representative datasetem dala jiný soubor (81 400 B vs 81 008 B originál) s odlišnou kalibrací vstupní aktivace (scale 8,92 vs 5,33) a 127/854 (15 %) přehozenými predikcemi proti archivnímu modelu na clean datech. Rozhodující test — přímé dekvantizování Conv2D/Dense kernelů a srovnání s float32 vahami z h5 (`ml/compare_weights.py`) — ukázal, že `reconverted.tflite` se od h5 liší jen o kvantizační šum (max diff 0,001–0,004 při scale ~0,002, kontrolní pár funguje jak se čeká), zatímco `archive/model.tflite` se od h5 liší o stovky kvantizačních kroků (max diff až 1,25 při scale ~0,001). Potvrzena větev (b): jde o dva různé natrénované modely stejné architektury, ne kvantizační artefakt. Toto posouvá narativ pro R4-major2 — srovnání Tabulky II (float32) a Tabulky III (int8) v původním článku nesrovnává float32/int8 verzi téhož modelu, ale dva různé modely. Podklad zapsán do `generated/reports/weights_provenance.md`. Nové skripty `ml/compare_tflite_weights.py`, `ml/compare_weights.py` — obojí commitnuto do gitu; výstupní CSV a rekonvertovaný `.tflite` jsou v `generated/` (gitignored), reprodukovatelné přiloženými příkazy.
- 2026-08-14 | M1 (doplněno, na dotaz uživatele) | **Identifikován pravděpodobný mechanismus vzniku dvou různých sad vah.** Originální trénovací skript `ml/main_cnn.py` (needitovaná verze z diplomky, odlišná od portované `ml/train_model.py`) na řádku 66 bezpodmínečně přepisuje `najlepsi_model.h5` (`model.save(...)`) při každém běhu, ale nikde nevolá `tf.random.set_seed()` — seedovaný je jen `train_test_split` (rozdělení dat), ne inicializace vah/trénovací trajektorie. Vlastní dřívější investigace (`REPRODUCTION_NOTES.md`, sekce 4, řádky 98–105) nezávisle potvrzuje, že tato citlivost je na tomto malém nevyváženém datasetu velká: retrénink na identických datech/hyperparametrech, lišící se jen seedem inicializace, dal val_accuracy 0,795 vs. 1,000. Pravděpodobný scénář: jeden běh `main_cnn.py` dal váhy kvantizované do firmware, pozdější běh přepsal `najlepsi_model.h5` bez zpětné rekvantizace. Git historie toto nerozliší (všechny tři archivní soubory pocházejí z jednoho squashnutého importního commitu `1146019`), ale mechanismus je podložen přímo kódem a nezávislým nálezem, ne spekulací. Doplněno do `generated/reports/weights_provenance.md` (nová sekce "Mechanismus vzniku") a do bodu 1.4.3 výše. Žádné nové skripty, jen text reportu a roadmapy.
- 2026-08-14 | M2 (dokončeno) | `ml/evaluate_robustness.py` rozšířen o `--split {all,train,test}`, odvozený feature root (`feature_index.parent` místo modulové konstanty) a `--per-sample-csv`. Regresní test beze nových flagů dal bitově shodná čísla s referencí (`results_ref_20260721/metrics_seed42.csv`, např. 30 dB accuracy 0,977751756440281) — žádná regrese. `--split test` na clean variantě vrátil přesně 171 vzorků. Per-sample CSV s 2 režimy × 5 variant × test split dal 1710 řádků, sloupec `split` obsahoval jen `test`. Feature root ověřen i na `generated/features_seed43/features_manifest.csv` — načetl jiná (očekávaně odlišná) čísla než seed 42, čímž je potvrzeno, že M3 může iterovat přes seedy beze zvláštní úpravy skriptu. Všechna akceptační kritéria splněna, milník HOTOVO.
- 2026-08-14 | validace M0-M2 (dva nezávislé Opus review agenty) | Na žádost uživatele proběhl dvojitý nezávislý review celého postupu M0–M2 — jeden agent na metodologickou/vědeckou správnost závěrů, druhý na kód/engineering/git hygienu; oba spustily skripty samy, nevěřily jen textu roadmapy. **Metodologický review:** potvrdil M1 závěr a zesílil ho (korelace dekvantizovaných vah archiv-vs-h5 -0,02 až 0,09 na všech 4 vrstvách, vs. 0,9999–1,0000 u kontrolního páru); sám spustil chybějící experiment float32 h5 vs. skutečný int8 `reconverted.tflite` a zjistil, že kvantizace na stejném modelu MCC prakticky nezmění — doporučil povýšit tento experiment z "volitelného" na klíčový nález; upozornil, že analogie na seed-sensitivity z `REPRODUCTION_NOTES.md` je slabší, než jak byla prezentovaná (jiný běh, jiný korpus). M0 determinismus a M2 regresní test oba nezávisle potvrdil jako genuinní, ne fragilní. **Engineering review:** potvrdil správnost dekvantizace/transpozice os v `compare_weights.py`, bit-identický M2 regresní test, čistou git hygienu (autorství jen Martin Maxa, žádný AI co-author, conventional commits bez scope, žádné `generated/`/`venv/` v gitu); našel jeden reálný bug — `ml/compare_tflite_weights.py` četla `.npy` z natvrdo zapsané `FEATURE_ROOT` konstanty místo `feature_index.parent` (stejná chyba jako před M2 úpravou), neškodilo aktuálnímu výsledku ale bylo nášlapnou minou pro M3. **Akce po review:** oprava feature-root bugu (ověřeno na seed43 — funguje, čísla se nezměnila), doplnění korelační statistiky do `compare_weights.py` a `weights_provenance.md`, zmírnění formulace seed-sensitivity analogie, formální doběhnutí a zdokumentování rozhodujícího experimentu (`generated/reports/quantization_effect_same_model.csv`) — vše zapsáno zpět do M1 sekce výše a do `generated/reports/weights_provenance.md`.
- 2026-08-14 | M3 (dokončeno, formulace v tomto záznamu následně opravena po review — viz další záznam) | Napsán `ml/reproduce.py`, orchestrátor volající `run_regime()` z M2 přímo (import, ne subprocess). Všech 5 seed feature adresářů již existovalo, generování featur se přeskočilo. Kompletní běh: 100 řádků metrik (5 seedů x 2 režimy x 2 scopes x 5 variant). Cross-check proti referenci z 21. 7.: `metrics_seedN.csv` pro N=42..46 bitově identické (diff bez výstupu); agregát `per_seed_metrics.csv` hodnotově identický (nulový absolutní rozdíl ve všech metrikách přes všech 100 kombinací), ale ne bitově (pořadí řádků/konce řádků se liší — upřesněno v M3 kroku 2 výše). Objeveno zajímavé zjištění: referenční `per_seed_metrics.csv` z 21. 7. už měl přesně cílové schéma (`mode,variant,seed,scope,n,accuracy,...`), což naznačuje, že podobný orchestrátor už jednou vznikl v dřívější, jinak nezachované session — `ml/reproduce.py` teď tento výstup formálně reprodukuje a je committnutý. Opakovatelnost ověřena (druhý běh seed 42 bitově identický). Všechna akceptační kritéria splněna, milník HOTOVO.
- 2026-08-14 | validace M3 (dva nezávislé Opus review agenty) | Na žádost uživatele proběhl dvojitý review M3, stejný vzorec jako u M0-M2. **Engineering review:** kód potvrzen správný a deterministický (formát variant sedí mezi všemi 3 skripty, `write_csv` nelze zavolat s prázdným seznamem, `groupby(sort=False)` je deterministické, subprocess volání `prepare_features.py` ověřeno dry-run testem na dočasném seedu, git hygiena čistá) — žádný must-fix nález. **Metodologický review přinesl 2 zásadní opravy a 2 doporučení:** (1) tvrzení "bitově identické" bylo v pracovním logu použito jako zastřešující nadpis i pro agregát `per_seed_metrics.csv`, který je ve skutečnosti jen hodnotově identický (jiné pořadí řádků, CRLF vs LF) — opraveno výše i v tomto zápisu; (2) sanity kontrola "v rámci 1 std" byla chybná i početně (přepočítáno: seed42 vs. mean 5 seedů se liší o 1,00 std při 30 dB int8, 0,84 std při 5 dB int8 — ne "v rámci", ale přesně v očekávaném řádu odchylky jednoho vzorku od průměru) a navíc kruhová (čísla v REPRODUCTION_NOTES JSOU seed 42, ne nezávislý zdroj) — opraveno na čestnou formulaci v M3 kroku 3. Doporučení: (3) cross-check nepřegeneroval featury (byly na disku), takže testuje jen "stejný stroj, stejné cache soubory" — ne přenositelnost ani determinismus MFCC/šum extrakce; zdokumentováno jako mezera pro budoucí re-run. (4) std přes seedy zachycuje jen šumovou varianci, ne nejistotu z 13 launch eventů v test splitu — held-out int8 @30dB má std přesně 0, což by se dalo mylně číst jako "žádná nejistota"; caveat doplněn do M3 kroku 5 pro použití v M5. (5) Oba review doporučili zvážit rozšíření multi-seed ošetření i na `reconverted.tflite` (rozhodující kvantizační pár z M1, dosud jen seed 42) — vyřešeno v následujícím záznamu.
- 2026-08-14 | M1 rozšíření (na žádost uživatele po review M3) | Rozhodnuto rozšířit multi-seed ošetření na rozhodující kvantizační pár. `ml/reproduce.py` parametrizován (`--model-keras/--model-tflite/--keras-label/--tflite-label/--results-root/--no-legacy-csv`), regresně ověřeno, že výchozí volání (bez nových argumentů) dává stejný výsledek jako před úpravou. Spuštěno na `najlepsi_model.h5` vs. `generated/models/reconverted.tflite` přes seedy 42–46, výstup do `generated/reports/quantization_effect_multiseed/` (oddělené od `generated/results/`, aby se neohrozily ověřené M3 výstupy — po testovacích bězích byl plný 5-seed `generated/results/per_seed_metrics.csv` obnoven a zkontrolován, že má 100 řádků). Výsledek: kvantizace na stejném modelu je lepší jen v 1 z 10 kombinací varianta×scope, horší v 9 z 10, průměrný efekt MCC −0,0115 (std 0,013) — mírně **zhoršuje** robustnost, nikoli zlepšuje. Silnější a jednoznačnější potvrzení jednoseedového nálezu M1. Zapsáno do `generated/reports/weights_provenance.md` (nová podsekce "Rozšíření na 5 seedů") a do M1 sekce roadmapy výše (kroky 7–8, aktualizované Výstupy).
- 2026-08-14 | M4 (dokončeno) | Napsán `ml/reproduce_legacy.py`, věrná replika bugu z `ml/eval_tflite_pc.py:54-70`. **Předběžná matematická analýza** (než proběhl běh): kvantizace výstupu `archive/model.tflite` má `zero_point=-128` (z M1); pro tuto hodnotu bug matematicky garantuje, že každá predikce s pravděpodobností `>= 0.5` wrapne na záporné číslo — predikce zněla "recall 0,00 na všech úrovních šumu". **Empirický běh přesně potvrdil predikci:** recall 0,00 na clean i všech 4 SNR úrovních, accuracy konstantní 92,62 % (= přesný podíl non-launch vzorků 791/854 — model "predikuje" úplně vše jako třídu 0). Srovnání s publikovanou Tabulkou III (recall 0,90–1,00) a opraveným M3 během (recall 0,76–0,98, seed 42): legacy-bug běh je v naprostém rozporu s oběma, ne jen "trochu jiný". **Potvrzena větev (b): hypotéza "PC vs ESP diskrepance = artefakt tohoto bugu" je vyvrácena.** Pokud by Tabulka III vznikla tímto skriptem v aktuální podobě, recall by byl nulový, ne 0,90–1,00 — publikovaná čísla musí vzniknout jinak. Zbývá M1 nález (různé váhy) jako hlavní kandidát pro vysvětlení PC-vs-ESP diskrepance; HW rozdíly (M6) jako druhý, dosud neprověřený. Zapsáno do `generated/reports/provenance_table3.md` a do roadmapy (sekce 1.4 bod 2, 1.5 mapování, M4 sekce). Všechna akceptační kritéria splněna, milník HOTOVO.
- 2026-08-14 | M5 (probíhá) | Napsán `ml/make_deliverables.py` — generuje `table2_float32.tex`/`table3_int8.tex` (styl shodný s `article_main.tex`), `reviewer_response.md` (5 sekcí dle recenzí) a `deliverables_data.json`, vše z CSV bez ručních čísel. `Taskfile.yml`: `reproduce:pc` rozšířeno oproti plánu — zahrnuje i M1 kroky (proveniénce vah + kvantizační experiment na 5 seedech), ne jen M3/M4, aby byl harness skutečně kompletní; `reproduce:clean` chrání `weights_provenance.md`/`provenance_table3.md` (ručně psané, nejsou regenerovatelné skriptem) i `results_ref_20260721/`. **Na žádost uživatele proveden skutečný end-to-end test:** `task reproduce:clean && task reproduce:pc` od nuly (smazány i featury pro všech 5 seedů, ~30 min regenerace) — po zálohování `generated/{results,reports,models}` do scratchpadu. Výsledek: všech 5 `metrics_seedN.csv` bitově identických s referencí **i po regeneraci featur** (dřív testováno jen s cache featurami — uzavírá mezeru z review M3); všechny M1/M4/M5 výstupy identické s předchozím cache během. Oba hand-authored reporty přežily clean nedotčené. LaTeX tabulky ověřeny jen staticky (vyvážené závorky, existující balíčky) — `pdflatex` není v tomto prostředí dostupný, skutečný build neproveden. Zbývá: HTML artifact.
- 2026-08-14 | M5 (dokončeno) | Načten skill `artifact-design`, vytvořen jednostránkový HTML artifact "evidence review" pro spoluautory/školitele: přehledový pruh 5 karet (verdikt confirmed/refuted/measured/partial) s odkazy na detailní sekce, pak sekce proveniénce vah (korelační stupnice), kvantizační efekt (divergentní bar chart 10 kombinací s int8−float32 rozdílem), held-out vs. full-corpus (skupinové pruhy MCC), legacy bug (tabulka predikce/měření/publikováno), a fakta/mezery pro M6. Design: chladně neutrální papírová paleta (ne cream/ne near-black-s-neonem, aby se vyhnul klišé z designové skill), akcentová barva mosaz (ordnance brass, tematicky vázaná na artilerii/nábojnice), bipolární korelační stupnice opravena z "plněného pruhu" (zavádějící u hodnot blízkých nule) na "marker na ose". Všechny číselné hodnoty v HTML ověřeny proti `deliverables_data.json` a `summary_mean_std.csv` (žádné lorem/vymyšlená čísla). Publikováno: <https://claude.ai/code/artifact/b0cf34ae-c933-40fd-9292-b51e0c44360b>. Všechna akceptační kritéria M5 splněna, milník HOTOVO. **Kritická cesta M0–M5 (odhad 10–15 h) je tímto uzavřena** — zbývá jen volitelný M6 (ESP32-S3 hardware).
- 2026-08-14 | M6 (probíhá, pozastaveno) | Deska se objevila připojená přes USB/IP passthrough (`/dev/ttyACM0`), ale `open()` nejdřív visel na neurčito (ověřeno přes pyserial i syscall `os.open()`, i bez sandboxu) — diagnostikováno jako nestabilní USB/IP tunel (dmesg ukazoval opakované "connection reset by peer" cykly), ne aplikační/kódový problém. Po uživatelově zásahu na hostitelské straně se port stabilizoval. **`task firmware:build` úspěšný** — ESP-IDF v5.4 z `/opt/esp` sestavilo firmware bez úprav (binárka 0x65ef0 B, 60 % volno), čímž se v praxi vyřešila nejasnost verze ESP-IDF z blokerů (deklarace >=4.1.0 vs lock 5.2.1 vs Docker v5.4 — v5.4 funguje). **Flash úspěšný** — `idf.py -p /dev/ttyACM0 flash` nahrál bootloader+app+partition table na reálnou desku, identifikovanou jako ESP32-S3 (QFN56 rev v0.2, WiFi/BLE, 8 MB PSRAM, MAC 98:3d:ae:61:1d:18) — potvrzuje, že jde o skutečný hardware, ne emulátor. Ověřen firmware kód (`main.cpp:164`): C++ dequantizace výstupu se automaticky promuje `int8_t`→`int32_t` při odečtení od `zero_point`, takže na desce int8 overflow bug z M4 nehrozí (jen na PC/numpy straně). Opraven `ml/validate_esp_uart.py` (threshold `>`→`>=`, `--per-sample-csv`). **Živá UART komunikace blokovaná:** smoke test (3 vzorky) i passivní poslech vrátily 0 bajtů oběma směry; `write()` hlásí úspěch, ale nic se nevrací, ani boot log. Diagnostika (RTS reset, čerstvé otevření, přímý syscall) nerozlišila příčinu (firmware vs. tunel). Uživatel není fyzicky u desky (nemůže zkontrolovat LED/reset tlačítko) — **na jeho žádost pozastaveno**, dokud nebude možná fyzická kontrola nebo stabilnější spojení. M6 zůstává mimo kritickou cestu k camera-ready deadline.
- 2026-08-14 | M6 (pokračování) | Uživatel navrhl hypotézu: deska má dva USB-C porty (nativní USB-Serial/JTAG vs. UART0 přes samostatný převodník), appka možná mluví na jiný port, než je připojený. Ověřeno v `sdkconfig` (`CONFIG_ESP_CONSOLE_UART_NUM=0`) — pravděpodobné. Firmware upraven: nový Kconfig přepínač `CONFIG_APP_DATA_LINK_USB_SERIAL_JTAG` (`main.cpp`, `Kconfig.projbuild`, `CMakeLists.txt` — `esp_driver_usb_serial_jtag` jako závislost), default `n` (beze změny chování), `y` přesměruje datový spoj na nativní USB-Serial/JTAG. Regresní build s `n` ověřen (velikost binárky prakticky nezměněná). Build s `y` a reflash na `/dev/ttyACM0` — odpověď stále nulová, ověřeno na úrovni ELF symbolů (`nm`), že se zkompilovala správná cesta kódu (hypotéza portu tedy nebyla příčina). V `dmesg` přesně v čase testu nalezeno `vhci_hcd: urb->status -104` (ECONNRESET) s "unlink" událostmi — **[formulace opravena, viz záznam o dva níže: tohle NENÍ přímý důkaz, jen korelace — tentýž vzor se objevil i během pozdějšího úspěšného běhu]**. `sdkconfig` vrácen na výchozí (`git checkout`), přebuildnut čistě, aby default `CONFIG_APP_DATA_LINK_USB_SERIAL_JTAG is not set` vzniklo přirozeně přes Kconfig (ne ručním zápisem).
- 2026-08-14 | M6 (vyřešeno) | Uživatel rebuildnul kontejner a přepojil desku na druhý USB-C port (fyzický UART-to-TTL převodník). ESP-IDF v `/opt/esp` přežilo rebuild. `dmesg` ukázal stejný ECONNRESET vzor už 10 minut po startu kontejneru (potvrzuje: trvalý problém tunelu, nezávislý na kontejneru/portu/firmwaru) — ale tentokrát flash i UART data **prošly bez jediné chyby**. Build s výchozím (UART0) nastavením přeflashován. Smoke test (3 vzorky): reálné odpovědi, `latency_ms=32` (přesně publikovaná hodnota). Plný test split (171 vzorků) na 3 variantách (clean, 30dB, 5dB) — 0 chyb ze 513 volání. **Per-sample srovnání proti opravenému PC-int8 (seed 42):** max delta = 0,003906 = přesně 1 kvantizační krok (1 LSB, scale 0,00390625 z M1), **0 přehozených predikcí ze všech 513 vzorků**. Akceptační kritérium M6 ("shoda <=1 LSB") splněno přesně. **Zásadní důsledek:** PC-int8 (opravený) a ESP32-int8 se na stejném kanonickém modelu shodují v rámci 1 LSB — PC-vs-ESP32 diskrepance z původního článku (R2#6/R3/R4-major1) tedy není inherentní rozdíl platforem, ale musela vzniknout nekonzistentní metodikou/modelem při přípravě původních čísel. Silně doplňuje M1 nález. Zbývá krok 6 (Release build, `idf.py size`, ops podpora pro R1 III.C) — hardware je momentálně dostupné.
- 2026-08-14 | validace M6 (dva nezávislé Opus review agenty) | Na žádost uživatele proběhl dvojitý review M6, stejný vzorec jako u M0-M3. Metodologický agent nedostal přístup k desce (aby nekolidoval s druhým agentem), engineering agent ano — provedl vlastní live smoke test (5 vzorků, reálné odpovědi, latency 32 ms) a nezávisle přepočítal per-sample srovnání z uložených CSV. **Engineering review: žádný must-fix nález** — `link_read`/`link_write`/`init_data_link` abstrakce v `main.cpp` korektní, UART0 větev bit-přesně odpovídá původnímu kódu, threshold fix a `--per-sample-csv` v `validate_esp_uart.py` bez chyby, git hygiena čistá (žádné `build/` artefakty v gitu, žádný AI co-author), jen kosmetická poznámka k potenciálnímu 1s blokování v nepoužívané USB-JTAG větvi. **Metodologický review potvrdil čísla nezávislým přepočtem** (shoda přesná, žádné cherry-picking v distribuci — medián i mean odpovídají), ale identifikoval 3 problémy s formulací/rozsahem: (1) test pokrýval jen 3 z 5 hladin šumu (clean/30dB/5dB, chyběly 20dB a 10dB) — doporučeno doplnit, protože deska byla dostupná; (2) kauzální tvrzení "diskrepance musela vzniknout nekonzistentní metodikou" bylo přehnané — test dokazuje jen "rozdíl platforem je vyloučen jako vysvětlení", ne přímo příčinu původní diskrepance; (3) "ECONNRESET potvrzuje příčinu" bylo přehnané — týž vzor se objevoval i během pozdějšího úspěšného běhu, což korelaci jako důkaz kauzality podkopává; podobně "vyloučení hypotézy portu" stálo jen na `nm` ověření, ne na funkčním přenosu s tou konfigurací. **Akce po review:** doplněny chybějící hladiny 20dB a 10dB (deska byla stále dostupná) — kompletní pokrytí 855 vzorků přes všech 5 úrovní, výsledek nezměněný (max delta 1 LSB, 0 flipů). Zmírněny formulace v M6 sekci (blokery, krok 5, důsledek) na eliminační argument místo přímého kauzálního tvrzení; ECONNRESET popsán jako nejistá korelace, ne potvrzená příčina.
- 2026-08-14 | M6 krok 6 (dokončeno) | Zvýšena přesnost měření latence z celých ms na mikrosekundy (`main.cpp` `%d us`, odpovídající úprava `ml/validate_esp_uart.py`: `latency_ms`→`latency_us`) — původní celočíselné ms zaokrouhlení by mohlo skrýt rozdíl Debug/Release. Release build (`CONFIG_COMPILER_OPTIMIZATION_PERF`): 30 vzorků, 32,777 ms průměr. Debug build (stejný mikrosekundový firmware, pro čisté srovnání): 30 vzorků, 32,818 ms průměr. **Rozdíl 0,04 ms (0,1 %) — zanedbatelný.** Publikovaná latence ~32 ms tedy neni artefakt Debug buildu; pravděpodobně kvůli ESP-NN akcelerovaným kernelům, na které úroveň optimalizace C++ nemá vliv. `idf.py size` (Release): Flash Code 197 872 B, Flash Data 142 272 B, DIRAM 140 019/341 760 B (40,97 %, `tensor_arena` ~93 % z `.bss`), **IRAM 16 383/16 384 B (99,99 %, téměř plná)**, RTC FAST 52/8192 B, celková image 408 582 B = 39 % z 1 MB app partition. Nativní podpora ops (R1 III.C): `espressif__esp-nn` je deklarovaná a přítomná závislost `esp-tflite-micro`, akceleruje `convolution`/`fully_connected`/`pooling`/`activation_functions`/`softmax` — přesně ty kategorie, které model používá; zbylé ops (`Reshape`/`Shape`/`StridedSlice`/`Pack`) jsou lehké a akceleraci nepotřebují. Repo/deska vráceny na `CONFIG_COMPILER_OPTIMIZATION_DEBUG=y` (výchozí commitnutý stav) — `sdkconfig` diff po návratu prázdný. **M6 tímto kompletně hotové** (kromě volitelného doplnění formálního review, které již proběhlo v předchozím záznamu).
- 2026-08-14 | validace celé roadmapy M0–M6 (dva Opus + jeden Fable agent) | Na žádost uživatele proběhl komplexní review celého efortu (ne jednotlivého milníku) — dva nezávislé Opus agenty (metodologický + engineering) prošly všech 16 commitů a celou roadmapu, poté Fable agent nezávisle ověřil nejdůležitější nálezy obou a sepsal finální konsolidovaný report. **Celkový verdikt obou Opus agentů i Fable:** evidence chain (M1 jiné modely → M4 bug vyvrácen → M6 platforma vyloučena) je solidní a sebekritický, pipeline je reprodukovatelný (`make_deliverables.py` dal bitově identický výstup při čerstvém běhu). **Dva nálezy vyžadovaly opravu:**
  1. **`REPRODUCTION_NOTES.md` sekce 3 odporovala `reviewer_response.md` sekci 2** — sekce 3 stále tvrdila, že se kvantizační efekt "reprodukuje čistě a nezávisle" (0,52→0,86 MCC), přesně na páru vah, který M1 prokázalo jako dva různé modely; `reviewer_response.md` toto tvrzení naopak odvolává. Banner na začátku souboru odkazoval na M1–M4 jako vyřešené, ale tělo sekce 3 nebylo upraveno. **Fable navíc odhalil, že se to skládá s druhým nálezem** — banner explicitně doporučuje `task reproduce:clean` jako reset, tedy dokument s rozporem navádí čtenáře na příkaz, který by smazal nenahraditelná M6 HW data (viz níže). Opraveno: sekce 3 dostala inline korekční poznámku s odkazem na `reviewer_response.md`, banner aktualizován na M0–M6.
  2. **`task reproduce:clean` nechránilo `esp32_per_sample_*.csv`** (5 souborů, reálná HW měření z M6, gitignored, nikde jinde nezálohovaná, `reproduce:pc` je neregeneruje — deska není v CI). Opraveno: `Taskfile.yml` výjimka rozšířena o `! -name 'esp32_per_sample_*.csv'`; soubory navíc zálohovány do scratchpadu pro jistotu.
  **Menší nálezy, také opraveny:** stará log formulace "přímý důkaz" u ECONNRESET (řádek ~519) oznámkována jako opravená/superseded; `ml/validate_esp_uart.py:56` měl stejný feature-root bug jako dřív `compare_tflite_weights.py` (`FEATURE_ROOT` konstanta místo `args.features.parent`) — opraveno a regresně ověřeno na desce (latence 32,824 ms, odpovídá); `table3_int8.tex`/`table2_float32.tex` dostaly obecnou footnote k neplynulosti held-out metrik na hranici 13 launch eventů; `reviewer_response.md` formulace "collapse to the same numbers" upřesněna na "on the tested seed" (1-LSB shoda byla ukázána na seed 42, ne na celém multi-seed korpusu). **Co review agenti naopak vyzdvihli jako bez výhrad:** M4 (falsifikovatelná predikce před během), zdůvodnění clean-variant delty jako tiskového zaokrouhlení, statistická poctivost (caveat 13 eventů, rozlišení std-jako-šumové-variance vs. sampling nejistota), git hygiena (žádný AI co-author, konzistentní conventional commits) a kompletnost `reproduce:pc` řetězu mimo záměrně manuální HW krok.
