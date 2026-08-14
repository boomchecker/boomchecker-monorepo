# BoomProtocol / Nanopb foundation

Shared Protocol Buffers schema and Nanopb code generation for BoomLink
(`docs/firmware/bom-stm32node/boomlink.md`). This package owns the `.proto`
source of truth, the generated-code pipeline, and the host-native test suite
that cross-checks Nanopb (the embedded C runtime) against Python protobuf on
the same wire bytes. It does not implement radio transport, link framing, or
any application message beyond `SystemMessage`'s `Ping`/`Pong` pair - see
"Scope" in the roadmap's PR 2 entry.

## Layout

```
proto/            .proto source files (source of truth for the wire format)
nanopb/           boomlink.options - bounded-field sizes for Nanopb
boomlink_codec.h/.c   Envelope encode/decode, target-agnostic C
tests/            host-native C test tool + pytest suite + golden vectors
CMakeLists.txt    generation + library + (standalone only) test targets
```

`*.pb.c`/`*.pb.h` and `*_pb2.py` are never committed - they are generated at
build/test time (see CMakeLists.txt) and always reproducible from the
`.proto` + `.options` files, which are the only checked-in source of truth.

## Building and testing

This directory is a CMake project that builds two different ways:

- **Standalone** (this README's "building and testing" - what `task test`
  does): host-native compiler, builds `boomlink_codec_tool` and runs the
  full CTest suite (a C self-test plus the Python interop/compatibility
  tests).
- **As a subdirectory** of `fw/bom-stm32node`'s ARM cross build: only the
  `boomlink_protocol` static library is built (the generated codec, linked
  into the firmware). No tests - a cross-compiled host tool makes no sense,
  and pytest cannot run on an STM32.

```sh
task setup   # create .venv with protobuf/grpcio-tools/pytest
task test    # configure, build, run the full suite via CTest
```

Requires `cmake`, `ninja`, and a host C compiler in addition to the Python
venv `task setup` creates - not additionally listed here since they are
already required to work in this monorepo at all (see `fw/bom-stm32node`'s
own `Taskfile.yml`/CI for the exact toolchain versions in use).

## Protocol compatibility rules (boomlink.md section 15.1)

Every PR that changes anything under `proto/` or `nanopb/` must keep these
holding, and `tests/test_compatibility.py` is what enforces them:

- **Never reuse a removed Protobuf field number.** Mark it `reserved` in the
  `.proto` file instead. (Numbers that were never assigned in the first
  place - e.g. `Envelope.payload`'s 10-13, reserved by the roadmap for PR 4's
  message groups - are not "removed" and are left as plain comments, not
  `reserved`, since nothing needs protecting yet.)
- **Bounded fields have a fixed maximum size** (`nanopb/boomlink.options`),
  and exceeding it is a decode failure, not a buffer overflow, on the Nanopb
  side even when the peer's encoder (e.g. Python protobuf, which has no
  client-side bound) is happy to produce something larger.
- **Malformed and truncated input fail closed** on both sides - verified
  against the Nanopb side directly via the C test harness.
- **Unknown fields are forward-compatible**: a field number neither side's
  current schema assigns must be silently skipped, not treated as an error,
  by both Python and Nanopb.
- **Old golden vectors keep decoding.** `tests/vectors/*.bin` are fixed,
  committed encodings the *current* schema must always still be able to
  decode to the values in `test_compatibility.py`'s `GOLDEN_VECTORS`.
  **Never regenerate or overwrite an existing vector file** - that would
  delete the regression test it exists to provide. Add new vectors (via
  `generate_vectors.py`, which already skips files that exist) when a new
  message shape needs one; never touch old ones.

## Adding a new message group (PR 4+)

1. Add the new `.proto` file under `proto/`, plus a `common.proto` entry if
   more than one group needs the same type.
2. Add its `oneof` branch to `Envelope` in `envelope.proto`, using the field
   number the roadmap already assigned in
   [boomlink.md section 7](../../../docs/firmware/bom-stm32node/boomlink.md#7-boomprotocol-message-model).
3. Add any variable-size field's bound to `nanopb/boomlink.options`.
4. Add the new proto's name to the `foreach` loops in `CMakeLists.txt`
   (`BOOMLINK_PROTO_NAMES` equivalent) - both the Nanopb and Python
   generation steps.
5. Add golden vectors and tests per the rules above.
