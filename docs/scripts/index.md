# Scripts

`scripts/` holds DSP and analysis tools used for research and experiments. They run in
the **`sw-devcontainer`**; most have a `task setup` that creates a Python virtualenv.

| Tool | Language | Purpose |
| ---- | -------- | ------- |
| **median-filter** | C core + Python harness | Median-based impulse [peak detector](peak-detector.md). C API: [reference](../peak_detector/links.md). |
| **lms-filter** | C core + Python | Fixed-point [FxLMS](lms-filter.md) active-noise-control demo (SISO + multichannel). |
| **tdoa_estimation** | Python | [TDOA / Angle-of-Arrival](tdoa.md) analysis (cross-correlation, parabolic interpolation, phase delay). |

## Running a script

```bash
cd scripts/median-filter
task setup          # create venv + install deps (ffmpeg etc.)
task -l             # list the script's tasks
task benchmark      # example: build C core + benchmark
```

The peak detector's C core (`scripts/median-filter/csrc/peak_detector.c`) is documented
two ways: a written [algorithm overview](peak-detector.md) and an auto-generated
[C API reference](../peak_detector/links.md) (produced by Doxygen via mkdoxy).
