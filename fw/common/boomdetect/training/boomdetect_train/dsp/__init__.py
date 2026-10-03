"""The front end, mirrored: decimation, framing, RMS gate, MFCC, windows.

Every function here has a C counterpart in fw/common/boomdetect/src and is
tested against it (tests/test_parity_selftest.py drives both from the same
integer LCG the board's `detselftest` uses). The mel, window and DCT tables are
not reimplemented: they are parsed out of src/mfcc_tables.h, so the Python and
the firmware cannot disagree about a filter edge.
"""
