"""Feature drivers (UI-agnostic) plus the console command registry."""

from .base import (
    REGISTRY,
    Command,
    CommandContext,
    Session,
    get_command,
    iter_commands,
    register_command,
)
from .record import RecordResult, RecordSession

__all__ = [
    "Session",
    "Command",
    "CommandContext",
    "REGISTRY",
    "register_command",
    "get_command",
    "iter_commands",
    "RecordSession",
    "RecordResult",
]
