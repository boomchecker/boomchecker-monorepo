# Drone Spectrum

This helper estimates the average spectrum of drone recordings from the
Hugging Face dataset `geronimobasso/drone-audio-detection-samples`.

The goal is not drone classification. The plot is meant to support the report
claim that drone sound contains narrow tonal components from motors/propellers
and a broader noise floor. That matters for ANC: feed-forward FxLMS is usually
most practical at lower frequencies, where wavelengths are longer and phase
errors are less punishing. If a large part of the drone energy sits at high
frequencies or is very broadband, active cancellation becomes harder and must be
combined with filtering, shielding, microphone placement, or a downstream
detector that can tolerate residual noise.

## Output

The script:

- loads only drone-labelled samples (`label == 1`),
- computes a Welch power spectral density for each recording,
- gives each recording equal weight,
- plots the mean PSD and a percentile band,
- shades the low-frequency ANC region.

The mean is computed in linear power units and converted to dB for plotting.
The percentile band is computed across per-recording dB spectra.

## Setup

From `scripts/lms-filter`:

```bash
task setup
```

The dataset may require Hugging Face authentication:

```bash
huggingface-cli login
```

Alternatively, copy the example env file and set `HF_TOKEN`:

```bash
cp drone-spectrum/env.example drone-spectrum/.env
```

The script loads `drone-spectrum/.env` automatically. The local `.env` file is
ignored by git.

## Run

From `scripts/lms-filter`:

```bash
task drone-spectrum
```

For a quicker smoke run:

```bash
task drone-spectrum -- --start-shard 3 --end-shard 4 --inspect-labels
```

Keep the default streaming mode for this dataset. Do not use
`--no-streaming --split 'train[16729:16730]'` as a smoke test: Hugging Face
Datasets can still download full parquet shards before selecting the slice.
For DADS those shards are large.

Do not use `--skip-rows 16729` for normal runs either. Streaming still has to
read the earlier parquet shards to skip their rows. Use `--start-shard` instead,
which restricts the dataset files before iteration starts.

Find the first drone shard before plotting:

```bash
task drone-spectrum -- --start-shard 0 --end-shard 1 --inspect-labels
task drone-spectrum -- --start-shard 1 --end-shard 2 --inspect-labels
task drone-spectrum -- --start-shard 2 --end-shard 3 --inspect-labels
task drone-spectrum -- --start-shard 3 --end-shard 4 --inspect-labels
```

Once a shard reports label `1`, plot from that shard:

```bash
task drone-spectrum -- --start-shard N --end-shard N+1 --max-samples 50 --output-dir /tmp/drone-spectrum
```

Useful options:

```bash
./python/venv/bin/python drone-spectrum/drone_spectrum.py \
  --dataset geronimobasso/drone-audio-detection-samples \
  --split train \
  --start-shard N \
  --end-shard N+4 \
  --max-samples 500 \
  --max-rows-scanned 0 \
  --max-duration-s 5 \
  --nperseg 2048 \
  --anc-max-hz 1000 \
  --output-dir drone-spectrum/out
```

The script writes:

- `drone_average_spectrum.png`
- `drone_average_spectrum_metrics.json`

If the stream scans many rows without finding drone-labelled examples, pass
`--progress-every 100` while debugging. Prefer moving `--start-shard` forward
over increasing `--skip-rows`.

`--no-streaming` is guarded by `--allow-non-streaming-download` because it may
download multi-GB shards. Use it only when you deliberately want a local cache of
the dataset files.

If you accidentally started a non-streaming run, stop it with `Ctrl+C`. Hugging
Face usually stores partial downloads under `~/.cache/huggingface/`; remove only
the DADS cache entries if you need to reclaim disk space.

## Interpretation

Use the plot as a diagnostic:

- tonal peaks indicate motor/propeller harmonics that an adaptive feed-forward
  filter may learn from motor-correlated references,
- elevated broadband energy indicates turbulent or environmental components
  that are less predictable from a motor reference,
- energy above the shaded low-frequency region is harder for practical ANC and
  may remain for the detector.
