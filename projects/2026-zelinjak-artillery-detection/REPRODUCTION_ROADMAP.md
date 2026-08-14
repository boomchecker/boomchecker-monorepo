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
| M2 | Úpravy evaluačních skriptů | NEZAHÁJENO | M0 | 1–2 h |
| M3 | Multi-seed reprodukční běh | NEZAHÁJENO | M1, M2 | 2–4 h |
| M4 | Legacy proveniénční běh | NEZAHÁJENO | M2 | 2–3 h |
| M5 | Deliverables | NEZAHÁJENO | M3, M4 | 3–4 h |
| M6 | ESP32-S3 hardware (volitelné) | NEZAHÁJENO | M5 + deska | 4–8 h |

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
2. **Podezření na bug v publikovaných PC-int8 číslech:** legacy skript `ml/eval_tflite_pc.py:68` dequantizuje `(output - zero_point) * scale` bez rozšíření typu; pod numpy 2.x (NEP 50) int8 aritmetika přetéká (např. 127 − (−128) wrapne). Opraveno v `ml/evaluate_pc.py:49-53`. Pokud Table III PC-int8 vznikla legacy skriptem, diskrepance PC vs ESP je artefakt bugu.
3. **Potvrzeno v M1 (2026-08-14): různé váhy, mechanismus identifikován.** Firmware `model_data.h` (81 008 B, bit-identický s `archive/models/model.tflite`) **nevznikl** kvantizací `archive/models/najlepsi_model.h5`. Dekvantizované Conv2D/Dense kernely `model.tflite` se od float32 vah v `najlepsi_model.h5` liší o desítky až stovky kvantizačních kroků (max diff 0,40–1,25 při scale ~0,0008–0,0022) — o dva až tři řády víc než u kontrolního páru (rekonverze `najlepsi_model.h5` sama proti sobě: max diff 0,001–0,004). 15 % vzorků (127/854) má přehozenou predikci mezi oběma `.tflite` soubory. Jde o dva různé natrénované modely stejné architektury, ne o kvantizační artefakt. **Příčina:** originální trénovací skript `ml/main_cnn.py` nikde nevolá `tf.random.set_seed()` (seedovaný je jen `train_test_split`) a na konci bezpodmínečně přepisuje `najlepsi_model.h5` (`model.save(...)`, řádek 66) — dva různé běhy tohoto skriptu na identických datech tedy dají různé váhy. Že je tato citlivost velká, dokládá nezávisle i vlastní retrénink v `REPRODUCTION_NOTES.md` sekce 4 (val_accuracy 0,795 vs. 1,000 při změně jen seedu inicializace). Nejpravděpodobněji: jeden běh `main_cnn.py` dal váhy zapečené do firmware, druhý (pozdější) běh přepsal `najlepsi_model.h5` bez rekvantizace. Detaily a plný postup: `generated/reports/weights_provenance.md`.
4. **Eval na trénovacích datech:** robustnostní sweep běžel na celém korpusu (většina vzorků viděna při tréninku). Split existuje (80/20 stratified, seed 42, `datasets/recordings/splits.csv`), ale test split má jen **13 launch eventů** — held-out čísla budou statisticky slabá, nutný caveat.
5. Předchozí reprodukce (archivní model, korpus 854, opravený eval) dává stejné trendy, ale čísla o něco nižší než v článku — např. int8 full-corpus: 30 dB Acc 97,78 % / MCC 0,86; 5 dB Acc 85,71 % / MCC 0,42.

### 1.5 Výtky recenzentů a kde je řešíme

| Výtka | Recenzenti | Milník |
|---|---|---|
| PC-int8 vs ESP32-int8 diskrepance stejného modelu | R2#6, R3, R4-major1, R4-Q1 | M1 (váhy), M4 (bug), M6 (HW důkaz) |
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
   - **Potvrzena větev (b):** `archive/models/model.tflite` nevznikl z `najlepsi_model.h5`. Jde o dva různé natrénované modely stejné architektury (72 193 parametrů). Do reviewer-response (M5) jde jako přímý podklad pro R4-major2 — silnější, než audit předpokládal (ne artefakt preprocessingu, ale nesrovnatelné modely). V M3 se oba kanonické artefakty reportují explicitně jako "as-shipped", bez tvrzení o identitě; zvážit doplňkovou třetí sadu čísel z `generated/models/reconverted.tflite` jako opravdový float32/int8 pár téhož modelu (rozhodnutí uživatele, mimo rozsah M1).

**Výstupy:**
- `generated/reports/weights_provenance.md` (plný závěr, tabulky, reprodukční příkazy).
- `generated/reports/weights_provenance_per_sample.csv` (854 řádků, skóre obou modelů vedle sebe).
- `generated/reports/weights_layers_archive_vs_h5.csv`, `weights_layers_reconverted_vs_h5.csv` (per-vrstva diff).
- Nové skripty: `ml/compare_tflite_weights.py`, `ml/compare_weights.py`.
- `generated/models/reconverted.tflite`, `reconverted_model_data.h` (gitignored, reprodukovatelné z `najlepsi_model.h5`).

**Akceptační kritéria:**
- [x] Jednoznačný závěr "stejné váhy ano/ne" podložený per-sample statistikou (max delta, počet flipů) i přímým srovnáním vah.
- [x] Závěr formulovaný tak, aby šel přímo citovat v odpovědi recenzentům (`generated/reports/weights_provenance.md`, sekce "Důsledky pro camera-ready a odpověď recenzentům").

**Odhad:** 1–2 h. Skutečnost: cca 1 h.

---

## M2 — Úpravy evaluačních skriptů (held-out, multi-seed, per-sample audit)

**Stav:** NEZAHÁJENO

**Cíl:** Rozšířit `ml/evaluate_robustness.py` o held-out eval, podporu multi-seed feature rootů a per-sample výstup — při zachování stoprocentní zpětné kompatibility (stávající Taskfile tasky se nesmí změnit chováním).

**Proč:**
- Held-out čísla (`--split test`) jsou přímá odpověď na R3 a R4-major3 ("results on data not used in training").
- Per-sample CSV je nutný předpoklad pro porovnání PC vs ESP32 s tolerancí 1 LSB v M6 a pro jakýkoli audit jednotlivých predikcí.
- Konfigurovatelný feature root je předpoklad multi-seed smyčky v M3 (dnes je `FEATURE_ROOT` konstanta na řádku 15).

**Metoda:** Minimální, zpětně kompatibilní změny — všechny nové parametry mají defaulty odpovídající dnešnímu chování.

**Kroky:**

1. `ml/evaluate_robustness.py`:
   - Nový argument `--split {all,train,test}` (default `all`). V `load_variant()` (dnes ř. 22–36) filtrovat `index["split"] == split` pro `split != "all"`.
   - `FEATURE_ROOT` odvozovat z adresáře `--features` (rodič manifestu featur) místo modulové konstanty — cesty `feature_path` v manifestu jsou relativní k němu; tím začnou fungovat `generated/features_seedN/`.
   - Nový argument `--per-sample-csv PATH`: zapsat řádek na vzorek se sloupci `mode, variant, split, recording_id, class_id, score, pred`.
2. Regresní kontrola: běh bez nových flagů na seed42 featurách musí dát **identická čísla** jako před úpravou (porovnat výstup před/po).
3. Sanity kontrola held-out: `--split test` na clean variantě vrátí 171 vzorků (13 launch / 158 non-launch).

**Výstupy:** Upravený `ml/evaluate_robustness.py`.

**Akceptační kritéria:**
- [ ] Bez nových flagů je výstup bitově shodný s chováním před úpravou (žádná regrese).
- [ ] `--split test` vrací správně filtrované počty vzorků.
- [ ] Per-sample CSV obsahuje řádek pro každý vzorek každého variantu.

**Odhad:** 1–2 h. Lze dělat souběžně s M1.

---

## M3 — Multi-seed reprodukční běh (nová sjednocená camera-ready čísla)

**Stav:** NEZAHÁJENO

**Cíl:** Kompletní robustnostní sweep: {clean, 30, 20, 10, 5 dB} x {float32, int8} x {full-corpus, held-out test} x {seed 42–46}, agregovaný na mean ± std. Toto jsou čísla pro novou Tabulku II/III camera-ready verze.

**Proč:** Sjednocený eval (jeden model, jeden preprocessing, opravená dequantizace, jednotný threshold `>=`) odstraňuje všechny známé zdroje nekonzistence mezi režimy — přímá odpověď na R4-major2. Held-out sloupec odpovídá R3/R4-major3. Multi-seed kvantifikuje varianci šumu — nutné, protože test split má jen 13 launch eventů a jedna realizace šumu je statisticky slabá.

**Metoda:** Nový orchestrátor `ml/reproduce.py` (čisté CLI bez config souboru — konzistentní se stylem repa). Featury per seed se regenerují deterministicky (`np.random.seed(seed)` v `prepare_features.py`, ř. 65; pořadí dané manifestem). Kanonické modely z `archive/models/` (rozhodnutí #2).

**Kroky:**

1. Napsat `ml/reproduce.py`:
   - Pro každý seed z {42, 43, 44, 45, 46}:
     ```
     prepare_features --include-noisy --snr-db 30 20 10 5 --seed N --output generated/features_seedN
     ```
     Přeskočit, pokud adresář existuje a není zadán `--force` (šetří cca 1 h výpočtu při opakovaných bězích).
   - Pro každý seed 4 kombinace evalu přes `evaluate_robustness`:
     - `--model-keras archive/models/najlepsi_model.h5` x `--split all` a `--split test`
     - `--model-tflite archive/models/model.tflite` x `--split all` a `--split test`
     - vždy s `--include-clean` a `--per-sample-csv`.
   - Agregace: `generated/results/per_seed_metrics.csv` (všechny kombinace) a `generated/results/summary_mean_std.csv` (group by mode x variant x scope, mean ± std přes seedy).
2. **Cross-check determinismu:** porovnat nové `metrics_seedN.csv` (full-corpus část) s referencí `generated/results_ref_20260721/`. Shoda -> pipeline je stabilní napříč prostředími. Neshoda -> analyzovat příčinu (verze knihoven, oprava splitu, pořadí vzorků) a **zdokumentovat v Pracovním logu — ne zamlčet ani nepřizpůsobovat**.
3. Sanity kontroly proti REPRODUCTION_NOTES (sekce 3): int8 full-corpus při 30 dB má vyjít cca Acc 97,8 % / MCC 0,86; float32 při 30 dB cca Acc 84,4 % / MCC 0,52.
4. Opakovatelnost: druhý běh stejného seedu musí dát bitově identické CSV.

**Výstupy:**
- `generated/features_seed42..46/` (regenerované featury).
- `generated/results/metrics_seed42..46.csv`, `per_sample_seed42..46.csv`.
- `generated/results/summary_mean_std.csv` — hlavní zdroj čísel pro M5.

**Akceptační kritéria:**
- [ ] Kompletních 5 seedů x 2 režimy x 2 scopes x 5 variant = 100 řádků metrik.
- [ ] Dvojí běh stejného seedu = bitově identické CSV.
- [ ] Shoda s referencí 2026-07-21 potvrzena, nebo odchylka vysvětlena v logu.
- [ ] Sanity čísla v očekávaném pásmu (viz krok 3).

**Odhad:** 2–4 h (z toho cca 1 h čistý výpočet: 854 WAV x 5 variant x 5 seedů MFCC extrakce + inference).

---

## M4 — Legacy proveniénční běh (původ publikovaných čísel)

**Stav:** NEZAHÁJENO

**Cíl:** Rozhodnout, zda publikovaná PC-int8 čísla (Tabulka III) jsou artefakt int8 overflow bugu v legacy evaluačním skriptu.

**Proč:** Nejtvrdší výtka recenzí (R2#6, R4-major1): tři inferenční režimy si odporují, ačkoli stejný int8 model na stejných vstupech musí dávat prakticky identická čísla. Kandidátní mechanismus: `ml/eval_tflite_pc.py:68` — `pred_prob = (output_data[0][0] - zero_point) * scale` s úzkými int8 operandy; pod numpy 2.x (NEP 50) rozdíl přetéká (např. 127 − (−128) = wrap místo 255). ESP32 C kód (`firmware/esp32s3/main/main.cpp:160-164`) typ rozšiřuje správně — bug se týká jen PC strany. Pokud legacy běh reprodukuje publikovaná PC-int8 čísla a opravený běh dává čísla blízká ESP32, je diskrepance vysvětlena mechanicky a "quantization improves robustness" se reframuje.

**Metoda:** Nový `ml/reproduce_legacy.py` — věrně replikuje legacy dequantizaci (úzké int8 odečítání, NEP 50 chování), ale načítá featury z manifest-based struktury (legacy hardcoded složky `gunshots_mfcc_audio_noise` atd. neexistují). Full-corpus, featury seed 42, model `archive/models/model.tflite`. Vedle toho identický běh s opravenou dequantizací (z M3) pro přímý diff.

**Kroky:**

1. Napsat `ml/reproduce_legacy.py`; bug replikovat přesně (datové typy, pořadí operací dle `eval_tflite_pc.py:66-68`); zbytek pipeline (načítání, threshold `>=`, metriky) shodný s opraveným evalem, aby jediný rozdíl byla dequantizace.
2. Běh na `generated/features_seed42`, full-corpus, `archive/models/model.tflite`, všechny varianty (clean + 4 SNR).
3. Sestavit srovnávací tabulku per SNR: publikovaná Table III (PC int8) vs legacy-bug běh vs opravený běh (z M3, seed 42, full-corpus).
4. Zapsat závěr do `generated/reports/provenance_table3.md`:
   - Větev (a): legacy ~ publikovaná čísla -> bug artefakt potvrzen; do camera-ready jde opravená tabulka + vysvětlení diskrepance; float32 vs int8 srovnání se přehodnotí na opravených číslech.
   - Větev (b): legacy != publikovaná -> hypotéza padá; diskrepance se v článku poctivě reframuje jako "open observation" (dle rozhodnutí v camera-ready plánu z 21. 7. je to přijatelné); zbývající podezřelí (váhy z M1, HW z M6).

**Výstupy:**
- `generated/reports/provenance_table3.md` + srovnávací CSV.

**Akceptační kritéria:**
- [ ] Tři sady čísel (publikovaná / legacy-bug / opravená) vedle sebe per SNR.
- [ ] Jednoznačný závěr větve (a)/(b), formulovaný pro přímé použití v Section V/VI článku a v odpovědi recenzentům.

**Odhad:** 2–3 h.

---

## M5 — Deliverables (harness, reviewer response, LaTeX, HTML)

**Stav:** NEZAHÁJENO

**Cíl:** Zabalit výsledky M1–M4 do čtyř dohodnutých výstupů; celá PC reprodukce spustitelná jedním příkazem.

**Metoda:** Nový `ml/make_deliverables.py` (čte agregované CSV z M3/M4, generuje texty) + dva nové Taskfile tasky. Žádná ruční čísla — vše generované ze zdrojových CSV, aby regenerace po případné změně byla triviální.

**Kroky:**

1. `Taskfile.yml` — nové tasky:
   - `reproduce:pc` — end-to-end řetěz: `ml/reproduce.py` -> `ml/reproduce_legacy.py` -> `ml/make_deliverables.py`.
   - `reproduce:clean` — smaže `generated/features_seed*`, `generated/results/*`, `generated/reports/*`; **nikdy** nesahá na `generated/results_ref_20260721/`.
2. `ml/make_deliverables.py` generuje:
   - `generated/reports/table2_float32.tex` a `table3_int8.tex` — IEEE booktabs formát, sloupce full-corpus i held-out, hodnoty mean ± std přes seedy; připravené na `\input` do `BEC/article/article_main.tex`.
   - `generated/reports/reviewer_response.md` — struktura: výtka -> důkaz -> navrhovaná formulace do článku. Pokrývá: R2#6/R4-major1 (M1 + M4), R4-major2 (M3 + M4), R3/R4-major3 (held-out z M3 + caveat 13 eventů), R4-Q4 (SNR vzorec z `prepare_features.py:31-36`), R1 III.C (velikost modelu 81 008 B; Flash/RAM/latence označit jako závislé na M6).
   - JSON/CSV podklad pro HTML artifact.
3. HTML artifact (nástroj Artifact): vizuální přehled pro spoluautory a školitele — publikovaná vs reprodukovaná čísla, full vs held-out, mean ± std, srovnání legacy-bug vs opravený běh.
4. Aktualizovat `REPRODUCTION_NOTES.md`: odkaz na tuto roadmapu, nový harness a umístění výsledků.

**Výstupy:**
- Funkční `task reproduce:pc` a `task reproduce:clean`.
- `generated/reports/reviewer_response.md`.
- `generated/reports/table2_float32.tex`, `table3_int8.tex`.
- URL HTML artifactu.

**Akceptační kritéria:**
- [ ] `task reproduce:clean && task reproduce:pc` doběhne od nuly bez ručních zásahů.
- [ ] .tex fragmenty se zkompilují v kontextu článku (zkušební `\input` do article_main.tex).
- [ ] reviewer_response.md pokrývá všechny 4 hlavní výtky + R4-Q4.
- [ ] Artifact vygenerován a sdílen.

**Odhad:** 3–4 h.

---

## M6 — ESP32-S3 hardware (volitelné; vyžaduje desku)

**Stav:** NEZAHÁJENO (blokováno dostupností hardwaru)

**Cíl:** Reálná embedded čísla pro Tabulku III a definitivní uzavření PC vs ESP diskrepance per-sample důkazem; Release-build latence a Flash/RAM čísla pro R1 III.C.

**Známé blokery a fakta:**
- `.devcontainer/fw-devcontainer/` referencovaný v REPRODUCTION_NOTES neexistuje v repu.
- Nekonzistentní ESP-IDF verze: `firmware/esp32s3/main/idf_component.yml` deklaruje `>=4.1.0`, `dependencies.lock` je resolvnutý proti 5.2.1, poznámky zmiňují Docker s v5.4.
- V tomto prostředí nalezeno `/opt/esp/python_env/idf5.4_py3.12_env` — build lze možná ověřit i bez fyzické desky (flash a UART validace desku vyžadují).
- Firmware je aktuálně Debug build (`CONFIG_COMPILER_OPTIMIZATION_DEBUG=y`) — publikovaná latence cca 32 ms je měřena v Debug režimu a jen kolem `Invoke()`.
- Firmware `model_data.h` je bit-identický s `archive/models/model_data.h` — na desce běží kanonický archivní model, žádná výměna není potřeba.

**Kroky (checklist):**

1. Zprovoznit ESP-IDF toolchain (preferovaně v5.2.1 dle lock souboru; případně ověřit funkčnost `/opt/esp` prostředí s v5.4 a výsledek zapsat do logu).
2. `task firmware:build`; při úspěchu `task firmware:flash ESPPORT=<port>`.
3. Upravit `ml/validate_esp_uart.py`: threshold `>` -> `>=` (ř. 69, sjednocení s PC evalem), logovat raw skóre per sample do CSV.
4. Přehrát identické featury (seed 42, stejné .npy jako v M3) přes UART; per-sample porovnat |PC-int8 skóre − ESP-int8 skóre|:
   - Cíl: rozdíl <= 1 LSB u všech vzorků -> diskrepance uzavřena, do článku jde jedna sloučená int8 tabulka.
   - Jinak: bisekce — vstupní kvantizace, verze TFLM operátorů, zaokrouhlování (numpy banker's rounding vs C round).
5. Release build (`CONFIG_COMPILER_OPTIMIZATION_PERF`): přeměřit latenci; `idf.py size` -> Flash/RAM čísla; ověřit nativní podporu všech ops v TFLM na ESP32-S3 (R1 III.C).

**Akceptační kritéria:**
- [ ] Per-sample shoda <= 1 LSB, nebo zdokumentovaná a vysvětlená příčina rozdílu.
- [ ] Release latence + Flash/RAM čísla připravená pro článek.

**Odhad:** 4–8 h.

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
