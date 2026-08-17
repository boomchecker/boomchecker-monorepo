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
nanopb/           <name>.options - bounded-field sizes for Nanopb, one file
                  per .proto that needs one (auto-discovered by name, see
                  CMakeLists.txt's comment on why - avoids a spurious
                  "did not match any fields" warning on every proto that
                  doesn't need one)
boomlink_codec.h/.c   Envelope encode/decode + boomlink_envelope_init(),
                      target-agnostic C
tests/            host-native C CLI tool (codec_tool.c) + pytest suite +
                  golden vectors (vectors_spec.py is their single source of
                  truth, sha256-pinned - see "Protocol compatibility rules")
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
  into the firmware and actually exercised there by the `proto` CLI command
  in `Core/Src/cli.c` - see its comment for why that matters). No tests - a
  cross-compiled host tool makes no sense, and pytest cannot run on an
  STM32.

```sh
task setup      # create .venv with protobuf/grpcio-tools/pytest/ruff
task test       # configure, build, run the full suite via CTest
task generate   # add any new golden vectors (see generate_vectors.py)
task lint       # ruff over tests/
```

Requires `cmake`, `ninja`, and a host C compiler in addition to the Python
venv `task setup` creates - not additionally listed here since they are
already required to work in this monorepo at all (see `fw/bom-stm32node`'s
own `Taskfile.yml`/CI for the exact toolchain versions in use).

The Debug preset builds with `-DBOOMLINK_SANITIZE=ON` by default
(AddressSanitizer + UndefinedBehaviorSanitizer) - this package's entire
reason to be host-testable is exercising a wire-format parser on
hostile/malformed bytes, and a memory-safety bug there is exactly what a
sanitizer catches that a plain pass/fail exit code does not. Pass
`-DBOOMLINK_SANITIZE=OFF` to `cmake --preset Debug` to disable it.

## Protocol compatibility rules (boomlink.md section 15.1)

Every PR that changes anything under `proto/` or `nanopb/` must keep these
holding, and `tests/test_compatibility.py` is what enforces them:

- **Never reuse a removed Protobuf field number.** Mark it `reserved` in the
  `.proto` file instead. (Numbers that were never assigned in the first
  place - e.g. `Envelope.payload`'s 10-13, reserved by the roadmap for PR 4's
  message groups - are not "removed" and are left as plain comments, not
  `reserved`, since nothing needs protecting yet.)
- **Bounded fields have a fixed maximum size** (`nanopb/<name>.options`),
  and exceeding it is a decode failure, not a buffer overflow, on the Nanopb
  side even when the peer's encoder (e.g. Python protobuf, which has no
  client-side bound) is happy to produce something larger. Read the real
  compiled bound from `codec_tool limits` (or the `codec_tool_limits` pytest
  fixture) rather than hardcoding a copy of the number - a hardcoded copy
  sizing a C stack buffer is exactly how this package once shipped a real
  stack overflow when the bound changed and one copy wasn't updated.
- **Malformed and truncated input fail closed** on both sides - verified
  against the Nanopb side directly via the C test harness.
- **Unknown fields are forward-compatible**: a field number neither side's
  current schema assigns must be silently skipped, not treated as an error,
  by both Python and Nanopb.
- **Old golden vectors keep decoding, and never change.** `tests/vectors/*.bin`
  are fixed, committed encodings the *current* schema must always still be
  able to decode to the values in `vectors_spec.py`'s `GOLDEN_VECTORS` -
  shared by `generate_vectors.py` and `test_compatibility.py` rather than
  each keeping its own copy. **Never regenerate or overwrite an existing
  vector file** - that would delete the regression test it exists to
  provide; `test_golden_vector_hashes_are_unchanged` enforces this with a
  pinned `sha256` per vector, not just a comment, so even a vector deleted
  and regenerated back to a similar-looking file is caught. Add new vectors
  (via `generate_vectors.py`/`task generate`, which already skips files that
  exist) when a new message shape needs one; never touch old ones.

## Adding a new message group (PR 4+)

1. Add the new `.proto` file under `proto/`, plus a `common.proto` entry if
   more than one group needs the same type.
2. Add its `oneof` branch to `Envelope` in `envelope.proto`, using the field
   number the roadmap already assigned in
   [boomlink.md section 7](../../../docs/firmware/bom-stm32node/boomlink.md#7-boomprotocol-message-model).
3. Add any variable-size field's bound to a new `nanopb/<name>.options` file
   (nanopb auto-discovers it by the proto's name - see CMakeLists.txt's
   comment - so it does not need registering anywhere else; CMake picks up
   the new file automatically on the next build via `CONFIGURE_DEPENDS`,
   no manual reconfigure needed).
4. Add the new proto's name to `CMakeLists.txt`'s `BOOMLINK_PROTO_NAMES` list
   - the one place both the Nanopb and Python generation steps read it from.
5. Add golden vectors (`vectors_spec.py` + `generate_vectors.py`) and tests
   per the rules above.
