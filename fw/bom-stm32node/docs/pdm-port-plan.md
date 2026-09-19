# Přenos PDM akvizice z Mik_stm do bom-stm32node (vícefázově)

## Kontext

V `fw/bom-stm32node/Mik_stm/` je odladěný STM32CubeIDE projekt (Nucleo-144 MB1404), kde
funguje akvizice z PDM mikrofonu MP23DB01HP přes SAI1 + GPDMA + ručně psaný DSP
(CIC5 → DC blocker → 101-tap FIR → 48 kHz PCM). Vše je smíchané v jednom
`Core/Src/main.c` (1209 řádků) spolu s obsluhou UART konzole a streamingem.

Cíl: přenést akviziční/DSP vrstvu do základního projektu `fw/bom-stm32node/` (vlastní
deska, CMake+Ninja+Taskfile) tak, aby zapadla do větší IoT jednotky (LoRa, GPS, 1PPS,
později detekce dronu). Postupujeme **ve třech fázích**, každá zvlášť ověřitelná.

**Dělba práce (potvrzeno):**
- **Uživatel** si sám v CubeMX udělá HW konfiguraci (SAI 16-bit frame, GPDMA circular,
  PLL2 pro 3.072 MHz CK1, boost CPU na ~250 MHz). Do `.ioc`, `SystemClock_Config()` ani
  generovaných částí `sai.c` **nezasahuji**.
- **Já** dodám aplikační/DSP vrstvu jako samostatné moduly v USER CODE blocích.
- Mikrofon čteme z **D1 / PE6** (jeden MEMS mik, jako Mik_stm).
- **Tento plán rozdělíme na fáze; každá fáze má vlastní commit a ověření.**

Multi-fázový plán zrcadlíme i do repa jako `fw/bom-stm32node/docs/pdm-port-plan.md`
(vytvořím při implementaci fáze 1), aby žil vedle kódu.

---

## Fáze 1 — Čistý port PDM akvizice (bez nahrávání)

Cíl: dostat do základního projektu SAI+DMA akvizici a DSP tak, že v RAM vzniká validní
48 kHz PCM. Žádný výstupní transport, žádná SD karta.

### Moduly
**`pdm_pcm.c` / `pdm_pcm.h`** — čistý DSP, bez HAL. Extrakce z Mik_stm `main.c`:
- konstanty `main.c:53-72` (`PDM_SCK_HZ`, `PDM_RING_HALFWORDS`, `PCM_FS_HZ`=48 kHz,
  `PCM_SAMPLES_PER_HALF`=1024, `PCM_GAIN`, `FIR_TAPS`=101)
- `pcm_process_half()` (`main.c:248-319`): CIC5 → DC blocker → FIR → saturace
- FIR koeficienty (Hanning-windowed sinc, Q15), `sat16()`, reset stavu filtrů
- stav filtru (`cic_i1..i5`, `cic_c1..c5`, `dc_acc`, `dc_seeded`, `pcm_mute`, `fir_x[]`,
  `fir_lp[]`) → zapouzdřit do `typedef struct pdm_pcm_t`
- API: `pdm_pcm_init(st, slot_mask)` (default `0xF807` = kanál A, −92 dBFS),
  `pdm_pcm_process_half(st, src, dst)`

**`mic.c` / `mic.h`** — akvizice (HAL-facing). Z Mik_stm `main.c:449-585`:
- vlastní `pdm_ring[PDM_RING_HALFWORDS]` (32 kB circular DMA buffer)
- `mic_dma_init()` — GPDMA1_Ch0 circular linked-list nad `SAI1_Block_A->DR`
  (port `MIC_DMA_Init`, `main.c:451-507`); **necháváme v kódu** (linked-list je ověřený)
- PDM sample delay `HAL_SAIEx_ConfigPdmMicDelay()`
- `mic_start()` → `HAL_SAI_Receive_DMA(&hsai_BlockA1, pdm_ring, PDM_RING_HALFWORDS)`, `mic_stop()`
- callbacky `HAL_SAI_RxHalfCpltCallback/RxCpltCallback/ErrorCallback` (`main.c:554-585`)
  → nastavují `half0_ready`/`half1_ready`, hlídají overrun
- `mic_poll(int16_t *pcm_out, size_t *nsamp)` — zpracuje hotovou polovinu přes
  `pdm_pcm_process_half()`, vrátí 1024 vzorků PCM
- závislost: `extern SAI_HandleTypeDef hsai_BlockA1;` (z CubeMX `sai.c`)

### Integrace do `main.c` (jen USER CODE)
- `USER CODE Includes`: `#include "mic.h"`
- `USER CODE 2`: `mic_dma_init(); mic_start();`
- `USER CODE WHILE`: `if (mic_poll(pcm, &n)) { /* zatím jen do RAM ring/statistika */ }`
- callbacky žijí v `mic.c`, takže regenerace `main.c` je nepřepíše

### Build
Nové zdrojáky do **kořenového** `CMakeLists.txt` (řádky 46-53, `# Add user sources here`
a `# Add user defined include paths` — soubor se z CubeMX negeneruje znovu):
```cmake
target_sources(${CMAKE_PROJECT_NAME} PRIVATE Core/Src/pdm_pcm.c Core/Src/mic.c)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE Core/Inc)
```

### Ověření fáze 1
- `task build` projde bez chyb.
- Po flashi běží DMA: `RxCpltCallback` toggluje LED (kontrola, že SAI+DMA jedou).
- Debuggerem / watchem: `pcm` buffer obsahuje smysluplná (ne-DC, ne-saturovaná) data,
  žádný `stream_overrun` → DSP stíhá real-time. (Bez transportu je to zatím jen na
  debuggeru; reálný poslech přijde ve fázi 3.)

---

## Fáze 2 — VCP konzole přes embedded-cli

Cíl: interaktivní ovládání přes VCP, ať jde spouštět akvizici/nahrávání a číst stav.

- Vendorovat **embedded-cli** (single-header, funbiscuit/embedded-cli) do
  `Core/Src`/`Core/Inc` nebo `third_party/` (repo už používá `scripts/.../third_party/`).
- `cli.c` / `cli.h` — napojení embedded-cli na **USART1 VCP** (PB14/PB15, 115200; příjem
  po znaku v IRQ nebo poll, výstup přes `HAL_UART_Transmit`).
- Příkazy (první sada): `status` (stav SAI/DMA, overrun, počet vzorků), `start`/`stop`
  akvizice, `peak` (aktuální špička PCM pro rychlou kontrolu, že mik „slyší").
- Integrace: v `USER CODE WHILE` volat `cli_process()`; příkazy volají `mic_*`.

### Ověření fáze 2
- Připojit se terminálem na VCP, `status`/`start`/`stop` reagují; `peak` roste při
  zvuku poblíž mikrofonu.

---

## Fáze 3 — SD karta, nahrávání a finální ověření

Cíl: nahrát PCM na SD kartu jako WAV a ověřit poslechem na PC.

- **FatFS** přidá uživatel v CubeMX (middleware, „User-defined" diskio), nebo vendorujeme.
  Deska nemá SDMMC → SD přes **SPI5** (PF7/PF8/PF9, CS PF3, detect PF2).
- `sd_spi.c/.h` — SD-over-SPI driver + FatFS `diskio` glue nad `hspi5` (init, read/write
  sektor). Největší nová komponenta (v Mik_stm chybí).
- `rec_wav.c/.h` — `rec_wav_start("/REC000.WAV")` (44B WAV hlavička, mono/48k/16-bit),
  `rec_wav_write(pcm, n)`, `rec_wav_finish()` (dopočítá délky).
- CLI příkaz `rec <sec>` (fáze 2) spustí nahrávání N sekund; `mic_poll` → `rec_wav_write`.

### Ověření fáze 3 (end-to-end)
- `rec 5` nahraje `/REC000.WAV`; kartu přečíst v PC, WAV musí znít čistě (1 kHz tón /
  řeč), bez sykotu. Porovnat s `Mik_stm/tools/pdm_capture.py` výstupem a
  `Mik_stm/hlas_cisty.wav`. Kontrola: bez `stream_overrun`, správná amplituda.

---

## CubeMX kontrakt (checklist pro uživatele, napříč fázemi)
- **CPU**: SYSCLK ~250 MHz, VOS0, odpovídající FLASH latency (kvůli real-time CIC+FIR)
- **PLL2** z jitter-čistého zdroje → SAI1 kernel 12.288 MHz; NODIV=1, MCKDIV=2 →
  SCK 6.144 MHz → **CK1 = 3.072 MHz**
- **SAI1_A PDM master RX**: DataSize 16, FrameLength 16, ActiveFrameLength 1, 1 slot,
  SlotActive 0, MSB first, ClockStrobing FALLING, MicPairsNbr 1, CLOCK1 enable, jen D1 (PE6)
- **GPDMA1_Channel0**: request SAI1_A, povolený `GPDMA1_Channel0_IRQn` (linked-list ale
  stavíme v `mic.c` — CubeMX SAI RX nesmí přepsat na normal-mode kanál)
- **PE6** = SAI1_D1 (speed VERY_HIGH), **PE2** = SAI1_CK1
- Fáze 3: **FatFS** + SPI5; USART1 VCP ponechat pro CLI
- handle názvy: `hsai_BlockA1`, `huart1` (VCP), `hspi5` (jinak upravíme `extern` v modulech)

## Rizika / poznámky
- **HAL verze**: základ v1.7.0 vs Mik_stm FW_H5 V1.5.1 — ověřit signatury
  `HAL_DMAEx_List_*` a `HAL_SAIEx_ConfigPdmMicDelay`.
- **CPU takt**: dokud uživatel neudělá boost na ~250 MHz, DSP na 64 MHz nemusí stíhat —
  ověření fáze 1 má smysl až po boostu.
- **SD přes SPI (fáze 3)**: největší nová část; počítat s laděním diskio + rychlosti SPI5
  (nutno stíhat 96 kB/s zápis).
