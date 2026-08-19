# stm32node-cli

Host-side tools for the **boomchecker-node** STM32 board (STM32H563), talking
over its USB CDC ACM virtual COM port. A Textual TUI plus a small Typer CLI.

Today it can **record N seconds of PCM audio and save it as a WAV** — useful for
bringing up the microphone path without an SD card. The architecture is layered
so more device features (e.g. reading a detector's output) slot in as new
sessions + screens.

## Layout

```
src/stm32node_cli/
  transport/   byte pipes (SerialTransport; a test double lives in tests/)
  protocol/    the wire contract: spec (source of truth) -> codec -> DeviceClient
  sessions/    UI-agnostic feature drivers + the feature registry
  tui/         Textual app; one screen per feature
PROTOCOL.md    generated from protocol/spec.py — the contract the firmware implements
```

## Tasks

```
task stm32-cli:setup    # create .venv and install (editable) with dev deps
task stm32-cli:run      # launch the TUI  (-- --port /dev/ttyACM0)
task stm32-cli:test     # run the test suite (no hardware needed)
task stm32-cli:lint     # ruff
task stm32-cli:proto    # regenerate PROTOCOL.md from the spec
```

Or directly: `stm32node-cli tui`, `stm32node-cli record 5 --port /dev/ttyACM0`,
`stm32node-cli ports`.

## Protocol

The serial protocol is defined once in `src/stm32node_cli/protocol/spec.py` and
rendered into [`PROTOCOL.md`](PROTOCOL.md). That document is the contract the
STM32 firmware must implement (the `stream <sec>` command and the `PCM1` binary
framing). A test guards that the generated file stays in sync with the spec.

> The firmware `stream <sec>` command does not exist yet — this tool defines the
> contract; the firmware side is a later phase.
