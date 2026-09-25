# BEC 2026 — Artillery Launch Detection (paper 53)

**Authors:** Jakub Zelinjak, Martin Maxa
**Status:** published — camera-ready submitted 2026-08-31
**Paper:** `BEC2026_paper_53.pdf`, sources in `article/`

## Depends on the Zelinjak project

This paper is **not** self-contained. It builds on
`projects/2026-zelinjak-artillery-detection/`, which owns:

- `datasets/recordings/` — the canonical WAV library, source of all negatives and of
  the canonical test split.
- `ml/` — `model.build_baseline_cnn` (the architecture), `utils` (`extract_window`,
  `find_peak`, `load_signal`), `prepare_features`, `convert_model.write_c_header`.
- `generated/features_*` — the feature caches the training scripts read.
- `firmware/esp32s3/` — the deployment target for `esp32:model`.
- `venv/` — the Python environment the retraining tasks run under.

Scripts resolve that dependency by walking up to the `.git` marker and then naming the
project explicitly, so moving this directory again does not break them:

```python
REPO_ROOT = next(p for p in Path(__file__).resolve().parents if (p / ".git").exists())
PROJECT_ROOT = REPO_ROOT / "projects" / "2026-zelinjak-artillery-detection"
```

The Taskfile does the same through the `PROJECT_DIR` and `PAPER_FROM_PROJECT` vars:
the retraining and dataset tasks run with `dir: {{.PROJECT_DIR}}` and reach back into
this directory.

**If the Zelinjak project is ever renamed or moved, update `PROJECT_ROOT` in the eight
scripts and `PROJECT_DIR` in `Taskfile.yml`.**

## Contents

```
article/       LaTeX sources, figures, generated tables, review notes
retraining/    three controlled retraining arms + ESP32 validation
new-dataset/   4-microphone recording campaign (50 shots, zipped)
scripts/       figure and table generators (+ vendored PlotNeuralNet)
templates/     IEEE conference template
```

## Build

```bash
task papers:bec26:build            # compile the article (needs IEEEtran)
task papers:bec26:setup            # install texlive-publishers + texlive-science
task papers:bec26:retrain:train:all DATASET=new
task papers:bec26:dataset-new:build
```

Table `article/tables/table_ablation.tex` and `table_robustness.tex` are
**auto-generated** by `scripts/generate_results_tables.py` from the retraining results —
do not edit them by hand.

## Note on `new-dataset/manifest_new.csv`

`audio_path` values are resolved by `ml/prepare_features.py` against
`datasets/recordings/` in the project, *not* against the manifest's own location. They
were rewritten when this directory moved out of the project. Regenerating the manifest
with `task papers:bec26:dataset-new:build` recomputes them automatically.
