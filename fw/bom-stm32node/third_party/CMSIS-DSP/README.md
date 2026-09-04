# CMSIS-DSP (vendored copy)

Upstream: <https://github.com/ARM-software/CMSIS-DSP>, tag **v1.15.0**.
License: Apache-2.0, see `LICENSE` (upstream ships no NOTICE file).

Only `Include/`, `PrivateInclude/` and `Source/` are kept — 467 files.

## Provenance, verified

466 of the 467 files are byte-identical to the v1.15.0 release archive
(checked with `cmp` over the whole tree when this README was added).

**One file is locally modified:** `Source/TransformFunctions/arm_mfcc_f32.c`
uses RMS rather than absolute-maximum scaling to condition the input for the
FFT. The change is recorded as `patches/0001-mfcc-rms-normalisation.patch`,
which applies cleanly to the upstream file and reproduces the file in this
tree byte-for-byte. Its origin is undocumented; see the patch header.

That patch is load-bearing: the deployed model (`Core/Inc/mlp_model_data_v6.h`)
was selected against MFCC values this code path produced. Any update — a new
upstream tag, or the planned switch to a CMake `FetchContent` — must either
re-apply it or re-baseline the detector on hardware and record the new
decision values.

The `$Revision`/`$Date` stamps inside individual sources are ARM's per-file
revision markers, ranging from V1.9.0 to V1.11.0 across this tree. They do not
name the release; do not use them to infer the version.

## How it is built

The firmware compiles the per-group combined translation units listed in
`../../CMakeLists.txt` (BasicMath, CommonTables, ComplexMath, FastMath, Matrix,
Statistics, Support, Transform) at `-O2` — see the comment there. Upstream's own
CMake is not part of this copy, so none of upstream's build options apply;
notably `ARM_MATH_LOOPUNROLL` is **not** defined here, while upstream's
`Source/CMakeLists.txt` defaults `LOOPUNROLL` to `ON`.

## Checking this copy

```sh
curl -sSfL -o cmsis.zip https://github.com/ARM-software/CMSIS-DSP/archive/refs/tags/v1.15.0.zip
unzip -q cmsis.zip -d /tmp/cmsis && UP=/tmp/cmsis/CMSIS-DSP-1.15.0
find Include PrivateInclude Source -type f -exec sh -c 'cmp -s "$1" "$0/$1" || echo "DIFF: $1"' "$UP" {} \;
# expected output: exactly one line, DIFF: Source/TransformFunctions/arm_mfcc_f32.c
```
