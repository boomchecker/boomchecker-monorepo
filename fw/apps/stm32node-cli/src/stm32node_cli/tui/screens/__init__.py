"""TUI feature screens.

Importing this package imports every screen module, and each screen module
registers its feature with the session registry (see ``sessions.base``). The
dashboard then renders whatever ended up registered.
"""

from . import record  # noqa: F401  (import for its register_feature side effect)

__all__ = ["record"]
