# TAS: beamforming a lokalizace dronu (2×8 mikrofonů)

Research report k předmětu TAS: akustická lokalizace dronu po detekci pomocí
mikrofonního pole 2×8 MEMS mikrofonů (2× ADAU7118, TDM8) připojeného ke
STM32H563 (`hw/node`, `fw/bom-stm32node`). Hodnocený výstup je report; návrh
PCB a firmware navazují až po zafixování geometrie simulací.

Postup a rozhodnutí: [PLAN.md](PLAN.md) (milníky M1 až M5).

## Struktura

```
PLAN.md          plán po milnících
Taskfile.yml     setup, report:build, report:clean
setup.sh         venv v python/venv + requirements.txt
env.example      HF_TOKEN pro stažení DADS (zkopírovat do .env)
python/          simulace DOA (od M3)
report/          LaTeX report (IEEEtran, čeština)
```

## Použití

```bash
task setup          # Python venv a závislosti
task report:build   # report/main.pdf
task report:clean
```

LaTeX potřebuje `pdflatex` a `bibtex` s českou babel podporou
(`task latex:setup` v kořeni repozitáře). `IEEEtran.cls` je přiložen v `report/`.

## Data

Simulace používá nahrávky dronů z datasetu DADS
(`geronimobasso/drone-audio-detection-samples` na Hugging Face). Stažená data
patří do `data/`, která je v `.gitignore`. Obrázek spektra v reportu je převzatý
z `projects/2026-maxam-fxlms-filter/report/figures/dads_average_spectrum.png`
(skript `projects/2026-maxam-fxlms-filter/drone-spectrum/drone_spectrum.py`).
