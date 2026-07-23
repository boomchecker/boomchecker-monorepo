"""Session base class and the feature registry.

A *feature* is one thing the board can do (record audio today; a detector
readout tomorrow). Adding one means: write a ``Session`` subclass here, write a
Textual screen under ``tui/screens/`` and call :func:`register_feature` from
that screen module. The TUI dashboard renders whatever is registered, so no
central switchboard needs editing.

This module deliberately does **not** import Textual: ``screen_factory`` is a
lazy callable supplied by the UI layer, keeping the session logic importable
(and testable) on its own.
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from collections.abc import Callable
from dataclasses import dataclass


@dataclass(frozen=True)
class FeatureInfo:
    """User-facing metadata for a feature, shown on the dashboard."""

    key: str
    title: str
    description: str


@dataclass(frozen=True)
class Feature:
    """A registered feature: its metadata and a factory for its TUI screen."""

    info: FeatureInfo
    screen_factory: Callable[[], object]  # returns a textual.screen.Screen


REGISTRY: dict[str, Feature] = {}


def register_feature(feature: Feature) -> None:
    """Register a feature (idempotent by key)."""
    REGISTRY[feature.info.key] = feature


def iter_features() -> tuple[Feature, ...]:
    """Return registered features in insertion order."""
    return tuple(REGISTRY.values())


class Session(ABC):
    """UI-agnostic driver for one device feature over a :class:`DeviceClient`."""

    @abstractmethod
    def run(self, *args: object, **kwargs: object) -> object:
        """Execute the feature and return its result."""
