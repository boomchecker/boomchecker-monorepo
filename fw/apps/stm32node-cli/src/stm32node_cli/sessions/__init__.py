"""Feature drivers (UI-agnostic) plus the feature registry the TUI reads."""

from .base import REGISTRY, Feature, FeatureInfo, Session, iter_features, register_feature
from .record import RecordResult, RecordSession

__all__ = [
    "Session",
    "Feature",
    "FeatureInfo",
    "REGISTRY",
    "register_feature",
    "iter_features",
    "RecordSession",
    "RecordResult",
]
