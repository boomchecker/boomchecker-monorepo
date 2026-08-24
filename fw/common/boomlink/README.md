# BoomProtocol / BoomLink shared code

Shared wire-format code for BoomLink (`docs/firmware/bom-stm32node/boomlink.md`),
in two layers that deliberately do not know about each other:

- **BoomProtocol** - the `.proto` source of truth, the Nanopb code-generation
  pipeline, and `boomlink_codec.h/.c`'s Envelope encode/decode.
- **BoomLink's link frame** - `linkframe/`, the fixed 20-byte binary header
  (section 7.3) that carries addressing, packet identity and ACK signalling.

The separation is enforced, not merely intended: section 9 requires that
"BoomLink never decodes the Protobuf payload and has no Nanopb dependency", so
the link layer must be able to filter, acknowledge and deduplicate a packet -
and reject foreign traffic from a few leading bytes - without a Protobuf
decoder anywhere near it. `boomlink_linkframe` therefore does not link Nanopb
and cannot include the codec's headers; the compiler rejects an attempt, rather
than a comment asking nicely.

Both layers are cross-checked against an independent Python implementation on
the same wire bytes: Python protobuf for the codec, `boomlink_linkframe.py` for
the header.

Not here: radio transport, and the link engine itself (addressing state, ACK
matching, retry, duplicate suppression, TX queue) - those are the later phases
of the roadmap's PR 3.

## Layout

```
proto/            .proto source files (source of truth for the wire format)
nanopb/           <name>.options - bounded-field sizes for Nanopb, one file
                  per .proto that needs one (auto-discovered by name, see
                  CMakeLists.txt's comment on why - avoids a spurious
                  "did not match any fields" warning on every proto that
                  doesn't need one). Nanopb would equally find one placed
                  next to its .proto in proto/ (its own upstream
                  convention), and the build tracks both locations - but
                  keep them here: having the same <name>.options in both
                  is a configure-time error, since the generator would
                  silently use one and ignore the other
boomlink_codec.h/.c   Envelope encode/decode + boomlink_envelope_init(),
                      target-agnostic C
linkframe/        BoomLink's fixed 20-byte link frame header (section 7.3):
                  boomlink_linkframe.h/.c, plus boomlink_linkframe.py - an
                  INDEPENDENT host implementation written from the spec, not
                  a binding to the C, so the two can check each other (and
                  the parser PR 5's host CLI will use). No Nanopb dependency,
                  by design and by build config
tests/            host-native C CLI tools (codec_tool.c, linkframe_tool.c,
                  sharing tool_support.c) + pytest suite + golden vectors
                  (vectors_spec.py is their single source of truth,
                  sha256-pinned - see "Protocol compatibility rules")
CMakeLists.txt    generation + libraries + (standalone only) test targets
```

`*.pb.c`/`*.pb.h` and `*_pb2.py` are never committed - they are generated at
build/test time (see CMakeLists.txt) and always reproducible from the
`.proto` + `.options` files, which are the only checked-in source of truth.

## Building and testing

This directory is a CMake project that builds two different ways:

- **Standalone** (this README's "building and testing" - what `task test`
  does): host-native compiler, builds `boomlink_codec_tool` and
  `boomlink_linkframe_tool` and runs the full CTest suite - the codec and
  link frame CLI self-tests, the link engine's C scenario binaries (port,
  duplicate cache, TX queue, RX pipeline, TX pipeline - see `CMakeLists.txt`
  for the current list, not restated here since it only grows), the
  Nanopb-free boundary and encoder-bound checks, and the Python
  interop/compatibility suite. Run `ctest --preset Debug` for the exact count
  and names.
- **As a subdirectory** of `fw/bom-stm32node`'s ARM cross build: the
  `boomlink_protocol`, `boomlink_linkframe` and `boomlink_linkengine` static
  libraries are built. `boomlink_protocol` is linked into the firmware and
  actually exercised there by the `proto` CLI command in `Core/Src/cli.c` (see
  its comment for why that matters); the other two are cross-compiled but not
  yet referenced by anything, which is deliberate - it proves they are free of
  host-isms and warning-clean for the target before the firmware that will use
  them exists.
  The warning-clean half needs `-DBOOMLINK_TARGET_WERROR=ON`, which CI passes
  and a local firmware build does not (so a warning from a newer compiler than
  CI's never blocks you); without it a warning there is only log text.
  Both archives are built but never reach the linker at all (nothing links
  them), so wiring them into the firmware will need `target_link_libraries`
  entries in `fw/bom-stm32node`, not merely a call site. The workflow asserts
  both archives exist, because with no dependents nothing else would notice
  their disappearing. No tests - a
  cross-compiled host tool makes no sense, and pytest cannot run on an
  STM32. Note this makes the host Python packages below a hard requirement
  of building the **firmware**, not just of running these tests: code
  generation runs on the build host in both modes. `task build` in
  `fw/bom-stm32node` puts this directory's `.venv` on `PATH` for exactly
  that reason, so `task setup` here is a prerequisite of the firmware build
  too.

```sh
task setup      # create .venv with protobuf/grpcio-tools/pytest/ruff
task test       # configure, build, run the full suite via CTest
task generate   # add any new golden vectors (see generate_vectors.py)
task lint       # ruff over tests/ and linkframe/
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
`-DBOOMLINK_SANITIZE=OFF` to `cmake --preset Debug` to disable it. The test
suite additionally forces `abort_on_error=1` on both sanitizers (see
`tests/_support.py`) so a finding arrives as a signal rather than an
ordinary nonzero exit that a negative-path test could mistake for the
tool's own intentional rejection.

Note the CMake option itself defaults to **OFF** - it is the Debug preset that
turns it on. A configure that bypasses the preset (`cmake -S . -B build`, or an
IDE's default) therefore gets a much weaker suite: without the sanitizers the
negative-path tests cannot tell an intentional rejection from a memory-safety
bug, and a real one passes green. That configuration warns at configure time
and the pytest header states it on every run; pass
`-DBOOMLINK_REQUIRE_SANITIZERS=ON` to make it a hard configure error instead.

The preset also sets `-DBOOMLINK_WERROR=ON`, which adds `-Werror` to this
package's own targets (never to the vendored nanopb runtime). Without it
`-Wall -Wextra` are advisory only - nothing fails and nothing annotates a
PR. It defaults to OFF, so a stricter local compiler than CI's can't block
an unrelated build; pass `-DBOOMLINK_WERROR=OFF` if a new compiler version
starts flagging something mid-task.

## Where boomlink.md section 15.2's list is covered

Section 15.2 names sixteen things a fake radio backend has to make testable.
Written down here because the list is a requirement and nothing enforces the
mapping - if a target is renamed or a scenario deleted, this table goes stale
silently. Treat it as a reading aid, not a check.

| Section 15.2 item | Where |
| --- | --- |
| link frame header encode/parse round-trip | `tests/test_linkframe.py`, both implementations on the same bytes |
| wrong magic/network ID or version rejected | `link_rx_test`: foreign and malformed traffic counted apart |
| unicast delivery | `link_rx_test`: reaches its destination and nobody else |
| wrong destination rejection | same scenario - the third node hears it and declines |
| broadcast acceptance | `link_rx_test`: broadcast reaches everyone, asks for no ACK |
| ACK matching | `link_tx_test`: an ACK completes the pending frame; near-miss ACKs do not. Rejection is pinned in `test_linkframe.py`, where an over-permissive matcher cannot hide |
| ACK timeout | `link_tx_test`: measured by advancing the clock 1 ms at a time, and shown to scale with the frame rather than being fixed |
| retry count | `link_tx_test`: three attempts, identical bytes, then final failure |
| duplicate suppression | `link_rx_test`: delivered once; and the key is `(source, session, sequence)`, pinned field by field |
| duplicate ACK resend | `link_rx_test`: the duplicate is acknowledged again, not delivered again |
| sequence/session across a reboot | `link_rx_test`: the same session replays and goes deaf; a fresh one is delivered. `dupcache_test` covers the table's own side |
| queue priority | `txqueue_test`, plus `link_rx_test` for the on-air sequence staying monotonic across reordering |
| queue overflow policy | `txqueue_test` in both directions, plus `link_tx_test` for the statistics |
| randomized backoff bounds | `link_tx_test`: seven measured draws, all in range and not all equal |
| malformed packet rejection | `link_rx_test`: truncated, oversize, and a byte corrupted in flight |
| a fake radio backend at all | `boomlink_port.h` + `tests/fake_port.h`, and `port_test` tests the fake itself - a fake that delivered a node its own frames would make duplicate suppression look like it worked without a second node involved |

Section 15.3's hardware tests are out of scope for host CI by definition.

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
  stack overflow when the bound changed and one copy wasn't updated. Nanopb
  enforces the generated struct's **padded** capacity rather than `max_size`
  itself, so a `max_size` that leaves padding makes it accept a byte (or more)
  past the declared array - with the default 2-byte `pb_size_t` that means
  keeping every `max_size` **even**, and under `PB_FIELD_32BIT` a multiple of 4.
  `boomlink_codec.c`'s `BOOMLINK_ASSERT_BOUNDED_BYTES` rejects a bad one at
  build time, but only for the fields explicitly listed there - see step 4 of
  the checklist below.
- **Shrinking a bound is a breaking wire-format change**, not a local tweak,
  once a committed golden vector carries a payload larger than the new value:
  old peers' frames stop decoding. That collision is what
  `test_golden_vector_decodes_with_nanopb` reports - it needs a
  `protocol_version` bump, never a regenerated vector. Growing a bound is
  safe until it blows the on-air budget, which `boomlink_codec.c` checks at
  build time.
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
   the new file automatically on the next build, and adding, removing or
   renaming one correctly forces regeneration regardless of the file's
   mtime - see CMakeLists.txt's comment on why `CONFIGURE_DEPENDS` alone is
   not enough for that).
4. **For each new bounded `bytes` field, add a
   `BOOMLINK_ASSERT_BOUNDED_BYTES(<Type>, <field>)` line in
   `boomlink_codec.c`.** C cannot enumerate a struct's members, so that check
   has to be written per field - and a field without one silently gets the
   over-accept-by-a-byte behaviour described in the compatibility rules above,
   which AddressSanitizer cannot see (the overrun lands inside the struct's own
   padding) and no test currently covers beyond `Ping`. `string` fields need
   no line: nanopb emits a plain `char[N]` and bounds it exactly.
5. Add the new proto's name to `CMakeLists.txt`'s `BOOMLINK_PROTO_NAMES` list
   - the one place both the Nanopb and Python generation steps read it from.
6. Add golden vectors (`vectors_spec.py` + `generate_vectors.py`) and tests
   per the rules above.
