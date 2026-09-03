# 005 — Validace detektoru na reálných nahrávkách z PDM mikrofonu

**Status:** ready · **Priorita:** P1 · **Datum plánu:** 2026-08-07
**Cíl:** poprvé protáhnout reálná data z mikrofonního řetězce (Nucleo H563 + PDM mic → USB) přes MFCC+SVM detektor a zjistit, jestli sedí úrovně (squelch, scaler) a jak model reaguje na skutečné pozadí a přehrávané drony.

## Co už je připraveno (2026-08-06)

| Věc | Kde / stav |
|---|---|
| Firmware (PDM→PCM→USB CDC, `stream`/`streamtest`) | `fw/bom-stm32node/build/Debug/bom-stm32node.elf` — zbuildováno, konfigurace pro 250 MHz ověřena (HSE 25 MHz, PLL ×40/2, I-cache, pdm_pcm -O2) |
| Flash nástroj | STM32CubeProgrammer v2.21.0: `C:\Programy\PG\bin\STM32_Programmer_CLI.exe` — ověřen |
| Host CLI (nahrávání) | `fw/apps/stm32node-cli/.venv` — nainstalováno, funguje (`ports`, `record`, `tui`) |
| Vyhodnocení | `src/analysis/validate_real_wav.py` + `fw/bom-stmnode/drony/.venv` — otestováno na vzorcích (dron +1.14 DRONE, šum −3.0 noise) |
| Cílové složky | `data/recordings/background`, `data/recordings/playback`, `data/unseen` (vše gitignorováno) |
| Oprava decimace | `audio_sai_handler.c` už decimuje `[::3]` bez vadného 15-tap FIRu (zkompilováno host gcc) |

## 0) Fyzická příprava (ruce na desce)

- [ ] **SB3/SB4 = ON** (25MHz krystal). Bez toho jádro běží ~83 MHz → mic overrun. Firmware to nezajistí.
- [ ] Mikrofon zapojen jako dosud: data **D1 = PE6**, clock **CK1 = PE2**.
- [ ] Připojit **obě USB**: ST-Link (flash) i uživatelský USB konektor desky (CDC konzole + data).

## 1) Flash firmwaru

Z kořene repa (`C:\Users\Kamil\Documents\boomchecker-monorepo`):

```powershell
# volitelně: ověřit, že je ST-Link vidět
C:\Programy\PG\bin\STM32_Programmer_CLI.exe -l

# flash + verifikace + reset
C:\Programy\PG\bin\STM32_Programmer_CLI.exe -c port=SWD -w fw\bom-stm32node\build\Debug\bom-stm32node.elf -v -rst
```

- Očekávání: `File download complete` + `Download verified successfully`, deska se resetuje.
- Když connect selže: zkusit `-c port=SWD mode=UR reset=HWrst`; zkontrolovat kabel/Device Manager.
- Po každém flashi/resetu se CDC port re-enumeruje — pár sekund počkat, než se objeví.

## 2) Najít COM port desky

```powershell
cd fw\apps\stm32node-cli
.venv\Scripts\python -m stm32node_cli ports
```

- Hledat popis **boomchecker-node** (VID:PID 0483:5710). Pozor: COM3/COM4/COM6 na tomhle PC jsou Bluetooth — ignorovat.
- Číslo portu (dále `COMx`) si poznamenat; po flashi se může změnit.

## 3) Smoke test USB — syntetický tón (bez mikrofonu)

```powershell
.venv\Scripts\python -m stm32node_cli record 5 --port COMx --test-tone --out ..\..\bom-stmnode\drony\data\recordings
```

- Očekávání: WAV ~5 s, čistý 1kHz tón, hlášení **`overrun=0 err=0`**.
- Ověřuje enumeraci, PCM1 framing a dekódování nezávisle na mikrofonu.
- Soubor vznikne jako `rec-YYYYmmdd-HHMMSS.wav` — smoke testy lze pak smazat.

## 4) Test mikrofonu + kontrola plné rychlosti (250 MHz)

```powershell
.venv\Scripts\python -m stm32node_cli record 10 --port COMx --out ..\..\bom-stmnode\drony\data\recordings\background
```

Kontrolní matice:

| Pozorování | Význam | Akce |
|---|---|---|
| trvá ~10 s, `overrun=0 err=0`, ve WAV slyšet místnost | vše OK, jádro na 250 MHz | pokračovat |
| trvá ~30 s a/nebo `overrun=1` | **jádro ~83 MHz → SB3/SB4 špatně** | přepnout bridges, znovu |
| `err=1`, WAV ticho | mic nedodává data | zkontrolovat PE6/PE2, zapojení |
| samý sykot/šum bez reakce na tlesknutí | špatný slot/mic | ověřit zapojení D1 |

- Do nahrávky **tleskni** — v přehrávači musí být tlesknutí zřetelné a nezkreslené.

## 5) Sběr nahrávek

Po každé nahrávce soubor hned **přejmenovat popisně** (výchozí jméno je jen timestamp).

**Pozadí (reálné negativy)** → `data\recordings\background\`:
- [ ] 2× 20 s tichá místnost (`record 20`)
- [ ] 2× 20 s běžný provoz: řeč, psaní, kroky
- [ ] 1× 20 s hluk z okna / ulice (pokud lze)
- [ ] 1× 20 s hudba/TV z reproduktoru (těžší negativ + kontrola, že repro samo nedělá falešné pozitivy)

**Přehrávané drony (pseudo-pozitivy)** → `data\recordings\playback\`:
- [ ] 3–5 klipů dronů přehrát z reproduktoru (~0,5–2 m, střední hlasitost), každý `record 20`
  - ideálně z nezávislého datasetu v `data\unseen\` (Halmstad: <https://github.com/DroneDetectionThesis/Drone-detection-dataset>)
  - pro začátek poslouží i trénovací `wav\...\Binary_Drone_Audio\yes_drone\*.wav` (vědět, že je to „viděná" distribuce)
  - do názvu nahrávky zapsat, který zdrojový klip hrál (např. `playback_halmstad_drone01.wav`)

## 6) Vyhodnocení

```powershell
cd ..\..\bom-stmnode\drony    # = fw\bom-stmnode\drony
.venv\Scripts\python src\analysis\validate_real_wav.py "data\recordings\background\*.wav"
.venv\Scripts\python src\analysis\validate_real_wav.py "data\recordings\playback\*.wav"
```

Co sledovat:

1. **Úrovně vs. squelch (0.010):** pozadí smí být pod (gate drží ticho), přehrávaný dron musí být **nad** — jinak přidat hlasitost repra nebo blíž.
2. **Decision hodnoty:** pozadí hluboko pod 0.5 (ideálně < 0), dron nad 0.5. Hraniční případy zkusit s `--threshold 0.3`.
3. **Citlivost na úroveň:** `--gain-db -6`, `+6`, `+12` — jak moc decision „jezdí" s hlasitostí (ukáže, jestli bude nutná kalibrace zisku / retrén scaleru).
4. **`--no-squelch`** na pozadí: kolik oken by bylo DRONE bez gate = surová FP míra na reálném pozadí.
5. **Clipping:** `peak` blízko 1.0 → ztlumit repro, nahrát znovu.

## 7) Zápis výsledků a rozhodnutí

- [ ] Poznamenat: typické RMS pozadí, RMS přehrávaného dronu, rozsah decision u obou tříd.
- [ ] Rozhodnout: (a) úrovně sedí → jít na integraci do FW; (b) nesedí → kalibrace zisku, případně retrén scaler+SVM na reálných nahrávkách (`train_svm.py` umí přijmout nové adresáře).

## Troubleshooting

| Problém | Řešení |
|---|---|
| Port není v `ports` | jiný USB port/kabel; Device Manager; počkat po resetu; deska se hlásí jako „boomchecker-node" |
| `record` spadne uprostřed | board se resetl / kabel; spustit znovu (retry je vestavěný) |
| Objeví se „usage: stream <sec>" místo dat | posláno mimo rozsah 1–60 s |
| WAV kratší než požadováno | sledovat trailer — `err=1` znamená doplněné ticho |

## Návaznost

Po úspěšné validaci → **todo 004** (integrace real-time pipeline do MCU): `mfcc_processor` + `svm_classifier` + upravený `audio_sai_handler` jako konzument `mic_poll()` v `fw/bom-stm32node`, CMSIS-DSP do CMake, CLI příkaz `detect`. Klíčová čísla: volný CPU rozpočet ~4 ms/21,33ms blok; RAM detektoru ~50 kB (deska má 640 kB, obsazeno 56 kB).

---

## VÝSLEDKY 2026-08-07 (session s reálným HW)

**Ověřeno na desce:** 250 MHz reálně běží (10s stream = 10,1 s, overrun=0), USB/PCM1 protokol OK, mikrofon dodává čistý signál. ST-Link se neenumeruje (2 kabely vyzkoušeny) — flash odložen, na desce běží kompatibilní FW. Ambient RMS ~0,005 ≈ trénovací škála (gain +24 dB sedí).

**Nahrávky:** `data/recordings/background/` (ticho, ambient, řeč, tleskání, hudba z repra) + `data/recordings/playback/` (bebop 3 úrovně). Poznatky: max hlasitost repráků = zkreslení + clipping → nevalidní pozitivum; 7.1 virtualizaci při testech vypnout.

**Klíčový nález — citlivost na hlasitost:** zisk g posouvá pouze mean-c0 (o √20·ln g); w[0]=+1,71 je největší váha v1 → tichý dron skóruje jako šum. Ověřeno: predikce posunu +0,74/+12 dB vs. naměřeno +0,73.

**Retrén (src/analysis/train_svm_level_robust.py):** gain augmentace (−30..+3 dB, K=2) + syntetické téměř-ticho negativy (RMS ≤0,004) + reálné kotvy z bg_ticho/bg_ambient. Held-out: recall při −15 dB 0,91 (v1: 0,82). Na reálných nahrávkách ale trvá kompromis: tichý dron vs. „slyšitelné ticho" se v mean+std MFCC prostoru překrývají — lineární SVM je neoddělí (v2 bez tichých negativ: tichý dron 30/33, ale ticho 10/13 FP; v2c s nulovými FP při prahu 0,5: tichý dron jen 5/33).

**Artefakty:** `models/drone_detector_svm_v2.pkl`, `models/scaler_v2.pkl`, `src/firmware/Inc/svm_model_data_v2.h` (v1 header netknutý). Rozhodnutí o nasazení v2 do FW odloženo do lepší validace.

**Další kroky (todo #8):**
1. Sběr dat: venkovní/další pozadí (trénovací negativy ≠ validační), přehrávání přes kvalitnější repro na 2–5 m při střední hlasitosti, unseen drony (Halmstad → `data/unseen/halmstad/`), ideálně reálný dron.
2. Zvážit bohatší příznaky pro tichý konec (harmonicita/spektrální plochost, max-pooling přes rámce) nebo malý MLP (26→32→1 ≈ 1 kB flash) místo lineární hranice.
3. Provozní vrstva: vyžadovat K po sobě jdoucích DRONE oken před hlášením.
4. ST-Link: vyzkoušet LED/jiný port/kabel → pak flash a příprava `detect` příkazu.

## UNSEEN TEST 2026-08-07 (Halmstad + Salford, squelch 0.005 / práh 0.5)

`src/analysis/eval_unseen_datasets.py`; soubor-OK: dron = ≥1 okno DRONE, ne-dron = 0 oken DRONE.

| dataset | skupina | v1 soubory | v1 okna | v2 soubory | v2 okna |
|---|---|---|---|---|---|
| Halmstad | DRONE (30) | 18/30 | 41 % (+0,24) | 20/30 | 49 % (+0,39) |
| Halmstad | BACKGROUND (30) | 27/30 | 98 % (−2,43) | 16/30 | 85 % (−1,69) |
| Halmstad | HELICOPTER (30) | 22/30 | 92 % (−1,05) | 17/30 | 81 % (−0,57) |
| Salford | DRONE přelety (9) | 0/9 | 0 % (−2,08) | 2/9 | 1 % (−1,42) |
| Salford | kalibrace (2) | 2/2 | 100 % | 2/2 | 100 % |

Závěry: (1) v2 zvedá senzitivitu na drony, ale platí falešnými poplachy na neviděných pozadích (Halmstad background 27→16 souborů čistých); (2) vrtulníky matou oba modely (8 resp. 13 FP souborů z 30) → do v3 tréninku přidat vrtulníky jako explicitní negativy (train/val split Halmstadu!); (3) reálné přelety na vzdálenost (Salford) nedetekuje prakticky nikdo (0/9 vs 2/9) — to je hlavní mezera pro v3 (pozitivní data z dálky + bohatší příznaky/malý MLP).

## v3 RETRÉN 2026-08-07 (Halmstad/Salford train-poloviny + vrtulníky jako negativy)

Trénink navíc: 138 seg. Halmstad dronů, 150+150 seg. pozadí+vrtulníků, 110 seg. Salford přeletů (1s segmenty, sudé soubory; liché = validace). Artefakty: `*_v3.pkl`, `svm_model_data_v3.h`.

Validační poloviny (sq 0,005 / práh 0,5; soubory OK a % oken):
| skupina | v1 | v2 | v3 |
|---|---|---|---|
| Halmstad DRONE (15) | 7/15, 38 % | 9/15, 48 % | 9/15, 56 % |
| Halmstad BACKGROUND (15) | 14/15, 99 % | 9/15, 82 % | 8/15, 82 % |
| Halmstad HELICOPTER (15) | 11/15, 94 % | 7/15, 80 % | 7/15, 82 % |
| Salford přelety (4) | 0/4, 0 % | 1/4, 1 % | 2/4, 5 % |

Reálné nahrávky: v3 = maximální senzitivita (tichý dron 32/33, střední 42/44 i při prahu 0,5!), ale ticho 9–10/13 FP, ambient 3–5/13 FP → nenasaditelné jako v2.

**ZÁVĚR SÉRIE v1→v3: lineární SVM na mean+std MFCC narazil na strop kapacity** — přidávání dat posouvá pracovní bod po křivce senzitivita↔specificita, ne nad ni (v1 = konzervativní/slepý na dálku; v3 = citlivý/hlučný). Další krok NENÍ další retrén stejné architektury, ale bohatší reprezentace: (a) příznaky harmonicity/spektrální plochosti (drony = tonální harmonické, pozadí/vítr = širokopásmové), (b) malý MLP 26→16→1 (~450 vah ≈ 1,8 kB float, triviálně přenositelný do C), (c) provozně K-z-N okenní hlasování. Všechny tři headery (v1/v2/v3) zachovány — přepnutí ve FW je výměna jednoho include.

## ST-LINK DIAGNÓZA 2026-08-07 (uzavřeno, neřešíme teď)

Příznaky: napájecí LED svítí, **COM LED bliká červeně = ST-Link procesor žije a čeká na enumeraci**, ale PC nezaznamená ŽÁDNOU USB událost (ani neúspěšný pokus — ověřeno Kernel-PnP logem). Vyzkoušeno: 2 kabely (vč. prokazatelně funkčního z CDC), jiné porty PC, čistý power-cycle, otočení USB-C konektoru o 180°. Závěr: **mrtvá datová cesta USB konektoru ST-Link části** (napájení OK, data ne). Deska je plně použitelná přes CDC.

Náhradní plán flashování (až bude potřeba, todo 004): (a) **DFU bootloader** — STM32H563 ROM bootloader přes uživatelský USB: BOOT0 na 3V3 při resetu → `STM32_Programmer_CLI -c port=USB1 -w <elf>`; (b) půjčený ST-Link / jiné Nucleo (CN4 propojky + SWD dráty).

## DETECT INTEGROVÁN DO FIRMWARU 2026-08-07 (todo 004 — kód hotov, čeká na flash)

Detektor je zaintegrovaný do `fw/bom-stm32node` a **build prochází** (RAM 13,4 %, FLASH 6,1 %):
- `Core/Src/detector.c` (+ .h) — konzument `mic_poll()` bloků: decimace `[::3]` s fázovou návazností mezi bloky, FIFO 16 kHz, rámce 1024/hop 512, squelch 0,010 s resetem akumulace, 14 rámců → mean+std → lineární SVM (práh 0,5). Výstup: `DET t=<s> dec=<±d.ddd> DRONE|noise` + `DETEND windows= drones= overrun= err=`.
- `Core/Src/mfcc_processor.c`, `svm_classifier.c` + hlavičky a tabulky zkopírovány z drony; **aktivní model = v1** (`svm_model_data.h`); přepnutí na v2/v3 = úprava include v `svm_classifier.c` (hlavičky `_v2.h`/`_v3.h` jsou vedle).
- `third_party/CMSIS-DSP/` — vendorováno 8 skupin zdrojů v1.10.0 (combined TU), `-O2` na DSP soubory, gc-sections ořeže nepoužité tabulky.
- `Core/Src/dfu_boot.c` + CLI příkaz **`dfu`** — softwarový skok do ROM bootloaderu (adresa 0x0BF87000 dle AN2606 pro H56x; NETESTOVÁNO do prvního flashe).
- `cli.c`: příkazy `detect <sec>` a `dfu` (5/8 slotů obsazeno). `mic_dma_init()` má nově idempotентní guard (stream i detect ho lazy-volají).
- Host: `spec.py` + přegenerovaný `PROTOCOL.md`; testy stm32node-cli 29/30 (1 preexistující Windows-only fail v test_wav path separátorech).

**První flash (bez ST-Linku):** BOOT0 na 3V3 + reset → deska se ohlásí „STM32 BOOTLOADER" → `C:\Programy\PG\bin\STM32_Programmer_CLI.exe -c port=USB1 -w fw\bom-stm32node\build\Debug\bom-stm32node.elf -v` → sundat propojku, reset. Každý další flash pak: příkaz `dfu` po sériové lince → port=USB1 flash (bez propojky).

**Ověření po flashi:** `version` → `streamtest 5` → `stream 10` (overrun=0) → `detect 10` v tichu (očekávám žádná/záporná okna) → `detect 20` s přehrávaným bebopem nahlas (očekávám DRONE okna; srovnat decision hodnoty s `validate_real_wav.py` na paralelním záznamu).
