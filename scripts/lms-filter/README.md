# LMS Filter

Prototype of a fixed-point classic LMS noise canceller for a future STM32H5
port. The PC harness synthesizes a clean sine signal, drone-like reference
noise, a delayed/FIR-filtered reference path, and a noisy microphone signal.
The C code estimates the reference-path noise and returns the LMS error signal.

## Workflow

```bash
task setup
task build
task test
task run
```

Generated outputs are written under `build/` and `python/out/`.

Useful tasks:

- `task config`: generate `build/generated/lms_config.h` from `sdkconfig` or
  `sdkconfig.defaults` using `python -m kconfgen` from `esp-idf-kconfig`.
- `task menuconfig`: edit local LMS Kconfig values through the installed
  `esp-idf-kconfig` menuconfig UI.
- `task kconfig:check`: validate `Kconfig` formatting with
  `python -m kconfcheck`.
- `task run -- --duration-s 5`: run the demo with custom Python arguments.

## Signal Model

The first-phase simulation uses a known reference signal and a noisy desired
signal:

```text
reference drone -> delay + short FIR -> disturbance
clean sine + disturbance -> desired/noisy input
LMS(reference, desired) -> cleaned/error output
```

The reference and desired arrays are converted to signed Q15 PCM before calling
the C library. The clean sine is kept separately only for host-side metrics and
plots; the LMS code never sees it.

## Fixed-Point Model

The public C API uses signed `int16_t` PCM centered around zero. Internally,
FIR products are accumulated in wider integers and shifted back to Q15. The
embedded-facing API is no-heap: callers allocate the state buffer returned by
`lms_state_size()`.

Classic LMS is sensitive to reference amplitude and the learning rate. Keep
`--reference-gain` at `1.0` for the default demo so the simulated acoustic path
stays representable by Q15 coefficients. Use `CONFIG_LMS_ADAPT_SHIFT` to make
the effective update smaller than one Q15 learning-rate LSB.

The effective coefficient update is:

```text
delta_w = CONFIG_LMS_MU_Q15 * error * reference >> (30 + CONFIG_LMS_ADAPT_SHIFT)
```

Large reference amplitudes, many taps, and colored drone harmonics can make a
plain LMS unstable. If the output starts to saturate, reduce
`CONFIG_LMS_MU_Q15` or increase `CONFIG_LMS_ADAPT_SHIFT`.

## Embedded Notes

The functions in `csrc/lms_filter.h` are the intended STM32-facing API:

- Call `lms_state_size()` once for the chosen config.
- Allocate the returned number of bytes statically or from a controlled memory
  region.
- Call `lms_init()` and feed DMA-sized blocks through `lms_process_block()`.
- Keep input PCM signed and centered around zero. If a peripheral path produces
  offset binary samples, subtract the midpoint before calling the LMS code.

`lms_filter_i16()` is a host convenience wrapper for Python and is not the
preferred embedded entry point because it allocates memory internally.
