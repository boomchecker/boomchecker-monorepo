"""Model families: how each is trained, scored in Python, and written as C.

Every family exposes the same two things: a `score(features) -> decision`
callable that reproduces the C forward pass in float32 (so the parity fixtures
can be generated without a board), and an exporter that writes the header the
C translation unit under models/ includes.
"""
