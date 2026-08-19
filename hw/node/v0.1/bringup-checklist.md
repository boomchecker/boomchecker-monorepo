# Bring-up checklist — BCH Node v0.1

Seznam testů pro **první revizi** desky `bch-stmnode_v0.1`. Cílem není změřit
parametry na papír, ale zjistit, **co na desce vůbec žije** a co je potřeba
opravit v `v0.2`. U každého testu je proto uvedeno jak "OK kritérium", tak
"co ještě přežijeme" — spousta věcí (slabší GNSS signál, nižší citlivost
mikrofonu, kratší dosah LoRa) je v první revizi akceptovatelná, pokud to
funguje principiálně a jde to dohnat v FW nebo drobnou úpravou.

Zdroje: `bch-stmnode_v0.1.PrjPcb`, `outputs/bom_bch-stmnode_v0.1.xlsx`,
`outputs/schema_boomchecker-stmnode_v0.1.PDF`, pinmapa z
`fw/bom-stm32node/bom-stm32node.ioc` (pozor — draft, viz [§13](#13-nesrovnalosti-k-ověření-proti-schématu)).

## Jak to používat

- Testy jdou **v pořadí** — sekce 0–2 jsou gate: dokud neprojdou, nemá smysl
  zkoušet periferie. V rámci sekcí 3–9 už lze pořadí měnit.
- **P0** = blokující pro další práci, **P1** = potřebné pro funkci jednotky,
  **P2** = nice-to-have / dá se odložit na v0.2.
- 🔧 = **dnes na to není firmware**, test si vyžaduje nový bring-up kód (viz
  [§14](#14-co-je-potřeba-dopsat-do-firmwaru)). Bez značky jde test udělat
  se současným `fw/bom-stm32node` (`task build`, `task flash`, CLI přes CDC).
- Výsledky zapisuj do tabulky v [§15](#15-záznam-měření), fail → issue s
  labelem `hw` a odkazem na ID testu (např. `GPS-3`).

### Co je potřeba na stole

Multimetr, laboratorní zdroj s omezením proudu (5 V / 0,5 A), USB-C kabel,
ST-Link V3 (SWD na J3, FTSH-107 2×7 1,27 mm), osciloskop nebo logic analyzer
(min. 50 MHz — PDM hodiny jsou 3,072/6,144 MHz), Li-Ion článek s JST-ZR
konektorem (J4), 2× deska pro LoRa test, mikrofonní desky `onemic v0.1` +
FFC kabel 12p / 0,5 mm (J11), microSD karta, GNSS s výhledem na oblohu
(okno nestačí na první fix), zdroj hlasitého impulzu (tlesknutí, prasknutí
balonku).

---

## 0. Před prvním připojením napájení (P0)

| ID | Test | OK kritérium |
|---|---|---|
| PRE-1 | Vizuální kontrola osazení proti `cpm_bch-stmnode_v0.1.csv` — orientace U4 (STM32H563ZIT6), U1 (Teseo-LIV3R), U6 (E22-900M22S), U2/U3 (LGA), polarita D1, tantal/elyt polarity | Nic obráceně, žádné chybějící součástky proti BOM |
| PRE-2 | Ohmmetrem odpor GND ↔ +3V3, GND ↔ VBAT, GND ↔ VBUS (5 V) | Žádný zkrat (jednotky Ω a méně = stop), typicky desítky kΩ+ |
| PRE-3 | Kontrola J5 (USB-C), J6 (microSD), J10/J11 (FFC) pod lupou — mosty na pinech | Bez můstků |
| PRE-4 | S2 (slide switch) v poloze OFF / bez baterie | — |

- [ ] PRE-1 [ ] PRE-2 [ ] PRE-3 [ ] PRE-4

---

## 1. Napájení a nabíjení (P0)

Osazeno: J5 USB-C (USB4720-03-A) → F1 (0,75 A pojistka) → U7 (USBLC6-2SC6Y ESD),
U9 (TPS563252DRLR buck, L4 2,2 µH) → +3V3, U8 (BQ21040DBVR, Li-Ion charger),
J4 (JST S2B-ZR baterie), S2 (přepínač), D1 (zelená LED), U12/U13 (TPS22919
load switche pro LoRa a IMU).

| ID | Test | Postup | OK kritérium | Přežijeme |
|---|---|---|---|---|
| PWR-1 | První zapnutí z lab. zdroje | 5 V na VBUS přes limit 100 mA, bez FW | Proud < 50 mA, nic se nehřeje | — |
| PWR-2 | Výstup bucku U9 | Multimetr na +3V3 | 3,3 V ±3 % | ±5 % s poznámkou |
| PWR-3 | Zvlnění +3V3 | Osciloskop AC, 20 MHz BW limit | < 50 mV p-p | < 100 mV p-p (ale zapsat — ovlivní ADC/mikrofony) |
| PWR-4 | Spínací frekvence U9 | Osciloskop na SW / L4 | ~1,2 MHz, bez skákání do burst módu při zátěži | — |
| PWR-5 | Nabíjení baterie U8 | Článek na J4, USB připojené | Nabíjecí proud teče, napětí článku roste, U8 se nehřeje > ~60 °C | Nižší proud než návrhový |
| PWR-6 | Provoz z baterie | Odpojit USB, S2 na ON | Deska běží dál, +3V3 stabilní až po vybití článku | — |
| PWR-7 | Přechod baterie ↔ USB | Připojit/odpojit USB za běhu | MCU se nerestartuje | Reset při přepnutí = zapsat do v0.2 |
| PWR-8 | Funkce S2 | Přepnout OFF/ON | Deska se vypne/zapne dle záměru schématu | — |
| PWR-9 | D1 (zelená LED) | Po zapnutí | Svítí/blikne dle zapojení (power/charge indikace) | — |
| PWR-10 🔧 | Load switch LoRa (U12, `EN_LORA` = PE7) | Přepnout GPIO, měřit proud | Napájení E22 se dá vypnout i zapnout | — |
| PWR-11 🔧 | Load switch IMU (U13, `EN_IMU` = PG14) | Totéž | Napájení IMU/MAG jde ovládat | — |
| PWR-12 | Spotřeba v základních stavech | idle / GNSS tracking / LoRa TX / nahrávání | Zaznamenat čísla — vstup pro výdrž a v0.2 | Cokoli, hlavně mít změřeno |
| PWR-13 | Napětí baterie do MCU | Zkontrolovat, jestli existuje dělič na ADC | **Očekáváme, že chybí** — pak měřit multimetrem a zapsat jako požadavek na v0.2 | — |

- [ ] PWR-1 [ ] PWR-2 [ ] PWR-3 [ ] PWR-4 [ ] PWR-5 [ ] PWR-6 [ ] PWR-7 [ ] PWR-8 [ ] PWR-9 [ ] PWR-10 [ ] PWR-11 [ ] PWR-12 [ ] PWR-13

---

## 2. MCU, hodiny, debug, konzole (P0)

| ID | Test | Postup | OK kritérium | Přežijeme |
|---|---|---|---|---|
| MCU-1 | SWD spojení | ST-Link na J3, `task flash` v `fw/bom-stm32node` | OpenOCD najde STM32H5, flash + verify projde | — |
| MCU-2 | Reset tlačítko S1 | Stisk | MCU se restartuje (FW naběhne znovu) | — |
| MCU-3 | HSE start (Y1) | Debugger: HSE ready flag, případně MCO/pin osciloskopem | Krystal se rozběhne, žádný retry | — |
| MCU-4 | **Frekvence Y1 vs. konfigurace** | Osciloskopem nebo z BOM: Y1 = 24 MHz, ale `.ioc` má `HSE_VALUE=25000000` | Sjednotit — jinak jsou špatně všechny odvozené hodiny (UART baud, SAI, USB) | Nic, tohle je must-fix |
| MCU-5 | LSE start (Y2, 32,768 kHz) | Debugger: LSE ready | Rozběhne se do pár set ms | Když ne, RTC/timestampy řešit z GNSS |
| MCU-6 | SYSCLK 250 MHz | Ověřit reálně (MCO nebo časování známé smyčky/GPIO toggle) | Odpovídá konfiguraci; PDM DSP na 250 MHz stíhá | — |
| MCU-7 | USB CDC enumerace | Připojit USB-C k PC | Objeví se CDC ACM, product string `boomchecker-node` | — |
| MCU-8 | Konzole | `version` na CDC portu | Vypíše verzi FW, prompt `> ` reaguje | — |
| MCU-9 | USART1 VCP (PB14/PB15) | Připojit na piny / ST-Link VCP | Text prochází oběma směry | P2 — je záloha ke CDC |
| MCU-10 | Teplota U4 při zátěži | Prst / termokamera po 10 min nahrávání | Nehřeje se abnormálně | — |

- [ ] MCU-1 [ ] MCU-2 [ ] MCU-3 [ ] MCU-4 [ ] MCU-5 [ ] MCU-6 [ ] MCU-7 [ ] MCU-8 [ ] MCU-9 [ ] MCU-10

---

## 3. GNSS — Teseo-LIV3R (P1)

Signálová cesta: J1 (patch antena 18×18 THM) → U10 (SAW B39162B4327P810) →
U5 (AT2659S LNA) → U1 (Teseo-LIV3R). Rozhraní: I2C1 (`GPS_SCL` PB6 / `GPS_SDA` PB7),
UART4 (`GPS_TX` PB8 / `GPS_RX` PB9), `GPS_1PPS` PG15, `GPS_RESET` PB5,
`GPS_WAKE-UP` PB4.

| ID | Test | Postup | OK kritérium | Přežijeme |
|---|---|---|---|---|
| GPS-1 🔧 | Modul žije | Reset přes PB5, čtení NMEA z UART4 (19 200/9 600 Bd dle konfigurace ROM) | Přijdou validní NMEA věty (`$GPTXT`, `$GxGGA`) | — |
| GPS-2 🔧 | I2C cesta | Sken I2C1, čtení stejných dat po I2C | Modul odpovídá na své adrese | Když nefunguje, jedeme dál po UART (P2) |
| GPS-3 🔧 | **Cold start fix venku** | Deska pod otevřenou oblohou, čekat na `$GxGGA` s fix quality ≥ 1 | Fix do ~5 min od studeného startu, poloha ± desítky metrů odpovídá realitě | Fix do 15 min, horší přesnost — hlavně že fix je |
| GPS-4 🔧 | Kvalita signálu | `$GxGSV`: C/N0 sledovaných satelitů, počet satelitů ve fixu | ≥ 6 satelitů, C/N0 nejlepších ≥ 40 dB-Hz | 35–40 dB-Hz a ≥ 4 satelity = "slabší, ale přežijeme"; zapsat jako téma pro anténu/LNA ve v0.2 |
| GPS-5 | LNA/SAW napájení | Změřit proud do U5, přítomnost napětí na anténním pinu | LNA napájený, proud odpovídá katalogu (~4 mA) | — |
| GPS-6 🔧 | 1PPS existuje | Osciloskop na PG15 po fixu | Puls 1×/s, čistá hrana | — |
| GPS-7 🔧 | 1PPS jako interrupt | EXTI na PG15, počítat pulsy | Přesně 1 Hz, žádné zdvojení/zákmit (jinak dodělat filtr/hysterezi) | — |
| GPS-8 🔧 | 1PPS jitter | Porovnat s časovačem na 250 MHz clocku, statistika přes 100 pulsů | Jitter řádu jednotek µs | Desítky µs — zapsat, TDOA rozlišení tím trpí |
| GPS-9 🔧 | Reset a wake-up piny | Toggle PB5 / PB4 | Modul reaguje (restart NMEA, probuzení) | — |
| GPS-10 🔧 | Hot start | Restart MCU s běžícím GNSS | Fix zpět do ~několika sekund | — |

- [ ] GPS-1 [ ] GPS-2 [ ] GPS-3 [ ] GPS-4 [ ] GPS-5 [ ] GPS-6 [ ] GPS-7 [ ] GPS-8 [ ] GPS-9 [ ] GPS-10

---

## 4. LoRa — E22-900M22S / SX1262 (P1)

Rozhraní: SPI1 (`LORA_SCK` PA5, `MISO` PA6, `MOSI` PA7, `NSS` PA4), `BUSY` PB1,
`DIO1` PB2, `DIO2` PF11, `NRST` PC4, `TXEN` PB0, `RXEN` PC5, `EN_LORA` PE7.
Profil pro bring-up: 869,525 MHz / BW 125 kHz / SF7 / CR 4/5 / 14 dBm
(`fw/bom-stm32node/docs/radio-profile.md`).

> ⚠️ **Nikdy nevysílat bez antény** — E22 má PA, bez zátěže riskuješ modul.
> TX vždy až po ověření, že anténa/50 Ω zátěž je připojená.

| ID | Test | Postup | OK kritérium | Přežijeme |
|---|---|---|---|---|
| LORA-1 | Napájení a reset | `EN_LORA` on, `NRST` sekvence | Modul se probere, `BUSY` padne do low | — |
| LORA-2 | SPI komunikace | `radio status` na CLI | `radio: ready`, vypíše profil (869,525 MHz, SF7, …) | — |
| LORA-3 | TCXO | Součást `begin()` (DIO3 1,8 V) | Init projde bez `RADIOLIB_ERR_SPI_CMD_TIMEOUT` / XOSC chyby | — |
| LORA-4 | TX/RX switch | Sledovat `TXEN`/`RXEN` při TX a RX | Přepínají se korektně, nikdy oba naráz aktivní | — |
| LORA-5 | `radio ping` — jedna deska | `radio ping TEST` | `sent "TEST" (4 bytes)`, žádná chyba | — |
| LORA-6 | **Ping/pong dvě desky** | Na jedné `radio ping`, na druhé sledovat `radio rx: …` | Paket dorazí oběma směry, CRC OK | — |
| LORA-7 | DIO1 IRQ | EXTI na PB2 | TX done / RX done přijdou jako přerušení, ne pollingem | Polling jako fallback (P2) |
| LORA-8 | RSSI/SNR na stole | `radio status` po příjmu, desky ~1 m | RSSI plausibilní (silné, desítky -dBm), SNR > 5 dB | — |
| LORA-9 | Dosah | 2 desky, ~100 m volný prostor a pak přes jednu překážku | Paket ratio ≥ 90 % na 100 m | Slabší dosah je v revizi 0.1 OK — zapsat čísla, ne přeceňovat |
| LORA-10 | Proud při TX | Změřit peak při TX 14 dBm | Odpovídá katalogu, +3V3 nespadne a MCU se nerestartuje | — |
| LORA-11 | Výkon na výstupu | Pokud je k dispozici měřák výkonu / SDR + attenuator | Ověřit, že nepřekračujeme limit sub-bandu (viz `radio-profile.md`) | Odložit na později, ale nezapomenout |

- [ ] LORA-1 [ ] LORA-2 [ ] LORA-3 [ ] LORA-4 [ ] LORA-5 [ ] LORA-6 [ ] LORA-7 [ ] LORA-8 [ ] LORA-9 [ ] LORA-10 [ ] LORA-11

---

## 5. IMU — ISM330DLCTR (P1)

SPI3 (`IMU_SCK` PC10, `MISO`/SDO PC11, `MOSI`/SDI PC12), `IMU_CS` PD0,
`IMU_INT1` PD1, `IMU_INT2` PD3, napájení přes load switch (`EN_IMU` PG14).

| ID | Test | Postup | OK kritérium | Přežijeme |
|---|---|---|---|---|
| IMU-1 🔧 | WHO_AM_I | Čtení registru `0x0F` po SPI3 | Vrátí `0x6B` | — |
| IMU-2 🔧 | Akcelerometr — statika | Deska položená na stole | Jedna osa ~±1 g, ostatní ~0, převrácením se znaménko obrátí | — |
| IMU-3 🔧 | Gyroskop | Deska v klidu / rotace rukou | V klidu ~0 °/s (bias do jednotek °/s), při rotaci reaguje správná osa | — |
| IMU-4 🔧 | Data rate | Nastavit ODR, měřit skutečnou periodu vzorků | Odpovídá nastavení | — |
| IMU-5 🔧 | INT1/INT2 | Nakonfigurovat data-ready / wake-up | Přerušení skutečně přijde na PD1 i PD3 | Když jeden pin nechodí, stačí pro start jeden |
| IMU-6 🔧 | Vypnutí přes load switch | `EN_IMU` low → high, znovu init | Po zapnutí modul znovu odpovídá (čistý power cycle) | — |

- [ ] IMU-1 [ ] IMU-2 [ ] IMU-3 [ ] IMU-4 [ ] IMU-5 [ ] IMU-6

---

## 6. Magnetometr — LIS2MDLTR (P2)

Dle schématu a `.ioc` je na **I2C2** (`MAG_SDA` PF0, `MAG_SCL` PF1) — README
u desky tvrdí SPI, viz [§13](#13-nesrovnalosti-k-ověření-proti-schématu).

| ID | Test | Postup | OK kritérium | Přežijeme |
|---|---|---|---|---|
| MAG-1 🔧 | Sken I2C2 | Adresa `0x1E` | Zařízení odpovídá | — |
| MAG-2 🔧 | WHO_AM_I | Registr `0x4F` | `0x40` | — |
| MAG-3 🔧 | Měření pole | Klidové čtení 3 os | Velikost vektoru ~25–65 µT (zemské pole) | — |
| MAG-4 🔧 | Reakce na magnet | Přiblížit magnet | Hodnoty výrazně vyjedou a vrátí se | — |
| MAG-5 🔧 | Vliv desky | Porovnat čtení s LoRa TX a bez | Zaznamenat offset od proudů/plechů — kalibrace je FW práce | Velký offset je pro v0.1 OK |

- [ ] MAG-1 [ ] MAG-2 [ ] MAG-3 [ ] MAG-4 [ ] MAG-5

---

## 7. PDM mikrofony (J11 + `onemic v0.1`) (P0 — hlavní funkce jednotky)

J11 (12p FFC, 52745-1297) nese `PDM_CLK`, `PDM_D1`, `PDM_D2`, `PDM_D3`,
`LR_SEL`, +3V, GND na desky `hw/onemic/v0.1`. MCU: SAI1 (`PDM_CLK` PE2,
`PDM_D1` PE6, `PDM_D2` PE4, `PDM_D3` PF10) + GPDMA, DSP CIC5 → DC block →
101-tap FIR → 48 kHz PCM (`Core/Src/mic.c`, `pdm_pcm.c`).

| ID | Test | Postup | OK kritérium | Přežijeme |
|---|---|---|---|---|
| MIC-1 | USB řetěz bez HW | `stm32node_cli record 5 --test-tone` | WAV s čistým 1 kHz — potvrzuje USB/framing dřív, než řešíš mikrofon | — |
| MIC-2 | Napájení mikrofonní desky | Multimetr na +3V na J11 / `onemic` | 3,3 V na mikrofonech | — |
| MIC-3 | PDM hodiny | Osciloskop na `PDM_CLK` (PE2) | Hodiny běží, frekvence odpovídá konfiguraci SAI (3,072 / 6,144 MHz), amplituda plná, hrany čisté i na konci FFC | — |
| MIC-4 | **Nahrání zvuku (D1)** | `record 5 --port /dev/ttyACM*` a přehrát WAV | Ve WAV je slyšet zvuk z místnosti, není to šum ani ticho | — |
| MIC-5 | Šumové pozadí | Nahrát v tichu, spočítat RMS / noise floor | Klidný signál bez brumu 50 Hz a bez pískání ze spínacího zdroje | Trochu zvýšený noise floor OK, zapsat hodnotu |
| MIC-6 | Impulz (tlesknutí) | Tlesknout blízko mikrofonu | Ostrý impulz, po něm rychlý návrat — bez zaseknutého DC offsetu | — |
| MIC-7 | Sytost / clipping | Hlasitý zdroj blízko | Ověřit, kde signál klipuje; klipování se z něj vzpamatuje | — |
| MIC-8 | Overrun | Nahrávat 60 s | Žádný SAI overrun, žádné vypadlé bloky (kontinuita PCM) | — |
| MIC-9 🔧 | Druhý kanál na D1 (`LR_SEL`) | Přepnout `LR_SEL`, číst druhý slot | Dostaneme oba mikrofony z jedné datové linky | Když ne, jedeme 3 kanály místo 6 |
| MIC-10 🔧 | D2 a D3 | Rozšířit SAI na `PDM_D2` (PE4) a `PDM_D3` (PF10) | Data z obou dalších linek, stejná úroveň jako D1 | — |
| MIC-11 🔧 | **Fázová koherence kanálů** | Jeden impulz, nahrát víc kanálů naráz, porovnat náběhy | Kanály jsou vzájemně vzorkově zarovnané (konstantní offset) — podmínka pro TDOA | Konstantní známý offset se dá zkompenzovat; nestabilní offset je must-fix |
| MIC-12 | FFC mechanika | Zapojit/rozpojit 5× | ZIF drží, kontakt se neztrácí při ohnutí kabelu | — |

- [ ] MIC-1 [ ] MIC-2 [ ] MIC-3 [ ] MIC-4 [ ] MIC-5 [ ] MIC-6 [ ] MIC-7 [ ] MIC-8 [ ] MIC-9 [ ] MIC-10 [ ] MIC-11 [ ] MIC-12

---

## 8. Analogové mikrofony — OPA2607 + ADC (P1)

U11 (OPA2607IDR, dual op-amp) tvoří dva kanály `MIC1_I → MIC1_O` a
`MIC2_I → MIC2_O`, výstupy jdou na ADC (PA0 = `MIC1_O`, PA1 = `MIC2_O`).
Vstupy vedou na J10 (6p FFC, 52745-0696). V FW je zatím jen CubeMX init
ADC1/ADC2, žádné vzorkování.

| ID | Test | Postup | OK kritérium | Přežijeme |
|---|---|---|---|---|
| AMIC-1 | Napájení a klidový bod U11 | Měřit `MIC1_O`/`MIC2_O` bez signálu | Stabilní DC bias uprostřed rozsahu ADC (ne 0 V, ne rail) | Posunutý bias = ztráta rozsahu, zapsat pro v0.2 |
| AMIC-2 | Zisk | Vstříknout známý signál (generátor, mV) na J10 | Zisk odpovídá návrhu (poměr R v zapojení), bez oscilací | ±30 % OK |
| AMIC-3 | Šum bez mikrofonu | Osciloskop / ADC na výstupu | Bez rozkmitání, bez oscilace op-ampu (OPA2607 je rychlý — sledovat vf. zákmity) | — |
| AMIC-4 🔧 | Čtení z ADC | Jednorázová konverze PA0/PA1 | Hodnoty odpovídají tomu, co je na pinu | — |
| AMIC-5 🔧 | Vzorkování zvuku | ADC + timer + DMA na ~48 kHz, uložit blok | Ve vzorcích je zvuk, ne šum | — |
| AMIC-6 | Napájení mikrofonů na J10 | Změřit bias/napájení pro analogové mikrofony | Odpovídá požadavku použitého mikrofonu | — |
| AMIC-7 | Přeslech od spínaného zdroje | Nahrát s bucking běžícím pod zátěží | Bez tónu na spínací frekvenci a jejích alikvótech ve slyšitelném pásmu | Zaznamenat úroveň, řešení až v0.2 |

- [ ] AMIC-1 [ ] AMIC-2 [ ] AMIC-3 [ ] AMIC-4 [ ] AMIC-5 [ ] AMIC-6 [ ] AMIC-7

---

## 9. microSD karta (P1)

J6 (105162-0001, push-pull), SPI5 (`SD_SCK` PF7, `SD_MISO` PF8, `SD_MOSI` PF9),
`SD_CS` PF3, `SD_DET` PF2.

| ID | Test | Postup | OK kritérium | Přežijeme |
|---|---|---|---|---|
| SD-1 | Mechanika | Vložit/vysunout kartu 5× | Push-pull drží, karta nevypadává | — |
| SD-2 🔧 | Card detect | Čtení PF2 s kartou a bez | Stav se mění správně (ověřit i polaritu) | Když nefunguje, detekce softwarem přes init |
| SD-3 🔧 | Inicializace karty | SPI init sekvence, CMD0/CMD8/ACMD41, čtení CID/CSD | Karta odpoví, přečteme kapacitu | — |
| SD-4 🔧 | Čtení/zápis bloku | Zapsat a zpět přečíst known pattern | Data se shodují | — |
| SD-5 🔧 | FAT | Namountovat FS, vytvořit soubor | Soubor je čitelný na PC | — |
| SD-6 🔧 | Propustnost zápisu | Zapisovat kontinuální blok, měřit MB/s | Zvládne datový tok plánovaného počtu audio kanálů se zásobou | Nižší rychlost = méně kanálů / kratší okna, zapsat |
| SD-7 🔧 | Zápis při LoRa TX | Kombinace zápisu a vysílání | Žádné chyby zápisu při proudových špičkách | — |

- [ ] SD-1 [ ] SD-2 [ ] SD-3 [ ] SD-4 [ ] SD-5 [ ] SD-6 [ ] SD-7

---

## 10. USB (P0)

`USB_DM` PA11 / `USB_DP` PA12, J5 USB-C, ochrana U7.

| ID | Test / OK kritérium | Přežijeme |
|---|---|---|
| USB-1 | CDC se objeví na Linuxu i Windows, product string `boomchecker-node` | — |
| USB-2 | Konzole odpovídá po dobu ≥ 30 min bez zatuhnutí | — |
| USB-3 | Re-enumerace po `task flash` a po resetu S1 | Nutnost znovu `usbipd attach` je známé chování, ne bug |
| USB-4 | Streaming PCM: `record 30` bez ztrát/timeoutů | Občasný retry OK, ztracené bloky ne |
| USB-5 | Chování při napájení z baterie + připojení dat. USB | Bez restartu |
| USB-6 | Kabel v obou orientacích USB-C | Musí fungovat obojí |

- [ ] USB-1 [ ] USB-2 [ ] USB-3 [ ] USB-4 [ ] USB-5 [ ] USB-6

---

## 11. Systémové / integrační testy (P1)

Tady se ukazuje většina problémů první revize — jednotlivé periferie umí
fungovat samostatně a rozbít se, když jedou naráz.

| ID | Test | OK kritérium | Přežijeme |
|---|---|---|---|
| SYS-1 | Nahrávání mikrofonu **současně** s LoRa TX | V audiu není slyšet TX (žádné cvakání v rytmu paketů), nahrávání nespadne | Slabý artefakt zapsat pro v0.2 (stínění/filtrace) |
| SYS-2 | GNSS fix drží **při** LoRa TX | Počet satelitů a C/N0 neklesnou dramaticky (869 MHz vs. 1575 MHz — sledovat harmonické) | Krátkodobý pokles OK |
| SYS-3 | GNSS fix drží při nahrávání a zápisu na SD | Fix se neztrácí | — |
| SYS-4 | Vše naráz (GNSS + LoRa RX + PDM + SD) po 30 min | Bez resetu, bez overrunů, bez ztráty fixu | — |
| SYS-5 | Časové značky z 1PPS nad audio streamem | Vzorky lze svázat s absolutním časem — základ pro TDOA mezi jednotkami | Odložit na FW fázi, ale ověřit, že HW to umožňuje |
| SYS-6 | Výdrž na baterii při typickém profilu | Změřit hodiny provozu — vstup pro dimenzování článku | Cokoli, hlavně změřené |
| SYS-7 | Chování při vybité baterii | Deska se chová definovaně (vypne se, nezatuhne v půlce zápisu na SD) | — |
| SYS-8 | Studený start po odpojení všeho napájení | Naběhne bez ručního zásahu | — |
| SYS-9 | Teplotní kontrola po 30 min plné zátěže | Žádná součástka nepálí (U9, U6, U8 zejména) | — |

- [ ] SYS-1 [ ] SYS-2 [ ] SYS-3 [ ] SYS-4 [ ] SYS-5 [ ] SYS-6 [ ] SYS-7 [ ] SYS-8 [ ] SYS-9

---

## 12. Mechanika a konektory (P2)

- [ ] MECH-1 Montážní otvory M1–M4 (3 mm) odpovídají zamýšlenému uchycení
- [ ] MECH-2 J5 (USB-C) je přístupný a drží při zapojeném kabelu
- [ ] MECH-3 S1 (reset) a S2 (přepínač) jsou obsluhovatelné v krabičce
- [ ] MECH-4 Kabel od baterie na J4 se nikde nemačká, konektor drží
- [ ] MECH-5 GNSS patch anténa J1 má rozumnou orientaci vůči oblohe a není zastíněná plechem/bateriií
- [ ] MECH-6 Anténa LoRa: umístění a kabel neruší GNSS a nemá ostré ohyby
- [ ] MECH-7 FFC k mikrofonům má dost délky a definovaný ohyb (mechanický přenos vibrací do mikrofonů = artefakty)

---

## 13. Nesrovnalosti k ověření proti schématu

Vzniklé porovnáním BOM, schématu a `bom-stm32node.ioc` — vyřešit **před**
tím, než se podle nich bude ladit FW:

- [ ] **HSE krystal**: BOM má Y1 = 24,0 MHz, `.ioc` má `HSE_VALUE=25000000`
  (a `EPOD_VALUE=25000000`). Jedno z toho je špatně — ovlivňuje SYSCLK, USB,
  SAI i baudrate. Viz test MCU-4.
- [ ] **Magnetometr**: `hw/node/v0.1/README.md` uvádí LIS2MDL na SPI, schéma
  i `.ioc` mají `MAG_SCL`/`MAG_SDA` (I2C2). Opravit README.
- [ ] **GNSS**: v schématu jsou vyvedené oba interfacy (I2C1 i UART4).
  Rozhodnout, který je primární pro FW, a ověřit, že Teseo v ROM konfiguraci
  odpovídá na tom zvoleném.
- [ ] **Pinmapa není source of truth**: `.ioc` je dle README draft. Po
  bring-upu vygenerovat/ověřit netlist z Altia a `.ioc` s ním sesouhlasit.
- [ ] **Měření napětí baterie**: v `.ioc` jsou na ADC jen `MIC1_O`/`MIC2_O`.
  Pokud na desce není dělič na VBAT, zapsat jako požadavek pro v0.2 (viz PWR-13).
- [ ] **Anténa pro LoRa**: ověřit, jak je řešený výstup E22 (IPEX/stamp) a
  zajistit, že se nikdy nevysílá bez zátěže.

---

## 14. Co je potřeba dopsat do firmwaru

Dnes v `fw/bom-stm32node` funguje: build/flash (`task build`, `task flash`),
USB CDC konzole (`version`, `stream`, `streamtest`, `radio`, `proto`), PDM
akvizice z D1 + PCM stream na host (`stm32node-cli record`), RadioLib/SX1262
(`radio status|ping|reset`). Pro testy označené 🔧 chybí:

- [ ] GNSS driver: UART4 NMEA parser + I2C přístup, `GPS_RESET`/`GPS_WAKE-UP`,
  EXTI na 1PPS, CLI příkaz `gps status` (fix, satelity, C/N0, TTFF)
- [ ] IMU driver: SPI3 + WHO_AM_I, čtení accel/gyro, INT1/INT2, CLI `imu`
- [ ] Magnetometr: I2C2 + WHO_AM_I, čtení 3 os, CLI `mag`
- [ ] Ovládání load switchů `EN_LORA` (PE7) a `EN_IMU` (PG14) z CLI
- [ ] PDM: `LR_SEL` a rozšíření SAI na D2/D3 (multikanál) + kontrola zarovnání kanálů
- [ ] ADC vzorkování analogových mikrofonů (timer + DMA, PA0/PA1)
- [ ] SD karta: SPI5 driver + FatFs, `SD_DET`, CLI test zápisu a měření propustnosti
- [ ] Diagnostický příkaz `selftest`, který projde všechny periferie a vypíše
  tabulku OK/FAIL — ať se dá tenhle checklist na dalších deskách odjezdit jedním příkazem

---

## 15. Záznam měření

Deska SN: ................ Datum: ................ Testoval: ................
FW commit: ................ Verze HW: `bch-stmnode_v0.1`

| Sekce | Provedeno | OK | FAIL (ID) | Poznámka / issue |
|---|---|---|---|---|
| 0. Před napájením | | | | |
| 1. Napájení | | | | |
| 2. MCU / hodiny / konzole | | | | |
| 3. GNSS | | | | |
| 4. LoRa | | | | |
| 5. IMU | | | | |
| 6. Magnetometr | | | | |
| 7. PDM mikrofony | | | | |
| 8. Analogové mikrofony | | | | |
| 9. microSD | | | | |
| 10. USB | | | | |
| 11. Systémové testy | | | | |
| 12. Mechanika | | | | |

**Verdikt revize:** ☐ deska použitelná pro vývoj FW ☐ použitelná s omezeními
(vypsat) ☐ nutná úprava před dalším osazením
