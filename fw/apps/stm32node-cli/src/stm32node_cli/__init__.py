"""stm32node-cli: host-side tools to talk to the boomchecker-node STM32 board.

Layered so that features are easy to add without touching the transport or
protocol code:

* ``transport`` - byte pipes (real serial today; anything implementing the
  ``Transport`` ABC works, including the in-memory test double).
* ``protocol``  - the wire contract: a declarative ``spec`` (single source of
  truth, also rendered into ``PROTOCOL.md``), a ``codec`` and a ``DeviceClient``.
* ``sessions``  - UI-agnostic feature drivers (e.g. recording) plus a feature
  registry that the TUI dashboard reads.
* ``tui``       - the Textual user interface, one screen per feature.
"""

__version__ = "0.1.0"
