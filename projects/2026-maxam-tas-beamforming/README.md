# TAS: beamforming a lokalizace dronu (2×8 mikrofonů)

Research report k předmětu TAS: akustická lokalizace dronu po detekci pomocí
mikrofonního pole 2×8 MEMS mikrofonů (2× ADAU7118, TDM8) připojeného ke
STM32H563 (`hw/node`, `fw/bom-stm32node`). Hodnocený výstup je report; návrh
PCB a firmware navazují až po zafixování geometrie simulací.

Postup a rozhodnutí: [PLAN.md](PLAN.md) (milníky M1 až M5).

## Struktura

```
PLAN.md          plán po milnících
Taskfile.yml     setup (venv v python/venv), sim:*, report:*
requirements.txt pinované závislosti (==)
env.example      HF_TOKEN pro stažení DADS (zkopírovat do .env)
python/          simulace DOA: balíček beamforming, testy, skripty (python/README.md)
data/            klipy DADS stažené `task sim:fetch` (mimo git)
report/          LaTeX report (IEEEtran, čeština)
```

## Použití

```bash
task setup          # python/venv, pinované závislosti, pip install -e python/
task sim:fetch      # 100 dronových klipů z DADS do data/dads (jednou)
task sim:test       # pytest nad simulací (závisí na sim:fetch)
task sim:lint       # ruff a mypy
task sim:fmt        # ruff format a oprava lintu
task sim:demo       # mapy výkonu metod -> report/figures/doa_demo.pdf
task sim:validate   # validační tabulka -> report/generated/validation.tex (~5 min)
task sim:clean      # cache a pomocné výstupy
task report:build   # report/main.pdf
task report:clean
```

Taskfile projektu je samostatný a není zapojený do kořenového `Taskfile.yml`;
spouští se z této složky. `task sim:fetch N=10` stáhne jiný počet klipů.
Model, konvence úhlů, metody a testy popisuje [python/README.md](python/README.md).

LaTeX potřebuje `pdflatex` a `bibtex` s českou babel podporou
(`task latex:setup` v kořeni repozitáře). `IEEEtran.cls` je přiložen v `report/`.

## Data

Simulace používá nahrávky dronů z datasetu DADS
(`geronimobasso/drone-audio-detection-samples` na Hugging Face, mono 16 kHz, klipy
většinou 0,5 s). `task sim:fetch` stáhne jen pevný výběr ze shardu 20 (řádově MB, ne
celý shard), uloží ho do `data/dads/` a zapíše `manifest.json`; `data/` je v `.gitignore`.
Volitelný `HF_TOKEN` patří do `.env`. Obrázek spektra v reportu je převzatý
z `projects/2026-maxam-fxlms-filter/report/figures/dads_average_spectrum.png`
(skript `projects/2026-maxam-fxlms-filter/drone-spectrum/drone_spectrum.py`).
