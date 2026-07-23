"""Session base class and the console command registry.

A *command* is one thing you can run from the TUI console (``record`` today, a
detector readout tomorrow). Adding one means: write a ``Session`` subclass for
the logic if needed, then build a :class:`Command` and call
:func:`register_command`. The console lists and dispatches whatever is
registered, so no central switchboard needs editing.

This module deliberately does **not** import Textual: a command's ``run`` is a
plain callable given a :class:`CommandContext`, keeping the logic importable
(and testable) without a UI.
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class CommandContext:
    """Everything a command needs from the app, with no UI coupling.

    ``emit`` appends one line to the console output; it is safe to call from a
    worker thread (the console marshals it back to the UI).
    """

    port: str
    out_dir: Path
    emit: Callable[[str], None]


# A command handler: given the context and the parsed argument list, do the work
# and report progress/results via ``ctx.emit``. Runs in a worker thread.
CommandRun = Callable[["CommandContext", "list[str]"], None]


@dataclass(frozen=True)
class Command:
    """A console command: its name, one-line usage/help and its handler."""

    name: str
    usage: str
    help: str
    run: CommandRun


REGISTRY: dict[str, Command] = {}


def register_command(command: Command) -> None:
    """Register a command (idempotent by name)."""
    REGISTRY[command.name] = command


def get_command(name: str) -> Command | None:
    """Look up a command by name (or None)."""
    return REGISTRY.get(name)


def iter_commands() -> tuple[Command, ...]:
    """Return registered commands in insertion order."""
    return tuple(REGISTRY.values())


class Session(ABC):
    """UI-agnostic driver for one device feature over a :class:`DeviceClient`."""

    @abstractmethod
    def run(self, *args: object, **kwargs: object) -> object:
        """Execute the feature and return its result."""
