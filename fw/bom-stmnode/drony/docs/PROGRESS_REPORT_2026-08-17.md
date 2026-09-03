# Progress report — GPS bring-up + mikrofony na dvou deskách (session 17. 8. 2026)

Autoři: Kamil Herman + Claude (AI asistent). Navazuje na PROGRESS_REPORT_2026-08.md
(sessiony 6.–10. 8.: on-device detekce, STKOF fix, modely v1–v6).

## 1. Shrnutí

**GNSS modul Teseo-LIV3R na desce bch-stmnode je oživený end-to-end**: nové firmware
příkazy `gps`/`gpstx`/`gpsrst`, host nástroj `tools/gps_log.py`, a hlavně **první fix
u okna — 50.10232 N, 14.39281 E, 5 satelitů, HDOP 2.7, rozptyl 7,2 m RMS**. Mikrofony:
na desce A po přepojení fungují (ráno dávaly samé nuly — vada zapojení); deska B má
**jiný typ mikrofonů (Infineon IM67D130A)**, které se chovají intermitentně — jejich
oživení je plán na příští session (viz §6). Na desce B není osazený Teseo modul, GPS
tam tedy z principu nejde otestovat.

## 2. Ranní diagnóza mikrofonů (deska A)

- `record` dával 100% nulové WAV; **testovací tón (`--test-tone`) přitom čistý** →
  DSP→USB→host řetězec zdravý, PDM data nechodila do PE6 (firmware čte jen PDM_D1=PE6,
  hodiny PDM_CLK=PE2; PE4=D2 se nečte).
- Na desce navíc překvapivě běžel čistý main firmware bez `detect` — přeflashováno
  drony buildem, on-device `detect --dbg` potvrdil rms=0 na všech 155 rámcích.
- Po Kamilově přepojení drátů: `record` OK (ambient RMS 0,013; spektrum <8 kHz dle FIR),
  `detect` LVL 0,001–0,005 v tichu, 0 falešných oken. Deska A je zdravá.

## 3. GPS bring-up — co vzniklo

**Firmware (`Core/Src/gps.c` + `gps.h`, úpravy `cli.c`, `CMakeLists.txt`):**
- UART4 (PB8 = RX z modulu, PB9 = TX; jména GPS_TX/GPS_RX v main.h jsou z pohledu
  modulu!) — RX přes přerušení do 1KB ringu; `UART4_IRQHandler` definovaný v gps.c
  (slabé vektory, CubeMX soubory nedotčené; NVIC priorita 7 pod USB=0 a mic DMA=5).
- `gps <sec> [baud]` — streamuje syrové NMEA věty na USB konzoli, trailer
  `GPSEND lines= bytes= ne= fe= ore= pe= overrun= err=` s čítači chyb UART per typ.
- `gpstx <věta> [baud]` — pošle NMEA/$PSTM příkaz (checksum dopočítá FW). RX zůstává
  trvale aktivní, takže odpověď modulu mezi příkazy se neztratí (vyzvedne ji další `gps`).
- `gpsrst` — 100ms pulz SYS_RSTn (PB5, aktivní low) pro tvrdý restart modulu.
- CLI kapacita zvednuta kvůli novým příkazům: `maxBindingCount` 8→10,
  `CLI_STATIC_BYTES` 2048→3072 (aktuálně 9/10 bindingů včetně `micdiag`, viz §6).
- `spec.py` + `PROTOCOL.md` regenerovány, `test_protocol_docs.py` prochází.

**Host (`tools/gps_log.py`, běží v drony venv — pyserial doinstalován):**
- `scan` — autodetekce baudu podle podílu validních NMEA checksumů,
- `capture <sec>` — log do `data/gps_logs/*.nmea` + živý status (fix/satelity/HDOP/poloha)
  a tabulka kvality satelitů každých 12 s,
- `ver`, `send <věta>`, `parse <soubor>` (offline vyhodnocení uloženého logu).
- První NMEA parser v repu (čistě stdlib): GGA/RMC/GSV/GSA, checksum validace,
  per-satelit C/N0 + elevace/azimut, SBAS mapování PRN+87, statistika rozptylu polohy.

## 4. GPS — výsledky měření

- **Baud: 9600** (ROM default), 98–100 % validních checksumů. Teseo-LIV3R je ROM
  verze — konfigurace nepřežije power-cycle bez VBAT, proto se modul nepřekonfigurovává
  a MCU se přizpůsobuje.
- **TX cesta ověřena** příkazem `$PSTMSRR`: modul prokazatelně restartoval (UTC spadl
  na default 23:59:4x, pak se znovu synchronizoval). Na `$PSTMGETSWVER` (žádné ID)
  tahle ROM neodpovídá — verzi FW modulu nezjistíme, nepodstatné.
- **Fix u okna (13:24 UTC+2, ~5 min po resetu):** quality 1, 3D, 5 použitých satelitů
  (GLONASS 75/76/88 + GPS 3/7, C/N0 27–36 dB-Hz), 14 v dohledu, HDOP 2.7, PDOP 4.6,
  50.10232 N 14.39281 E, výška ~270 m, UTC přesný.
- **Přesnost:** rozptyl polohy jen **7,2 m RMS (max 12,3 m)** — pozorovaná chyba
  40–50 m je tedy **systematický bias z jednostranné geometrie** (použité satelity
  v azimutech 58–200° = jen výhled z okna) + multipath přes sklo, ne šum. Venku
  s volným výhledem čekáme 2–5 m (datasheet 1,5 m CEP). Průměrování pomáhá na šum,
  bias neodstraní. EGNOS 123 modul trackuje (33 dB-Hz), korekce zatím neaplikuje
  (GGA quality zůstává 1); experiment s `$PSTMSBASONOFF` je možný follow-up.
- **Ze stolu fix nejde**: 2 slabé satelity (EGNOS 32, GLONASS 24 dB-Hz), zbytek
  "searching". Anténa potřebuje minimálně parapet, ideálně venkovní montáž.
- **Kuriozita/kvirk: NE (noise) flag na ~40 % přijatých bajtů** (fe/ore/pe ≈ 0, data
  přesto z 98–100 % validní — 3vzorkové majoritní hlasování byte zachrání). Podezření
  na hraniční úrovně/kvalitu hrany na PB8 — až bude multimetr: změřit klidovou úroveň
  TX modulu (3,3 V vs ~1,8 V ⇒ VCC_IO mismatch).

## 5. Deska B ("opravená")

- Obě desky mají **stejné USB sériové číslo** ("000000000001") → hlásí se na stejném
  COM portu a nejde je rozlišit z PC; rozlišovák: `gps 2` (deska B mlčí — **Teseo na ní
  není osazený**; `gpsrst` vznikl původně jako fallback při této diagnóze).
- Přeflashována dnešním buildem (detect + gps + gpstx + gpsrst).
- Mikrofony (Infineon, viz §6): ve 14:02 nahrávka OK (RMS 0,018, spektrum zdravé,
  detect čistý), od 14:04 **spontánně samé nuly** na record i detect bez jakékoli
  SW změny mezi tím. Uložené nahrávky: `fw/bom-stmnode/drony/data/recordings/rec-20260817-140158.wav` (funkční),
  `-140437/-140521/-140743.wav` (nulové).

## 6. Nové mikrofony: Infineon IM67D130A (plán na příště)

Deska B má jiný druh PDM mikrofonů než deska A: **IM67D130A** (AEC-Q103 automotive
XENSIV MEMS; datasheet v `drony/Infineon_IM67D130A_DataSheet_v01_00_EN.pdf`).

Klíčové parametry: SNR 67 dB(A), AOP 130 dBSPL, citlivost −36 dBFS @94 dBSPL,
THD <1 % do 128 dBSPL, dolní mez 28 Hz, omni. VDD 1,62–3,6 V (3V3 rail OK),
100 nF blokovací kondenzátor co nejblíž VDD, volitelně ~100 Ω sériově v DATA proti
zákmitům. Piny: DATA / VDD / CLOCK / **LR select** / GND.

**Kompatibilita s naším firmwarem: dobrá.** Power mode se volí frekvencí hodin a naše
SAI CK1 = 3,072 MHz padne přesně do High-Performance módu (2,9–3,3 MHz). Start-up
≤20 ms po přivedení VDD+CLK (hodiny běží jen během stream/detect → první půl-buffer
je settling, jako dosud).

**Kritický detail — pin LR určuje fázi hodin:** LR=GND ⇒ mikrofon vysílá data po
NÁBĚŽNÉ hraně (platná během high fáze), LR=VDD ⇒ opačně; v druhé půlperiodě je DATA
high-Z. Naše SAI má `ClockStrobing=FALLINGEDGE` (sai.c:47) — konfigurace, se kterou
fungují mikrofony na desce A. **Plovoucí LR = nedefinovaná fáze ⇒ SAI může číst
high-Z úsek = přesně symptom "chvíli funguje, pak samé nuly".** To je hlavní podezřelý
pro intermitentní chování z §5.

**Dodatek (14:30, první pokus o oživení s 1 mikrofonem):** nahrávka opět nulová.
Vyloučili jsme fázi hodin (experiment `ClockStrobing=RISINGEDGE` — také nuly;
vráceno na FALLINGEDGE, což je správně pro LR=GND). Přidán diagnostický příkaz
**`micdiag`** (`mic.c: mic_diag_run`): za běhu CK1 vzorkuje PE6/D1 a PE4/D2 jako
GPIO a počítá přechody; s vypnutými hodinami udělá pull-up/pull-down test. Výsledek:

```
MICDIAG PE6/D1 clk=on toggles=0 hi=0/200000
MICDIAG PE4/D2 clk=on toggles=0 hi=0/200000
MICDIAG PE6/D1 clk=off pu=1 pd=0 (floating/tri-state)
MICDIAG PE4/D2 clk=off pu=1 pd=0 (floating/tri-state)
```

⇒ mikrofon nevysílá **nikam** (ani D1, ani D2) a obě linky jsou plovoucí (žádný
zkrat). IM67D130A drží DATA v high-Z, když nemá hodiny (<250 kHz ⇒ standby) nebo
napájení — **první kontrola příště tedy: multimetrem VDD přímo na mikrofonu,
kontinuita PE2→CLOCK a DATA→PE6, LR→GND.** SW cesta je prokazatelně zdravá
(deska A funguje beze změny firmwaru).

Postup příště:
1. Změřit: VDD na pouzdru mikrofonu (3,3 V), kontinuitu PE2→CLOCK, DATA→PE6,
   LR→GND; 100 nF u VDD, krátké vodiče.
2. `micdiag` — musí ukázat toggles v řádu tisíců na PE6; pak `streamtest` →
   `record 5` → `detect` LVL; očekávaný ambient RMS ≈ 0,005 při +24 dB gainu.
3. Kdyby data vypadala bitově posunutá / poloviční amplituda: ladit SAI_PDMDLY,
   případně vyzkoušet LR=VDD; při tichu osciloskop na CLK/DATA.
4. **Výběr kanálu L/R je softwarový:** firmware čte jen „mic A" páru na D1 —
   `mic.c:104` předává `PDM_SLOT_MASK_A = 0xF807` (každé 16bit slovo SAI nese
   8 bitů od každého mikrofonu páru; maska B = `0x07F8` je druhý kanál,
   `pdm_pcm.h:38-39`). Mikrofon se špatnou úrovní LR skončí v B polovině a
   firmware vrací nuly, i když mic vysílá — pak stačí přepnout na
   `PDM_SLOT_MASK_B` nebo přehodit LR. `micdiag` je vůči fázi agnostický
   (vzorkuje surový pin), takže dnešní závěr „mic nevysílá vůbec" platí
   nezávisle na L/R.

## 7. Stav repa

Necommitnuto (větev `Kamil_connection`): `Core/Src/gps.c`, `Core/Inc/gps.h`,
`Core/Src/cli.c`, `CMakeLists.txt`, `tools/gps_log.py`, `spec.py` + `PROTOCOL.md`,
tento report. Build: RAM 15,9 %, FLASH 6,6 %. Obě desky flashnuté dnešním buildem.

## 8. Otevřené body

1. **Bring-up IM67D130A na desce B** (postup v §6) — příští session.
2. Commit dnešních změn.
3. GPS: venkovní test přesnosti; experiment SBAS (`$PSTMSBASONOFF`); změřit úrovně
   na PB8 (NE flagy).
4. Trvá z minula: reálné dron nahrávky pro validaci v5/v5e, K-of-N rozhodování.
