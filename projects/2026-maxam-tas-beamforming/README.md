# TAS: beamforming a lokalizace dronu (2×8 mikrofonů)

Research report k předmětu TAS: akustická lokalizace dronu po detekci pomocí
mikrofonního pole 2×8 MEMS mikrofonů (2× ADAU7118, TDM8) připojeného ke
STM32H563 (`hw/node`, `fw/bom-stm32node`). Hodnocený výstup je report; návrh
PCB a firmware navazují až po zafixování geometrie simulací.

## Struktura

```
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
task sim:demo       # mapy výkonu metod -> report/figures/doa_demo.pdf a report/generated/demo.tex
task sim:validate   # validační tabulka (30, 10, 0 dB) -> report/generated/validation.tex (~9 min)
task sim:ablate     # ablace voleb zpracování na referenční geometrii (~10 min), řádky do python/results/summary.csv
task sim:sweep:geometry  # screening 21 geometrií a potvrzení nejlepších 3 (stage screening, confirm)
task sim:sweep:snr  # SNR sweep s CRB a citlivost na geometrii z python/results/selected.json
task sim:figures    # grafy a tabulky sekce 4 z python/results/summary.csv (rychlé, bez simulace)
task sim:all        # vše: demo, validate, ablate, sweep, figures
task sim:clean      # cache a pomocné výstupy
task report:build   # report/main.pdf
task report:clean
```

Taskfile projektu je samostatný a není zapojený do kořenového `Taskfile.yml`;
spouští se z této složky. `task sim:fetch N=10` stáhne jiný počet klipů (testy se řídí
počtem v `manifest.json`), argumenty skriptů se předávají za `--`, např.
`task sim:validate --force -- --limit 8 --out /tmp/v.tex` (`--force` obejde kontrolu
zdrojů u `sim:demo` a `sim:validate`).
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
