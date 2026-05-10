# FXLMS ANC Demo

Fixed-point filtered-x LMS active-noise-control demo for a future STM32H5 port.
The PC harness synthesizes a drone-like reference signal `x[n]` and a primary
path `P(z)` to create drone noise at the microphone. Optionally, the harness
can mix a wanted WAV signal into the microphone path so the C core suppresses
the drone noise while leaving unrelated audio in the residual.

## Workflow

```bash
task setup
task build
task test
task run
```

Generated outputs are written under `build/` and `python/out/`.

Useful tasks:

- `task config`: generate `build/generated/fxlms_config.h` from `sdkconfig` or
  `sdkconfig.defaults` using `python -m kconfgen` from `esp-idf-kconfig`.
- `task menuconfig`: edit local FXLMS Kconfig values through the installed
  `esp-idf-kconfig` menuconfig UI.
- `task kconfig:check`: validate `Kconfig` formatting with
  `python -m kconfcheck`.
- `task run -- --duration-s 5`: run the demo with custom Python arguments.
- `task run -- --wanted-wav audio/tank-moving.wav --wanted-gain 0.35`: mix a
  wanted WAV signal into the primary microphone signal.

## Signal Model

```text
Python simulation:
  x[n] -> P(z) -> drone_primary[n]
  wanted_wav[n] ----------+
                          v
  drone_primary[n] + wanted[n] -> d[n]

C core:
  x[n] -> G(z) -> y[n] -> C(z) -> secondary[n]
  e[n] = d[n] - secondary[n]
  x_filtered[n] = C_hat(z) * x[n]
  G(z) update uses e[n] and x_filtered[n]
```

Mapping:

- `P(z)`: Python primary path simulation, implemented as delay plus short FIR.
- `wanted_wav[n]`: optional useful audio loaded with `--wanted-wav`; it is
  converted to mono, resampled to `--fs`, cropped or zero-padded to
  `--duration-s`, normalized, then scaled by `--wanted-gain`.
- `G(z)`: adaptive FIR controller in C.
- `C(z)`: fixed demo secondary path in C, applied only to `y[n]`.
- `C_hat(z)`: fixed secondary-path estimate in C, used only for the filtered-x
  adaptation branch.

The main block API takes `reference_x[]` and `primary_d[]`, then returns
`error_e[]`, `controller_y[]`, and `secondary_output[]`.

When a wanted WAV is used, `error.wav` contains the wanted signal plus the
remaining drone residual. The demo also writes `wanted.wav`, `drone_primary.wav`,
and `noise_residual.wav`, where `noise_residual = error - wanted`. The
`drone_attenuation_tail_db` metric is computed from that residual so it measures
drone suppression instead of rewarding removal of the useful WAV signal.

All FIRs in this demo are documented and implemented newest-sample-first: tap
`i` multiplies sample `n - i`. This convention is shared by the adaptive
controller `G(z)`, secondary path `C(z)`, secondary-path estimate `C_hat(z)`,
and Python primary path `P(z)`.

## Fixed-Point Model

The public C API uses signed `int16_t` PCM centered around zero. Internally,
FIR products are accumulated in wider integers and shifted back to Q15. The
embedded-facing API is no-heap: callers allocate the state buffer returned by
`fxlms_state_size()`.

The controller update is:

```text
delta_g = CONFIG_FXLMS_MU_Q15 * error * filtered_x
          >> (30 + CONFIG_FXLMS_ADAPT_SHIFT)
```

If the output saturates, reduce `CONFIG_FXLMS_MU_Q15` or increase
`CONFIG_FXLMS_ADAPT_SHIFT`.

## Embedded Notes

The functions in `csrc/lms_filter.h` are the intended STM32-facing API:

- Call `fxlms_state_size()` once for the chosen config.
- Allocate the returned number of bytes statically or from a controlled memory
  region.
- Call `fxlms_init()` and feed DMA-sized blocks through `fxlms_process_block()`.
- Keep input PCM signed and centered around zero. If a peripheral path produces
  offset binary samples, subtract the midpoint before calling the FXLMS code.

`fxlms_filter_i16()` is a host convenience wrapper for Python and is not the
preferred embedded entry point because it allocates memory internally.
