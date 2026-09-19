# boomdetect

Acoustic detection shared between the firmware and a host test build: MFCC front
end, feature aggregation, and the classifier registry.

It exists for one reason. The detector used to live in
`fw/bom-stm32node/Core/Src`, which is CubeMX-owned and cross-compiles only for
the Cortex-M33 — so nothing in it could be run, tested or compared against the
Python the models were trained with. The deployed model ships ~1700 trained
floats that no test in this repository could check. Moving the arithmetic into a
package that also builds natively is what makes that checkable.

## Two ways this builds

| | how | what you get |
|---|---|---|
| cross | `add_subdirectory()` from `fw/bom-stm32node` | the library only, at `-O2`, no tests |
| host | `cmake --preset Debug` here | the same library plus tests, ASan+UBSan on |

The switch is `CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR`, the same
mechanism `fw/common/boomlink` uses.

```sh
task build   # configure + build (host)
task test    # + ctest
```

## CMSIS-DSP is fetched, not vendored

`FetchContent` pinned to **v1.15.0**. It lives here rather than in the firmware
because the host build needs it too — without a host MFCC there is no parity
suite, and without a parity suite this package has no reason to exist.

Three things about that are load-bearing, and all three are silent if you get
them wrong:

- **`LOOPUNROLL` is forced OFF.** Upstream defaults it on. The hand-rolled build
  this replaced never defined `ARM_MATH_LOOPUNROLL`, and unrolling changes
  accumulation order in `arm_dot_prod_f32`/`arm_rms_f32`, which moves MFCC
  coefficients in the last places. The deployed model was selected against the
  un-unrolled numbers.
- **`-O2` is re-applied for the cross build.** The firmware compiled its DSP at
  `-O2` even in Debug because the MFCC shares a 21.33 ms budget with the PDM
  conversion, and the Debug toolchain is `-O0 -g3`. That was a directory-scoped
  source property in the firmware, which cannot reach a target defined here.
- **The Core headers come from the firmware.** CMSIS-DSP includes
  `cmsis_compiler.h`, which ships with the device support package, not with DSP.
  The cross build takes it from `BOOMDETECT_CMSIS_CORE_DIR`, injected by the
  consumer; the host build sets `HOST=ON`, which makes CMSIS define
  `__GNUC_PYTHON__` and skip that include entirely.

## The one deviation from upstream

`src/boomdetect_mfcc_f32.c` is our own copy of `arm_mfcc_f32()` that conditions
the FFT input by RMS instead of by absolute maximum.

The vendored CMSIS-DSP that preceded this package carried that edit with nothing
recording it — 466 of its 467 files were byte-identical to v1.15.0, and that one
was not. Scaling in and back out cancels in exact arithmetic, so it is not a
change of definition, but it changes float32 rounding and it changes which
frames are considered too quiet to normalise. The deployed model was selected
against the values it produces.

Carrying it as our own translation unit rather than patching the fetched tree
keeps upstream pristine, avoids a `PATCH_COMMAND` that re-runs and fails on the
second configure, and puts the licence attribution somewhere a reader will
actually find it.

## Three things a first-time reader should know

**Adding this package makes the FIRMWARE build need the network.** CMSIS-DSP is
fetched at configure time rather than vendored, which is what removed 11 MB and
470 files from the tree, and the cost is that `cmake --preset Debug` in
`fw/bom-stm32node` now clones from github.com. Both CI workflows cache the
fetched source; an offline build needs `FETCHCONTENT_SOURCE_DIR_CMSISDSP`
pointing at a copy.

**Host and target do not produce identical bits.** Same C, same input, same
window and frame counts, but the Cortex-M33 build takes CMSIS-DSP's
architecture-specific paths at `-O2` while the host takes the generic ones, and
float32 addition is not associative. Measured on the fixture: MFCC coefficients
differ by at most 1.8e-4 relative, decisions by at most 1.1e-6. That is why
there are two fixtures rather than one, and it is the reason a future
C-versus-Python parity suite has to state a tolerance instead of comparing bits.
The scale this measurement suggests is 1e-6 on the decision.

**"The same decisions the board would make" holds only for audio that went
through the board's front end.** `pdm_pcm.c`'s CIC and FIR stay in the firmware
and are not part of this package, and `boomdetect_push()`'s decimation is plain
sample dropping with no anti-alias filter of its own - the 8 kHz FIR upstream of
it is what makes that legitimate on the board. A WAV captured off the `stream`
command at 48 kHz is therefore a valid input; a WAV from anywhere else answers a
different question. Prefer `stream`-captured audio for any parity corpus.

## Regression fixture

The generator lives in `src/boomdetect_selftest.c` and is driven from two
places: the board's `detselftest` command, and `boomdetect_selftest_tool` here.
It replaces the live microphone, which never repeats, with an integer LCG, and
prints every stage as raw IEEE-754 bit patterns.

| fixture | produced by | checked by |
|---|---|---|
| `tests/vectors/selftest_expected.txt` | the board, before the move into this package | `tests/vectors/check_selftest.py`, needs hardware |
| `tests/vectors/selftest_host.txt` | `boomdetect_selftest_tool` on x86-64 | `ctest`, needs nothing |

The two do not match, for the reason above; each is compared only against its
own side.

A mismatch on either means one of: the RMS deviation above was lost,
`LOOPUNROLL` got switched on, `-O2` did not survive, or the change moved the
arithmetic. All four are findings. Two of them are now checked without a board:
`LOOPUNROLL` fails the configure, and the `Shipped` preset builds at `-O2`
without sanitizers so CI can diff it against the `Debug` output.
