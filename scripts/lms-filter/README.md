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

## Fixed-Point Model

The public C API uses signed `int16_t` PCM centered around zero. Internally,
FIR products are accumulated in wider integers and shifted back to Q15. The
embedded-facing API is no-heap: callers allocate the state buffer returned by
`lms_state_size()`.

Classic LMS is sensitive to reference amplitude and the learning rate. Keep
`--reference-gain` at `1.0` for the default demo so the simulated acoustic path
stays representable by Q15 coefficients. Use `CONFIG_LMS_ADAPT_SHIFT` to make
the effective update smaller than one Q15 learning-rate LSB.
