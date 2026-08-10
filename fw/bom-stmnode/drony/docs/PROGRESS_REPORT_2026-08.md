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
