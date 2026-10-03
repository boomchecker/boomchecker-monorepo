"""Training, evaluation and C export for the boomdetect acoustic drone detector.

The C library in fw/common/boomdetect owns the arithmetic that runs on the
board. This package owns everything that produces and judges the numbers it
ships: a Python front end that mirrors the C stage by stage (and is tested
against it), the training of every model family the registry knows, the
evaluation that compares them on identical windows, and the exporters that
write the model headers and the parity fixtures the C tests consume.

Nothing here runs on the board, and nothing on the board depends on this
package at build time.
"""

__all__ = ["__version__"]
__version__ = "0.1.0"
