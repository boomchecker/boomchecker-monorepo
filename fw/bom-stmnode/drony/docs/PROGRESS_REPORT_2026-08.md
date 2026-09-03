# Progress report — detekce dronů na STM32H563 (sessiony 6.–7. 8. 2026)

Autoři: Kamil Herman + Claude (AI asistent). Navazuje na PROGRESS_REPORT.md (červen 2026)
a na práci Martina Maxy (PDM mikrofon → USB streaming, PR #70 na main).

## 1. Shrnutí

Propojili jsme obě domény projektu: **detekční algoritmus (MFCC + lineární SVM) poprvé
běžel přímo na desce Nucleo-H563 nad živými daty z PDM mikrofonu** (příkaz `detect`,
overrun=0 — pipeline se vešla do real-time rozpočtu vedle PDM→PCM DSP). Zároveň jsme
na reálných nahrávkách kvantifikovali hlavní slabinu modelu — **citlivost na absolutní
hlasitost** — a sérií retrénů ověřili, že jde o strop kapacity lineárního SVM, ne o chybu
dat. Flashování funguje **bez ST-Linku** (ROM DFU bootloader + nový příkaz `dfu`).
Session skončila s jednou otevřenou chybou (viz §6 a §7).

## 2. Ověření hardwaru a řetězce (7. 8. dopoledne)

- Jádro běží skutečně na **250 MHz**: 10s nahrávka trvala 10,1 s, `overrun=0` (na 83 MHz
  by trvala ~30 s). SB3/SB4 správně.
- USB CDC + PCM1 protokol + mikrofon: čistý signál, správné počty bloků, `streamtest` OK.
- Úrovně: ambient RMS ≈ 0,005 ≈ trénovací škála (gain +24 dB v pdm_pcm sedí).
- **ST-Link je hardwarově mrtvý** (COM LED bliká = procesor žije; PC nevidí ani pokus
  o enumeraci; 2 kabely, jiné porty, otočení USB-C, power-cycle — bez efektu).
  Nevadí — flash jde přes DFU (viz §5). *(Dodatek 10. 8.: neplatí — doma nefungoval,
  v práci po výměně kabelu funguje normálně, viz §9.)*
- Nahrána validační sada přes `stream`: 5× pozadí (ticho, ambient, řeč, tleskání, hudba
  z repra) + 3× přehrávaný bebop v různých hlasitostech → `data/recordings/`.

## 3. Klíčový nález: SVM je citlivé na hlasitost

- Zisk g posouvá **jediný příznak** — mean(c0) — o √20·ln(g); ostatních 25 příznaků je
  na zisk invariantních. Ověřeno na datech: predikce posunu decision +0,74 na +12 dB,
  změřeno +0,73.
- w[0] = +1,71 je největší váha modelu v1 ⇒ **tichý/vzdálený dron skóruje jako šum**.
  Tiché přehrávání (RMS 0,004): decision −0,54; totéž s digitálním +18 dB: +0,56
  (29/44 oken DRONE).
- Druhá past: **max hlasitost malých repráků = zkreslení/clipping** → spektrum se rozmaže
  a decision PADÁ (−2,1) — hlasitěji ≠ lépe. Testovat na ~60–80 % hlasitosti, bez 7.1
  virtualizace.
- Firmware squelch 0,010 je pro přehrávané drony moc přísný (reálné úrovně 0,004–0,015);
  pro testy je správný bod squelch 0,003–0,005.

## 4. Retrény a benchmarky na neviděných datech

Skript `src/analysis/train_svm_level_robust.py` (gain augmentace −30…+3 dB, syntetické
téměř-ticho negativy, reálné kotvy z mikrofonu, v3 + train/val split Halmstad/Salford).
Modely: `svm_model_data.h` (v1), `_v2.h`, `_v3.h` — ve firmwaru výměna jedním include.

Validační poloviny neviděných datasetů (squelch 0,005, práh 0,5; soubory OK / % oken):

| skupina | v1 | v2 | v3 |
|---|---|---|---|
| Halmstad DRONE (15) | 7/15, 38 % | 9/15, 48 % | 9/15, 56 % |
| Halmstad BACKGROUND (15) | 14/15, 99 % | 9/15, 82 % | 8/15, 82 % |
| Halmstad HELICOPTER (15) | 11/15, 94 % | 7/15, 80 % | 7/15, 82 % |
| Salford přelety (4) | 0/4, 0 % | 1/4, 1 % | 2/4, 5 % |

Reálné nahrávky: v3 detekuje tichý bebop 32/33 a střední 42/44 oken (i při prahu 0,5),
ale ticho místnosti hlásí 9–10/13 jako dron. **Závěr série: lineární SVM nad mean+std
MFCC narazil na strop — přidávání dat posouvá pracovní bod po křivce citlivost↔falešné
poplachy, ne nad ni.** (v1 = konzervativní, slepý na dálku; v3 = citlivý, hlučný.)
Vrtulníky matou všechny verze. Skutečné vzdálené přelety (Salford) nedetekuje prakticky
žádná verze — to je hlavní mezera.

## 5. Příkaz `detect` ve firmwaru + flashování bez ST-Linku

Integrováno do `fw/bom-stm32node` (necommitnuto, build prochází: RAM 13,4 %, FLASH 6,1 %):

- `Core/Src/detector.c` — konzument `mic_poll()`: decimace `[::3]` s fázovou návazností
  (101-tap FIR v pdm_pcm je anti-alias; krátký FIR z původního návrhu byl ověřeně škodlivý
  a je odstraněn i z `drony/src/firmware/audio_sai_handler.c`), FIFO 16 kHz, rámce
  1024/512, squelch → 14 rámců → mean+std → SVM. Výstup `DET t= dec= DRONE|noise`,
  `LVL` (úroveň 1×/s), `DETEND windows= drones= overrun= err=`.
- `detect <sec> [squelch_milli] [thr_milli] [dbg]` — squelch/práh laditelné za běhu,
  dbg=1 tiskne breadcrumb F=/a=/r= + změřené časy h= (mic_poll vč. PDM konverze)
  a m= (MFCC) v µs přes DWT cycle counter.
- `Core/Src/dfu_boot.c` + příkaz `dfu` — skok do ROM bootloaderu (H563 = **0x0BF97000**;
  0x0BF87000 je H503 a způsobí fault — ověřeno v praxi). **Celý cyklus reflashe bez
  ST-Linku funguje:** `dfu` po sériové lince → `STM32_Programmer_CLI -c port=USB1 -w
  <elf> -v` → fyzický RESET (`-g` po DFU nefunguje spolehlivě — USB aplikace pak
  nenaběhne; vždy RESET).
- První vstup do bootloaderu (bez `dfu` příkazu ve firmwaru): drátek 3V3 (CN8) → otvor
  BOOT0 na neosazeném CN11 footprintu (názvy pinů na spodním potisku) při RESETu.
- Host: `spec.py` + přegenerovaný PROTOCOL.md (`detect`, `dfu`); testy stm32node-cli
  29/30 (1 preexistující Windows-only fail na oddělovačích cest v test_wav).

**Milník: `detect 10` v tichu na desce** — 4 okna správně noise (decision −0,7…−2,3,
shodné s PC pipeline), `overrun=0 err=0`, zbytek korektně pod squelchem.

## 6. Ladění na hardwaru — nálezy ze závěru session

1. **Wedge při nízkém squelchi (build 2):** `detect X 3 250` (MFCC na každém rámci)
   po ~1–4 s přestal vypisovat a **USB zemřelo až do resetu** (port „zařízení nefunguje").
   Pracovní hypotéza: hraniční překročení 21,33ms rozpočtu (17 ms PDM + MFCC + čekání
   USB zápisů) → rostoucí backlog → smyčka přestala čekat na bloky → `usb_cli_pump()`
   se přestal volat → vyhladovění USBX → Windows zařízení odepsal. Opravy připraveny
   v buildu 5: max 1 rámec na blok (průměrný přítok 341 < odtok 512 vzorků ⇒ backlog
   neroste), pump každý rámec, DWT měření pro potvrzení čísel.
2. **`usb_cli_flush_tx()` volaný z CLI bindingu zamrzá** (buildy 3–4 nevypsaly nic;
   build 2 bez něj tiskl). Z buildu 5 odstraněn — DET zápisy si rozjetý staged transfer
   dočistí samy přes `finish_staged_tx()`. Kořenová příčina v USBX write state machine
   zatím nedohledána (kandidát na debug s logic analyzerem / until-fix nepoužívat).
3. **Lekce z testovacího harnessu:** otevření COM portu během re-enumerace po resetu
   vytvoří „zombie" handle a rozbije port až do další enumerace. Správný postup: počkat
   na PnP Status OK + handshake `version` s retry, teprve pak posílat příkazy.

## 7. Kde přesně jsme skončili (stav k 7. 8. večer)

- **Na desce běží build 4** — `stream`/`streamtest`/`version`/`dfu` fungují, ale
  **`detect` na něm zamrzne** (obsahuje flush-bug §6.2). Deska je po posledním testu
  zaseklá — po RESETu normálně naběhne.
- **Build 5 s opravami je zbuildovaný a čeká** v `fw/bom-stm32node/build/Debug/
  bom-stm32node.elf` — NENÍ flashnutý (session ukončena před posledním cyklem).
- Vše je pouze v pracovním stromě (commit zatím žádný — vědomé rozhodnutí).

## 8. Co se nestihlo + doporučené další kroky

1. **Flashnout build 5 a ověřit detect** (10 min): RESET → `dfu` → flash → RESET →
   handshake → `detect 8 3 250 1` → přečíst h=/m= časy (potvrdí/vyvrátí hypotézu §6.1)
   → `detect` s přehrávaným dronem = plné demo detekce na desce.
2. **Dohledat kořen flush-wedge** v usb_cli/USBX (nízká priorita — obejito).
3. **Model v4**: prolomit strop kapacity — příznaky harmonicity/spektrální plochosti
   nebo malý MLP (26→16→1 ≈ 1,8 kB float32, přenositelný do C ručně), K-z-N hlasování
   oken; trénovat s vrtulníky a dálkovými přelety, validovat na odložených polovinách
   (žádný re-tuning na 8 souborech z 7. 8.!).
4. Sběr dat: venkovní pozadí, kvalitní repro na 2–5 m, ideálně reálný dron.
5. Hygiena repa: rozhodnout commit/merge (drony na Kamil_stmfw, integrace v pracovním
   stromě), zálohovat datasety (1,5 GB jen na tomto PC), případně PR podle zvyklostí
   týmu (squash; vynechat *.o a PDF).

## 9. Dodatek 10. 8. 2026 — kořen wedge nalezen (přetečení zásobníku) a opraven

- **ST-Link ožil**: doma nefungoval (ani s více kabely a porty — viz §2), **v práci
  po výměně kabelu začal fungovat** — enumeruje se vč. VCP na COM5, sonda V3J16M8
  funguje. Diagnóza „hardwarově mrtvý" z §2 tedy neplatí; příčinou byla zjevně
  kombinace kabel/prostředí doma. Ponaučení: při příštím selhání vyzkoušet i další
  kabely jinde, než se HW odepíše. SWD flash s `-v -rst` nahrazuje celou DFU
  proceduru — žádný BOOT0 drátek, žádné mačkání RESETu.
- Build 5 flashnut přes SWD. `detect 1 1000 500 1` (squelch 1000 ⇒ **žádné MFCC**)
  přesto po 19 rámcích (~0,6 s) zamrzl jako dřív ⇒ hypotéza §6.1 (překročení
  real-time rozpočtu) **vyvrácena** — wedge nastával i zcela bez DSP zátěže.
- **Forenzní analýza přes SWD na zaseklé desce** (hotplug, bez resetu):
  jádro běželo (CYCCNT se točil), ale ICSR VECTACTIVE=3 ⇒ **HardFault handler**;
  CFSR = 0x00100001 ⇒ **UFSR.STKOF — přetečení hlavního zásobníku** (+ IACCVIOL),
  HFSR.FORCED=1. Mic čítače zamrzlé (32 půlek ≈ 0,68 s), `s_running=1`, USB write
  path volná — vše důsledek HardFaultu (priorita −1 blokuje SysTick/GPDMA/USB IRQ;
  proto zamrzlý `HAL_GetTick()` nechal detect „běžet", Windows hlásil „zařízení
  nefunguje" a mic přestal dodávat bloky).
- Dump zásobníku + rekonstrukce návratových adres: `main → CLI → cmd_detect →
  detector_run → det_print → snprintf → newlib _svfprintf_r/_printf_i` a na vrcholu
  zanořené `USB_DRD_FS_IRQHandler → HAL_PCD_IRQHandler → PCD_EP_ISR_Handler → USBX`.
  Příčina: **`_Min_Stack_Size = 0x400` (1 KB, CubeMX default)** při všem na MSP
  (main + newlib printf + USBX + vnořená přerušení); startup nastavuje
  `MSPLIM = _sstack`, takže překročení = okamžitý STKOF. Nejhlubší stopa končila
  60 B nad limitem. Hloubka je omezená (žádná rekurze).
- **Oprava (build 6): `_Min_Stack_Size = 0x4000` (16 KB)** v STM32H563xx_FLASH.ld
  (RAM 15,7 %). Zpětně vysvětluje VŠECHNY varianty wedge z §6: flush z bindingu
  (§6.2) = hlubší zanoření okamžitě; nízký squelch (§6.1) = MFCC+printf řetěz;
  „USBX starvation" i „flush state machine bug" byly falešné stopy.
- **Ověření buildu 6 na desce** (vše `overrun=0 err=0`, logy v
  `drony/data/detect_logs/`):
  - `detect 1 1000 500 1`: celý průběh F=0…29 + DETEND (dřív umíral u F=18).
  - `detect 8 0 250 1` (squelch 0 = MFCC každý rámec + dbg = maximální zátěž):
    248 rámců, 17 oken, ticho vše noise (−1,9…−3,2). **Časy: h(mic_poll+PDM)
    max 17 102 µs, m(MFCC) max 573 µs ⇒ worst-case 17,7 ms z 21,33 ms (17 %
    rezerva).** MFCC stojí jen 0,57 ms — rozpočet nikdy nebyl problém.
  - **Plné demo: `detect 20 3 250` s přehrávaným bebopem (rms 0,006–0,010):
    44 oken, 40× DRONE (dec +0,3…+1,8), 4× noise na švech loopu.** Ticho
    předtím: 0/17 falešných poplachů. = milník §8.1 splněn.
- Nový nástroj `drony/tools/board_session.py` (venv stm32node-cli): čeká na
  enumeraci (VID:PID 0483:5710), settle proti zombie-handle, `version` handshake
  s retry, `detect` runner s h=/m= statistikou a logy, `dfu-flash` automatizace
  (ponechána pro případ, že by ST-Link zase odešel).
- Zbývá z §8: dohledat nic (bod 2 vyřešen = STKOF), model v4 (bod 3), sběr dat
  (bod 4), hygiena repa (bod 5) + commit opravy stacku a board_session.py.

## 10. Dodatek 10. 8. 2026 odpoledne — v2nm a v4: strop lineárního SVM prolomen

- **v2nm** (v2 recept bez mean-c0; `train_svm_level_robust.py` s flagy VERSION/
  DROP_MEAN_C0/USE_UNSEEN_TRAIN): mean(c0) je jediný level-závislý příznak, jeho
  vyhozením vznikl model **dokonale invariantní na hlasitost** (held-out metriky
  identické nativně i při −15 dB: 0,896/0,909/0,906) — ale lineární hranice bez
  hlasitosti ztrácí cit (přehrávaný bebop „stredni" 0/44; ticho na hraně −0,0).
  Závěr: potvrzen strop kapacity, hlasitost dělala část skutečné práce.
- **v4 = malý MLP na invariantních příznacích** (`train_mlp_v4.py`): StandardScaler
  → 25 příznaků (bez mean-c0) → 32 ReLU → 1 logit (~3,7 kB float32). Trénink na
  v3 receptu (parquet + DroneAudioDataset + gain augmentace + syntetické ticho +
  reálné kotvy + **train-poloviny Halmstad/Salford**), pozitiva oversamplovaná
  na paritu; kandidáti 16/32 neuronů, vybrán 32 dle held-out −15 dB acc; export
  s paritním testem proti sklearn (max rozdíl 1,3e-06).
- **Výsledky:** held-out **0,972 acc nativně I při −15 dB** (nejlepší model vůbec;
  v1 0,935 s propadem recall 0,94→0,82). **Salford reálné vzdálené přelety 4/4
  souborů, 68 % oken** (v1 0/4, v3 2/4 s 5 %) — hlavní mezera v1–v3 zavřená.
  Pozadí s obřími rezervami (ticho logit −6,6; rec −33; tleskání −19). Vrtulníky
  92 % oken čistě. Slabina: tiché *přehrávání* z repráků 2/33 (skutečné tiché
  drony Salfordu ale fungují → nejspíš artefakt reproduktoru na malé hlasitosti).
- **Firmware:** `svm_classifier.c` má nyní `#ifdef MLP_HIDDEN` forward pass
  (aktivní model = jediný include; `mlp_model_data_v4.h` v Core/Inc). POZOR:
  decision je surový logit → **neutrální práh detect je 0, ne 500**
  (`detect 20 3 0`); logity běžně ±10 až ±60. Na desce **build 8 (v4)**;
  zálohy pro okamžité A/B přepnutí bez rebuildů: `build/Debug/
  bom-stm32node_v1_build6.elf`, `_v2nm_build7.elf` (flash `-c port=SWD -w <elf>
  -v -rst`).
- **Živě na desce:** ticho 17/17 noise (logity −3,5…−10, overrun=0); přehrávaný
  bebop `detect 20 3 0` → 18/42 oken DRONE (logity až +9,6) v dávkách podle
  obsahu loopu — v4 je „vybíravější" než v1 (ten dával 40/44, ale přes
  hlasitost). Další operační krok: K-z-N hlasování oken; další modelový krok:
  harmonicita/spektrální plochost jako extra příznaky.
- Host: `validate_real_wav.classify()` nyní přijímá i callable modely (MLP).
  Vše z 10. 8. odpoledne je NECOMMITNUTO — čeká na Kamilův fyzický test v4.

## 11. Dodatek 10. 8. 2026 večer — experimentální session v5

Systematický pokus překonat v4 (`src/analysis/experiment_v5.py`, 6 kandidátů,
všichni level-invariantní; jednotné window-level vyhodnocení = sémantika
firmwaru; kandidáti uloženi v `models/exp_v5/`):

| experiment | myšlenka | klíčový výsledek |
|---|---|---|
| E1 win14 | trénovat na 14rámcových oknech místo celých klipů (odstranění train/deploy mismatche) | nejlepší held-out acc 0,926; vyváženy |
| E2 mix | syntetické vzdálené drony (pozitiva + pozadí při SNR 0–15 dB) | citlivost ↑, specificita ↓ |
| E3 win+mix | obojí | Halmstad DRONE 13/15, 67 % oken |
| **E4 feats** | **E3 + delta-MFCC a max−mean agregace** (spočítatelné ze 14×13 bloku!) | **tichý dron 27/33 (+2,4)** — dosud 2/33! |
| E5a/E5b | hlubší (32,16) / širší (64) síť na E4 datech | E5b: střední 30/44, pozadí nejčistší |

- **Nasazeno: v5 = E4** (`mlp_model_data_v5.h`, 52 příznaků → MLP 51→32→1,
  7 kB; parita exportu 2,1e-06). Firmware: `det_aggregate` rozšířen na layout
  **[mean 13, std 13, dmean 13, cmax 13]** — dmean = průměr |mezirámcových
  delt| (modulace vrtulí), cmax = max−mean (špičatost, level-invariantní);
  starší SVM hlavičky čtou prvních 26 → všechny modely zůstávají přepínatelné
  jedním include. **Build 9 na desce.** Živě: ticho 17/17 noise (−3,7…−5,8);
  bebop 20/43 oken DRONE (srovnatelné s v4 při této hlasitosti — rozdíl v5
  se projeví u tichého zdroje). Zálohy ELF: v1_build6, v2nm_build7, v4_build8.
- **Nález o stabilitě:** refit E5b (`refit_e5b.py`) NEreplikoval sílu původní
  instance na přehrávkách (střední 30/44 → 0/44; unseen sady držely). Doména
  „dron z repráku" je mimo trénovací rozdělení a je seed-nestabilní — proto
  v5b NEvydáno (poznámka v `export_v5.py`). Efekt delta/cmax příznaků na tichý
  dron je naopak podložen dvěma nezávislými fity (27/33 E4, 17/33 E5a).
- **Důsledky pro další práci:** (1) sběr reálných nahrávek do tréninku je teď
  nejvyšší páka — ukotví nestabilní doménu; (2) levný stabilizátor = ensemble
  přes seedy (3× forward ~ stále µs na MCU); (3) K-z-N hlasování zůstává
  jako operační vrstva. Doporučený pracovní bod v5: `detect <sec> 3 0`
  (max. citlivost) až `detect <sec> 3 1000` (rezerva proti FP).

**Kolo 2 (`exp_round2.py`, tamtéž večer):** dvě zbývající kombinace proti
přesným nasazeným vahám v5:

| kandidát | held-out acc | Salford | tichý dron | pozadí |
|---|---|---|---|---|
| v5 (nasazeno) | 0,886 | 4/4, 67 % | **27/33** | dobré |
| E6 = bez mixování | **0,932** | 4/4, 52 % | 14/33 | **nejčistší** (rec −45!) |
| E7 = ensemble 3 seedů | 0,920 | 4/4, **72 %** | 17/33 | dobré |

Závěry: (1) **mixování se obhájilo** — bez něj (E6) spadne tichý dron na
polovinu; platí se za něj specificitou na datasetových negativech, ale reálná
pozadí zůstávají čistá i s ním. (2) **Ensemble stabilizuje přesně podle
očekávání**: nejlepší výsledek na skutečných přeletech (Salford 72 % oken),
ale nestabilní playback doménu zprůměroval dolů (střední 2/44) — kandidát pro
polní nasazení, až budou reálné nahrávky v tréninku. (3) Žádný kandidát v5
nedominuje; E6/E7 členové uloženi v `models/exp_v5/`. Menu pracovních bodů:
v5 = citlivý (demo s repráky), E6 = konzervativní, E7 = polní/robustní.

## 12. Dodatek 10. 8. 2026 pozdě večer — výběr šampiona podle Kamilova kritéria: v6

Kamilovo kritérium: **poplach = ≥2 DRONE okna na nahrávku**; dronová nahrávka
musí poplach spustit, ne-dronová nikdy; **řídit se bebop/membo materiálem**
(3 reálné nahrávky z mikrofonu + oba zdrojové loopy), Halmstad sekundárně,
**Salford vyřazen** („droni tam zní divně"). `pick_champion.py` prohledal
všech 10 kandidátů × prahy −2…12 s tvrdou podmínkou nula ≥2-okenních FP:

- **Šampion: E6 (win14 + bohaté příznaky, BEZ mixování) @ práh +7,25 → nasazen
  jako v6** (`mlp_model_data_v6.h`, build 11 na desce). Bebop/membo poplachy
  4/5 (tichy 3 okna, stredni 3, bebop loop 59/69, membo loop 66/67 s logity
  do +35; jediný miss = clipnutá nahrávka), Halmstad 10/15 aspoň jedno okno,
  jediné zbloudilé single okno (HELICOPTER_030) na 35 negativech, **0 falešných
  poplachů**. Druhý v pořadí v4 (3/5), pak E7/E4.
- Firmware změny: default `thr_milli` = **7250** (detector.h; práh patří
  aktivnímu modelu), CLI rozsah thr rozšířen na ±20000 (cli.c), include → v6.
  `board_session.py` default thr rovněž 7250.
- **Živě na desce (build 11):** ticho 17/17 noise (logity −7,6…−45); přehrávaný
  bebop loop 20 s → **3 DRONE okna (+8,9/+10,6/+14,1), 0 FP** = poplachové
  kritérium splněno. Pozn.: metodicky jde o vyladění pracovního bodu na
  známém materiálu (loopy sdílí původ s trénovacími daty) — pro terén zůstává
  v5e/E7 a sběr reálných nahrávek.
- Zálohy ELF: v1_build6, v2nm_build7, v4_build8, v5_build9, v5e_build10;
  aktuální = **v6 build 11**.

**Dlouhá živá validační série (tamtéž, v6 @ default 7250, squelch 3):**

| test | výsledek |
|---|---|
| ticho 60 s | ✅ 0/52 oken |
| vrtulník z repráků (HELICOPTER_030 = nejtěžší negativ) | ✅ **poplach nespuštěn** — 1/66 zbloudilé okno (+8,9), přesně dle PC predikce |
| hudba z repráků 30 s | ✅ 0/23, max −6,3 |
| bebop loop | ⚠️ ve 12:35 poplach ✓ (3 okna +8,9…+14,1); od ~13:00 špičky jen +3,5 |
| membo loop | ⚠️ 0/66 (testováno až po degradaci akustiky) |

Nález: mezi 12:35 a 13:05 se **změnila akustická cesta reproduktor→mikrofon**
(ověřeno nahrávkou `rec-20260810-130859.wav`: stejná hlasitost, spektrální
těžiště −500 Hz, PC i deska shodně max −2,5 → **end-to-end parita PC↔deska
potvrzena**; po částečné nápravě +3,5). Falešně-poplachová strana kritéria je
tedy živě neprůstřelná (3/3 čisté vč. vrtulníku přímo do mikrofonu); detekční
strana vyžaduje kvalitní přehrávací setup — kontrola: `detect 20 3` musí na
loop dávat dec ≥ +8. Snižování prahu NENÍ řešení (vrtulníková zbloudilá okna
+8,9 leží nad degradovanými bebop špičkami +3,5). Skutečná odpověď = reálné
nahrávky/dron (sběr dat, §8.4).

**Nasazení v5e (na Kamilovo přání, dříve večer — nahrazeno v6):** E7 ensemble exportován jako
`mlp_model_data_v5e.h` (`export_v5e.py`; 3 členové s vlastními scalery,
21,1 kB, parita všech členů OK) a **flashnut jako build 10** —
`svm_classifier.c` má třetí větev `#if defined(MLP_ENSEMBLE)` (průměr logitů
3 sítí). Ticho na desce: 17/17 noise (logity −1,6…−4,1 — ensemble průměruje,
menší magnitudy než v5), overrun=0. Očekávání pro test s repráky: v5e je
záměrně konzervativní na přehrávkách; na plné demo s repráky flashnout zpět
`bom-stm32node_v5_build9.elf`. Zálohy ELF: v1_build6, v2nm_build7, v4_build8,
v5_build9, aktuální = v5e build 10.
