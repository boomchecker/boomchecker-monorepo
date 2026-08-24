# Boom Test

Hardware-in-the-loop pytest suite for BoomLink. Connects to two physical
`bom-stm32node` boards over USB CDC serial, assigns one `master` and one
`slave` for the run, and drives both through their CLI (`cli.c`) to exercise
the two-board scenarios from
[boomlink.md section 15.3](../../../docs/firmware/bom-stm32node/boomlink.md#153-hardware-integration-tests).

`master`/`slave` is a test-harness pairing only, chosen for this run so
scenarios can address "the other board" without an operator labelling
them by hand. BoomLink itself has no master address (section 7.2) - both
boards run the same firmware image.

## What's covered today

Only [section 15.3](../../../docs/firmware/bom-stm32node/boomlink.md#153-hardware-integration-tests)
scenario 1 (raw RadioLib/EBYTE ping/pong) is implemented, because it is the
only one whose CLI surface exists yet (PR 1). Scenarios 2-8 need the
BoomLink link engine (PR 3), the detection/config/command services (PR 4) and
the USB gateway framing (PR 5); add them here as those land - the
discovery/pairing fixtures in `tests/conftest.py` are meant to be reused
as-is.

```
tests/
  test_protocol.py    CLI reply parsing - no hardware needed, runs anywhere
  test_discovery.py   the rig can find exactly 2 boomchecker-node boards
  test_pairing.py     master/slave resolve to distinct, live boards
  test_ebyte_radio.py scenario 1: radio ping from one board, radio rx on the other
```

## Setup

```sh
task boom-test:setup
```

Connect two boomchecker-node boards over USB (VID:PID `0483:5710`, see
`fw/bom-stm32node/host/boomchecker-node.inf`) and check they enumerate:

```sh
task boom-test:ports
```

## Running

```sh
task boom-test:test
```

With exactly two boomchecker-node boards connected, they are auto-discovered
and paired deterministically (sorted by USB serial number). Tests **skip**
(not fail) if fewer than two are found, or if more than two are found and
discovery can't tell which pair to use - pick explicitly instead:

```sh
task boom-test:test -- --master-port /dev/ttyACM0 --slave-port /dev/ttyACM1
```

`test_protocol.py` needs no hardware and always runs.

## Notes

- The board's CLI is a plain text console (embedded-cli), not the framed
  `stm32node_cli` protocol - `boom_test.board.Board` talks to it directly
  rather than depending on `fw/apps/stm32node-cli`.
- `radio ping`/`radio status`/`radio rx: ...` are PR 1's raw bring-up
  commands (see `cli.c`), not yet part of any versioned protocol - expect
  their exact text to move once BoomLink/BoomProtocol commands replace them.
