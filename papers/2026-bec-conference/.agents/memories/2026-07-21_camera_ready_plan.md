# Camera-ready plán (BEC2026, paper 53) — deadline 2026-08-31

Kapacita: ~1 den/týden (4–8 h), 6 týdnů → rozpočet ~24–40 h. Priorita jiného projektu — tento plán počítá s minimem a bufferem.

## Klíčová zjištění z repa (2026-07-21)

Tři technické výtky recenzentů mají už teď kandidátní vysvětlení v kódu:

1. **PC-int8 vs ESP32-int8 rozdíl (R2#6, R3, R4-major1)** — dva hlavní podezřelí:
   - `ml/eval_tflite_pc.py:66-68` — legacy PC-int8 skript má **int8 overflow v dequantizaci** (`output - zero_point` bez rozšíření typu; numpy 2.x wrapuje). Opravená verze je v `ml/evaluate_pc.py:49-53`. ESP32 (`main.cpp:160-164`) počítá správně. Pokud Table III PC-int8 čísla pocházejí z legacy skriptu, rozdíl je artefakt bugu.
   - **Různé váhy**: firmware má zabudovaný archivní model (`model_data.h`, 81008 B), pipeline `convert_model.py` generuje jiný (~81400 B). PC eval a ESP32 tedy mohly běžet **různé modely**.
   - Menší: threshold `>` (validate_esp_uart.py:69) vs `>=` (evaluate_pc.py:68); numpy banker's rounding vs C round().
2. **Eval na trénovacích datech (R3, R4-major2/3)** — potvrzeno: `evaluate_robustness.py` běží na celém korpusu. Split ale existuje (80/20 stratified, seed 42, `datasets/recordings/splits.csv`) → held-out eval = jen filtrování. Pozor: test má jen **13 launch eventů** → široké intervaly, nutný caveat.
3. **Nesoulad počtu eventů**: článek tvrdí 706 (62/644), manifest má **854 (63 launch / 791 non-launch)**; 706 = jen `impulse_noise` label. Nutno vyřešit provenance čísel před čímkoli dalším (`REPRODUCTION_NOTES.md:10-24`).

Další fakta: model 72 193 params, full-integer int8 (repr. dataset = 100 train MFCC), tensor arena 80 KiB, latence měřená jen pro Invoke() (bez MFCC/UART), firmware build je **Debug, ne Release** (ovlivňuje ~32 ms číslo), power measurement neexistuje. Metadata distance/caliber/angle v manifestu prázdná — do článku lze doplnit jen to, co víme odjinud (Dana 152 mm, měřicí kampaň).

## Rozhodnutí předem (potvrdit s spoluautory/školitelem)

- **Neretrénovat model.** Kanonický = archivní int8 model z firmware. Jen sjednotit evaluaci. Retrénink = všechna čísla nová = přepisování celého Results.
- **Dataset publikovat?** (R2#5, R4-Q5) — pravděpodobně ne (vojenská měření); do článku napsat "available on request" nebo důvod omezení.
- **Power measurement** — nedělat, přiznat jako future work (R4 minor).
- **Titul**: zúžit na single-stage, např. "A Lightweight Post-Trigger Acoustic Classifier for Embedded Artillery Launch Detection" (R1, R2, R4).

## Harmonogram (1 pracovní den/týden)

### Den 1 — týden 28. 7.: forenzní audit čísel + jednotný eval harness (4–6 h)
- Zjistit, kterým skriptem vznikla Table I/III čísla (git log, archive/, poznámky) → vyřešit 706 vs 854.
- Jeden eval skript: stejný .tflite (archivní z firmware, přes `convert_to_tflite.py`), stejné manifest features, stejný threshold (`>=`), opravená dequantizace, výstup per-sample pravděpodobností do CSV.
- Přegenerovat PC float32 + PC int8 výsledky, celý korpus i test-only split.
- Výstup: tabulka nových čísel + odpověď, zda float32→int8 "zlepšení" přežije opravu bugu.

### Den 2 — týden 4. 8.: ESP32 re-run (4–8 h, potřeba hardware)
- Upravit `validate_esp_uart.py`: threshold `>=`, logovat raw skóre per sample.
- Přehrát identické vstupy (stejné .npy) přes UART, porovnat per-sample: PC-int8 vs ESP32-int8 skóre. Očekávání: shoda ±1 LSB → diskrepance vyřešena.
- Přeměřit latenci s Release buildem (`CONFIG_COMPILER_OPTIMIZATION_PERF`); vytáhnout Flash/RAM z `idf.py size`; ověřit nativní podporu ops v TFLM (R1 III.C).
- Pokud rozdíl přetrvá → bisekce (vstupní kvantizace, operator versions) — proto buffer.

### Den 3 — týden 11. 8.: analýza + přepis Results (4–6 h)
- Nová Table III (3 režimy, sjednocené), nová held-out tabulka/sloupec, definice SNR do Experimental Setup (vzorec z `prepare_features.py:31-36`).
- Rozhodnout narativ kvantizace: vysvětleno jako artefakt / reframe jako open observation (R4-major2).
- Vysvětlit FP-inflaci float32 (R1 Sec V) na nových číslech.
- Doplnit model size B / Flash / RAM / ops support.

### Den 4 — týden 18. 8.: přepis textu 1 (4–8 h)
- Titul + abstract (explicitně: validace jen post-trigger klasifikátoru, held-out caveat).
- Sec II zkrátit, přidat kvantitativní výsledky citovaných prací, zmínit Elkarous 2025, zvýraznit contribution gap (R2#1); snížit počet referencí.
- Sec III: trigger-stage odkaz/výkon (R1 III.A), dataset detaily (kalibr, prostředí, vzdálenosti — co je známo), imbalance diskuse, proč CNN vs one-class metody (R3), proč ESP32-S3, proč 22.05 kHz, host/MCU partitioning (MFCC na hostu!).
- Limitations přesunout dopředu, connect s abstract caveaty.

### Den 5 — týden 25. 8.: přepis textu 2 + finalizace (4–6 h)
- Akronymy při prvním použití, reference na int8/float32/kvantizační techniky (R2#2,3).
- Figs 1&2: odstranit prázdné jednotky, zvážit sloučení; Table I separator.
- Grammar pass (celý text), zkontrolovat 4–6 stran.
- Poslat spoluautorům na review.

### Týden 31. 8. — buffer + odevzdání (2–4 h)
- Zapracovat komentáře, IEEE PDF eXpress validace, copyright form, submit.
- **Stejný den early-bird registrace deadline — nenechat na poslední chvíli.**

## Rizika
- Nová čísla se výrazně liší od publikovaných → víc přepisování (buffer den 3+6).
- ESP32 diskrepance nezmizí po sjednocení → reframe jako observation, popsat honest (přijatelné pro camera-ready).
- Hardware nedostupný v týdnu 2 → prohodit den 2 a 3.
