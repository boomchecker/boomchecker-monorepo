# stm32node-cli

Host-side tools for the **boomchecker-node** STM32 board (STM32H563), talking
over its USB CDC ACM virtual COM port. A Textual TUI plus a small Typer CLI.

Today it can **record N seconds of PCM audio and save it as a WAV** — useful for
bringing up the microphone path without an SD card — **run the board's on-device
drone detector** (`detect`), streaming its `LVL`/`DET`/`ALM` report lines live
and printing a `DETEND` summary, and **switch the active model** (`model`) and
**PDM microphone slot** (`micslot`) so a whole field test runs from here without
a raw terminal. The architecture is layered so more device features slot in as
new sessions + screens.

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
`stm32node-cli detect 5 --port /dev/ttyACM0`, `stm32node-cli ports`.

### Model and microphone selection

```
stm32node-cli model              # list the classifiers in the image, * = active
stm32node-cli model gbt_f1       # select one for later detect runs
stm32node-cli micslot            # show which PDM mic of the pair is decoded
stm32node-cli micslot a          # select slot A (the live mic on this build)
```

Both selections mirror the firmware `model`/`micslot` console commands. Neither
is persisted on the board — a reset returns to the deployed default (`mlp_f1`,
and the firmware's default slot). They **do** survive across separate CLI calls,
though: opening the USB serial port does not reset the board, so
`stm32node-cli model gbt_f1` followed by a separate `stm32node-cli detect 30`
runs `gbt_f1`. The same commands work in the TUI console.

### Stopping detect

`detect 0` runs until stopped; any positive `<sec>` can also be ended early.
**Press any key** to stop — the board is sent a byte, so it stops cleanly and
frees the radio instead of running on blind (the old `bdcli.py` couldn't do
this); the host then prints `stopped`. Ctrl-C also works. The stop key needs an
interactive terminal; under a pipe/redirect use Ctrl-C. In the TUI console, type
`q` then Enter. A run that reaches its `<sec>` limit on its own still prints the
full `DETEND` summary.

### Batch recording

`record <sec> <count>` (TUI console and CLI alike) records `<count>` files of
`<sec>` seconds each into one folder, `recordings/batch-YYYYmmdd-HHMMSS/`.
Without `<count>` it is the single-file `record` as before:

```
stm32node-cli record 10 20 --port COM7
```

Each `chunk-NNN.wav` is written the moment its seconds have arrived while the
board keeps streaming, so `q` in the TUI or Ctrl-C on the CLI keeps everything
recorded so far. `index.csv` in the folder lists every file with its stream
number, real length and the stream's overrun/err health.

The board is asked for the longest `stream` that holds a whole number of chunks
(60 s = six 10-second files) and the host cuts it as it arrives. Boundaries
inside one stream are gapless; between streams there is a command round-trip
and the PDM settling transient (~0.12 s clipped), so the first chunk of every
stream starts with it. A chunk longer than 60 s is rejected.

## Protocol

The serial protocol is defined once in `src/stm32node_cli/protocol/spec.py` and
rendered into [`PROTOCOL.md`](PROTOCOL.md). That document is the contract the
STM32 firmware must implement (the `stream <sec>` command and the `PCM1` binary
framing). A test guards that the generated file stays in sync with the spec.

> The firmware `stream <sec>` command does not exist yet — this tool defines the
> contract; the firmware side is a later phase.
