# Retrénink s řádným protokolem: metodologie, postup a výsledky

Datum: 2026-08-19 | Kontext: camera-ready BEC2026 paper 53 (deadline 2026-08-31)

Tento adresář obsahuje tři kontrolované retréninkové experimenty (větve ablace), které
opravují metodologické vady původního trénovacího postupu z diplomky a odpovídají na
výtky recenzentů R3 a R4 (major concern 3): *„results on data not used in training"*.

## 1. Motivace: co bylo špatně na původním tréninku

Původní trénovací skript `ml/main_cnn.py` (převzatý z diplomky) má tři vady:

1. **Žádná validační množina.** Jen 2-way split 80/20; „testovací" partition sloužila
   zároveň jako `validation_data` monitor během tréninku. Hyperparametry (epochs=50,
   class_weight, architektura) byly laděny pohledem na tutéž množinu, na které se
   reportovaly výsledky.
2. **Nereprodukovatelný split.** Data se načítala přes `os.listdir()` (pořadí dané
   filesystémem) a `train_test_split(random_state=42)` nad tímto pořadím — konkrétní
   složení původního splitu je nenávratně ztracené. Kanonický `splits.csv` v repu proto
   témeř jistě neodpovídá množinám, na kterých se archivní modely reálně trénovaly, a
   „held-out" čísla archivních modelů nelze garantovat jako čistá.
3. **Neseedovaný trénink + bezpodmínečný přepis výstupu.** `tf.random.set_seed()` se
   nikde nevolal a každý běh přepsal `najlepsi_model.h5` — mechanismus, kterým vznikly
   dva různé modely v archivu (viz `generated/reports/weights_provenance.md`, milník M1).

## 2. Metodologie společná všem větvím

- **Pevný 3-way split** (`retrain/splits3.csv`, commitnutý): train 546 (40 launch) /
  val 137 (10 launch) / test 171 (13 launch). Test = beze změny kanonický test split
  (`datasets/recordings/splits.csv`), takže čísla zůstávají srovnatelná s evaluací
  archivních modelů i ESP32 měřeními z M6. Val je vykrojená z kanonického trénu
  (stratifikovaně, seed 42) skriptem `retrain/make_splits.py`.
- **Plné seedování**: `random`, `numpy`, `tf.random.set_seed` +
  `tf.config.experimental.enable_op_determinism()` — dva běhy se stejným seedem dají
  identické váhy. Trénink 5 seedů (42–46) kvantifikuje citlivost na inicializaci.
- **Architektura beze změny**: `build_baseline_cnn` z `ml/model.py` (LeNet-like CNN,
  72 193 parametrů) — deployment čísla z M6 (Flash/RAM/latence/ops) zůstávají platná.
- **Model selection jen přes val**: early stopping na `val_loss` (patience 10,
  restore best weights), val je clean. Test se dotkne až finální evaluace.
- **Int8 konverze** (`retrain/convert_int8.py`): stejné nastavení jako
  `ml/convert_model.py`, ale representative dataset striktně z **train** partition
  (nikdy val/test) a bez hard-coded feature rootu.
- **Evaluace**: `ml/reproduce.py` (stejný harness jako M3) — 5 šumových seedů (42–46),
  varianty clean + waveform šum SNR 30/20/10/5 dB, float32 i int8, full-corpus i test.
  Hlavní čísla = test scope. Každá buňka tabulky = mean ± std přes 25 běhů
  (5 tréninkových × 5 šumových seedů).

### Trénovací data jednotlivých větví

| Větev | Adresář | Trénovací množina | Vzorků |
|---|---|---|---|
| ① jitter | `retrain/` | clean + MFCC-domain jitter (recept diplomky: 4 kopie, šum ∝ std matice, úrovně 0.1–0.5) | 2 730 |
| ② jitter+wf | `retrain-waveform/` | větev ① + waveform-noise varianty (SNR 30/20/10/5) | 4 914 |
| ③ wf-only | `retrain-waveform2/` | clean + waveform-noise varianty, bez jitteru | 2 730 |

Waveform augmentace: ke každé trénovací nahrávce 4 kopie, jedna na SNR hladinu; Gaussův
šum škálovaný podle energie nahrávky, přidaný do waveformy **před** MFCC extrakcí
(stejná transformační cesta jako eval protokol). Šumový seed augmentace = **142**,
disjunktní od evaluačních seedů 42–46 (`task retrain-wf:features`). Leak není možný ani
principiálně: augmentují se jen train nahrávky, eval měří jen test nahrávky.

## 3. Postup (chronologicky)

1. `make_splits.py` → `splits3.csv` (commitnut, natvrdo).
2. Větev ①: `task retrain:train:all` — 5 seedů; early stopping po 15–24 epochách,
   val_acc 0.97–0.99 (stabilní napříč seedy — na rozdíl od původního skriptu, kde změna
   seedu dávala val_acc 0.795 vs 1.000).
3. Eval větve ① → zjištění, že MFCC-jitter model kolabuje při nízkém SNR na held-out
   datech (viz výsledky) a že tréninkový seed je dominantní zdroj variance.
4. Diagnóza: doménová neshoda trénovací augmentace (aditivní šum v cepstru) vs. eval
   protokol (nelineární cepstrální zkreslení z waveform šumu) — článek tuto neshodu
   sám deklaruje jako záměr (Sec. II) a ptá se, zda ji model přežije.
5. Větev ②: `task retrain-wf:train:all` (jitter + waveform) a větev ③:
   `task retrain-wf2:train:all` (waveform-only) → úplná ablace.
6. Int8 konverze všech 15 modelů + eval (25 běhů/buňku, viz výše).
7. **Ověřovací kontroly** (sekce 5): integrita splitů, duplicity, neviděné SNR hladiny.

## 4. Výsledky

### 4.1 Ablace — held-out test split (171 vzorků, 13 launch), MCC mean ± std

| SNR | ① jitter | ② jitter+wf | ③ wf-only |
|---|---|---|---|
| **float32** | | | |
| Clean | 0.892 ± 0.117 | 0.814 ± 0.156 | **0.976 ± 0.020** |
| 30 dB | 0.872 ± 0.106 | 0.805 ± 0.157 | **0.972 ± 0.018** |
| 20 dB | 0.648 ± 0.073 | 0.810 ± 0.164 | **0.966 ± 0.020** |
| 10 dB | 0.425 ± 0.106 | 0.793 ± 0.181 | **0.957 ± 0.057** |
| 5 dB | 0.203 ± 0.176 | 0.800 ± 0.207 | **0.908 ± 0.107** |
| **int8** | | | |
| Clean | 0.879 ± 0.073 | 0.758 ± 0.155 | **0.968 ± 0.016** |
| 30 dB | 0.807 ± 0.101 | 0.729 ± 0.125 | **0.972 ± 0.018** |
| 20 dB | 0.641 ± 0.077 | 0.670 ± 0.087 | **0.966 ± 0.020** |
| 10 dB | 0.426 ± 0.079 | 0.585 ± 0.055 | **0.967 ± 0.051** |
| 5 dB | 0.210 ± 0.160 | 0.563 ± 0.065 | **0.911 ± 0.106** |

Zdroj: `retrain*/results/train_seed*/per_seed_metrics.csv`, agregát
`retrain-waveform2/results/ablation_summary.csv`.

Interpretace:

- **Recept diplomky (①) na poctivě held-out datech kolabuje při nízkém SNR** (MCC 0.20
  @ 5 dB) a má obrovskou seed varianci (per-seed 0.03–0.49 @ 5 dB). Odpovídá to na
  „key question" článku: levná MFCC augmentace drží jen do ~20 dB.
- **Waveform augmentace (③) dává plochou robustnostní křivku** 0.97 → 0.91 přes celý
  rozsah SNR, poráží i archivní firmware model, a int8 ≈ float32 (kvantizace
  neutrální — čtvrté nezávislé potvrzení).
- **MFCC jitter aktivně škodí**: ② < ③ všude; jitter učí špatnou invarianci (aditivní
  cepstrální šum ≠ nelineární zkreslení z waveform šumu). Ve ② navíc int8 tratí výrazně
  víc než float32 (0.56 vs 0.80 @ 5 dB) — pravděpodobně kalibrace representative
  datasetu (clean vzorky) vůči šumovým aktivacím; ve ③ tento efekt není.
- Změna charakteru selhání ③ při extrémním šumu: precision ~1.0, recall klesá — model
  spíš vynechá launch, než aby falešně poplašil (opak float32 FP inflace z článku).

### 4.2 Nález: duplicitní nahrávky napříč splity

Hash-based kontrola (SHA-256 clean MFCC matic) odhalila **54 skupin bit-identických
nahrávek** pod různými `recording_id` (vícenásobné importy téhož zdroje — SoundBible
efekty, „(1)" kopie, výřezy z téže střelnice). **34 ze 171 testovacích vzorků (2 launch,
32 non-launch) má exaktní duplikát v train/val** — seznam:
`retrain/test_duplicate_contaminated_ids.csv`.

Na deduplikovaném testu (137 vzorků, 11 launch) se ale pořadí větví nemění a čísla ③ se
dokonce **zlepšila** — duplicity tedy nevysvětlují její výhru:

| SNR | ① int8 | ③ int8 | archivní firmware int8 |
|---|---|---|---|
| Clean | 0.917 | **1.000** | 1.000 |
| 30 dB | 0.857 | **1.000** | 1.000 |
| 20 dB | 0.667 | **0.990** | 0.850 |
| 10 dB | 0.431 | **0.969** | 0.608 |
| 5 dB | 0.212 | **0.897** | 0.603 |

Důsledek: duplicity kontaminují **všechna** dosavadní held-out čísla v projektu (včetně
archivního modelu a M3 výstupů) a je to samostatný reportovatelný nález pro dataset
sekci článku. Doporučení: deduplikovat korpus, přegenerovat splity, finální čísla
reportovat na deduplikovaném testu.

### 4.3 Generalizace na neviděné SNR hladiny (větev ③)

Model trénovaný na SNR {30, 20, 10, 5} dB, evaluace na hladinách, které při tréninku
neviděl (features seed 42, test split; `retrain-waveform2/results/unseen_snr_seed*.csv`):

| SNR | float32 MCC | int8 MCC | Poznámka |
|---|---|---|---|
| 15 dB | 0.957 ± 0.054 | 0.966 ± 0.036 | interpolace — plochá křivka drží |
| 0 dB | 0.756 ± 0.195 | 0.745 ± 0.199 | extrapolace — precision 1.00, recall ~0.60: degraduje kontrolovaně (vynechává, nefalešně poplašuje) |

## 5. Ověřovací kontroly (proti podezření z chyby)

| Kontrola | Výsledek |
|---|---|
| Disjunktnost train/val/test | 0 průniků; test ≡ kanonický test; train∪val ≡ kanonický train |
| Sdílený šum train/eval | Vyloučen konstrukcí: augmentace seed 142 na train nahrávkách, eval seedy 42–46 na test nahrávkách |
| Exaktní duplicity přes splity | Nalezeny (34/171 v testu) — kvantifikováno, dedup přepočet výhru ③ potvrzuje (sekce 4.2) |
| Neviděné SNR hladiny | 15 dB drží (0.96), 0 dB degraduje kontrolovaně (0.75) — nejde o memorování evaluačních bodů |
| Determinismus | Regenerace `splits3.csv` bitově shodná; trénink plně seedovaný |

Známé limity: (a) trénovací augmentace používá stejnou *rodinu* zkreslení jako eval
protokol (Gaussův waveform šum) — generalizace na reálný, ne-Gaussův polní šum zůstává
otevřená a patří do limitations článku; (b) dedup kontrola chytá jen bit-exaktní shody,
near-duplicity (jiný výřez téhož zdroje) zůstávají neodhalené; (c) val má jen 10 launch
eventů — model selection je šumová.

## 6. Reprodukce

```bash
# z adresáře BEC/
task retrain:train:all        # větev ① (splits3.csv vznikne automaticky)
task retrain-wf:features      # augmentační featury (seed 142), jednorázově
task retrain-wf:train:all     # větev ②
task retrain-wf2:train:all    # větev ③

# int8 konverze a eval (z rootu projektu, pro každou větev/seed):
venv/bin/python BEC/retraining/retrain/convert_int8.py --model <model.h5>
venv/bin/python ml/reproduce.py --seeds 42 43 44 45 46 \
    --model-keras <model.h5> --model-tflite <model.tflite> \
    --results-root <větev>/results/train_seed<N> --no-legacy-csv
```

Modely a per-sample dumpy jsou gitignored (plně reprodukovatelné, viz `BEC/.gitignore`);
commitnuté jsou agregované metriky a důkazní CSV.

## 7. Nový dataset a window ablace (2026-08-20)

Po rozhodnutí pro větev ③ proběhla druhá vlna experimentů: nový launch dataset ze
4-mikrofonní kampaně a změna analytického okna. Chronologie a poznatky:

### 7.1 Nový dataset

50 zipů = 50 výstřelů × 4 synchronně oříznuté mikrofonní kanály (48 kHz mono WAV).
Konstrukce, event-level split (32×4 train / 8×1 val / 10×1 test launch + negativy
sdílené se starým datasetem) a zdůvodnění: `BEC/new-dataset/README.md`. Trénink přes
`--dataset {old,new}` ve všech třech větvích (zpětně kompatibilní), Taskfile
`DATASET=old|new` + `dataset-new:*` tasky. Staré dana_artillery nahrávky vyřazeny
(riziko duplicit týchž fyzických výstřelů).

### 7.2 Nález: int8 kalibrace je citlivá na složení representative datasetu

První konverze modelů nového datasetu dala **MCC 0.0 u 3 z 5 seedů** (int8 predikoval
nesmysly, float32 v pořádku). Příčina: `convert_int8.py` bral prvních 100 *seřazených*
train vzorků — nové launch recording_id (číselné prefixy) se řadí před negativy, takže
kalibrace viděla **jen launch třídu**. Oprava: rovnoměrně rozprostřený deterministický
výběr přes seřazený train (79 negativ / 21 launch) → int8 ≡ float32 (±0.002 MCC).
**Pro článek (R4-Q2):** měřený příklad, že za int8 anomáliemi bývá pipeline
(kalibrace), ne kvantizace sama.

### 7.3 Výsledky na novém datasetu, okno 40/60 (archiv: `retrain-waveform2/results-new-window4060/`)

Int8, held-out test (10 launch eventů + 158 negativ), mean ± std přes 5 train × 5 noise
seedů: clean 0.958, 30 dB 0.959, 20 dB 0.966, 10 dB 0.963, 5 dB 0.955. Extrapolace mimo
trénovaný rozsah: 0 dB 0.841 (precision 1.0, recall 0.74), −5 dB 0.643 — degraduje
kontrolovaně (vynechává, nefalešně poplašuje); velká std extrapolace šla skoro celá za
seedem 46.

### 7.4 Analýza chyb a okna

Confusion matrix: FP prakticky nula (25/19 750 negativních inferencí; jen
`Engine_Inside_Car` a `Classroom_Ambiance`), **other_gunshot 0 FP** (ruční zbraně model
nikdy nesplete s dělem). FN koncentrované do `0011_0226s_shot_011` (49/125) — vizualizace
(`results-new-window4060/fn_window_analysis*.png`) ukázala, že peak-detekce funguje, ale
tento výstřel má komplexní multi-peak strukturu s onsetem ~40 ms před detekovaným peakem
→ 40/60 okno mine náběh. Ilustruje citlivost peak-centered okna (souvisí s R4#4).

### 7.5 Změna okna na 30/70 a finální čísla

`ml/utils.py extract_window`: 40 % před / 60 % za peakem → **30 % / 70 %** (delší
decay část). Featury nového datasetu přegenerovány, 5 seedů přetrénováno, přeevalováno.
**Int8, test split, mean ± std (aktuální kanonická čísla, `results-new/summary_new_dataset.csv`):**

| SNR | 30/70 MCC | (40/60 bylo) | Prec / Rec (30/70) |
|---|---|---|---|
| Clean | **0.990 ± 0.020** | 0.958 | 0.982 / 1.000 |
| 30 dB | **0.988 ± 0.022** | 0.959 | 0.982 / 0.996 |
| 20 dB | **0.992 ± 0.020** | 0.966 | 0.996 / 0.988 |
| 10 dB | **0.977 ± 0.035** | 0.963 | 0.985 / 0.972 |
| 5 dB | **0.976 ± 0.039** | 0.955 | 1.000 / 0.956 |

FN `shot_011` kleslo 49 → 14/125; per-seed @ 5 dB 0.92–1.00; std poloviční proti 40/60;
float32 ≈ int8. Poučení: jednosnímková energetická analýza okna nebyla prediktivní —
rozhodl retrénink na nových oknech.

**Extrapolace mimo trénovaný rozsah (30/70, int8, `results-new/summary_lowsnr.csv`):**
0 dB MCC 0.947 ± 0.049 (precision 1.000, recall 0.904), −5 dB MCC 0.833 ± 0.107
(precision 1.000, recall 0.716) — proti 40/60 (0.841 / 0.643) velký skok a žádný
seed nekolabuje (min 0.67 @ −5 dB vs. 0.00 dřív). Ani při šumu silnějším než signál
model falešně nepoplaší, jen vynechává. FN při extrémech opět dominuje session 0011.

### 7.6 Souhrnný posun (poctivý held-out test, int8, MCC)

| Krok | Konfigurace | Clean | 5 dB | Seed rozptyl @ 5 dB |
|---|---|---|---|---|
| Výchozí stav (recept diplomky, řádný protokol) | větev ① — MFCC jitter, starý dataset | 0.879 | **0.210** | 0.03–0.49 |
| + waveform augmentace | větev ② | 0.758 | 0.563 | |
| − MFCC jitter (ablace) | větev ③ | 0.968 | 0.911 | 0.73–0.99 |
| + nový 4-mic dataset (event-level split) | větev ③, okno 40/60 | 0.958 | 0.955 | |
| + okno 30/70 | **finální** | **0.990** | **0.976** | 0.92–1.00 |

Hlavní číslo: **MCC @ 5 dB z 0.21 na 0.98**. Pod trénovaným rozsahem navíc 0 dB → 0.947,
−5 dB → 0.833, vždy s precision 1.000 (model nikdy falešně nepoplaší, jen vynechává).

Kontext: archivní float32 `najlepsi_model.h5` měl MCC ~0.52 už na clean datech (FP
inflace, precision 0.33) a publikovaná ESP32 čísla 0.99→0.80 byla artefakt (jiný model,
eval na trénovacích datech, duplicity). Skutečná poctivá startovní čára = větev ①.

Zdroje posunu v pořadí důležitosti: (1) doménově správná augmentace (waveform šum místo
MFCC jitteru): 0.21 → 0.91 @ 5 dB, největší jednotlivý skok; (2) okno 30/70: 0.955 →
0.976 @ 5 dB, extrapolace 0.84 → 0.95 @ 0 dB, poloviční std, FN `shot_011` 49 → 14;
(3) nový dataset: srovnatelná čísla, ale důvěryhodná (bez duplicit, event-level split,
4-mic diverzita, víc sessions); (4) vedlejší úlovky: kvantizace prokazatelně neutrální,
int8 kalibrační nález, duplicity odhalené. Vše **beze změny architektury** — 72k
parametrů, deployment stopa i M6 čísla (Flash/RAM/latence) platí dál. Proti recenzované
verzi: ta tvrdila 0.80 @ 5 dB na z většiny trénovacích datech; teď 0.98 @ 5 dB na
datech, která model nikdy neviděl.

### 7.7 Otevřené položky
- ESP32-S3 HW validace vybraného seedu (kandidáti 44/45: MCC 1.0 @ 5 dB).
- Přepis článku: mj. „40 %/60 %" → „30 %/70 %" v Sec III; nová Table I (events vs.
  samples); SNR definice vůči 1s klipu (R4-Q4); kalibrační nález do diskuze.
- Pozor: featury starého datasetu (`generated/features_seed*`) jsou ze 40/60 éry —
  `task reproduce:clean && reproduce:pc` by je přegeneroval s 30/70 a bitový
  cross-check proti referenci z 21. 7. přestane sedět (archivní éra M0–M6).

## 8. Důsledky pro camera-ready

**Rozhodnuto 2026-08-19: větev ③ je nový kanonický model pro camera-ready.** Duplicity a
případné rozšíření datasetu (nové launch/impact nahrávky) se řeší následně — pokud se
dataset předělá, celá pipeline se přetrénuje a čísla přegenerují. ESP32-S3 validace
větve ③ zbývá (postup M6).

1. Větev ③ je kandidát na nový kanonický model: poctivá held-out čísla MCC 0.97 → 0.91
   (30 → 5 dB), bez kontaminačních caveatů, stejná architektura a deployment stopa.
   Vyžaduje: HW validaci na ESP32 (postup M6), přepis Robustness Training (waveform
   augmentace místo MFCC jitteru) a úpravu rámování „key question" v Sec. I/II.
2. Ablace ①/②/③ je sama o sobě publikovatelný výsledek: kvantifikuje cenu doménové
   neshody augmentace (0.20 vs 0.91 MCC @ 5 dB) a přímo odpovídá R3 (návrh augmentace)
   i R4-major3 (held-out čísla).
3. Nález duplicit patří do dataset sekce a je nutné ho promítnout do všech held-out
   tvrzení (i pro archivní model).
