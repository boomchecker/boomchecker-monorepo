#!/usr/bin/env python3
"""One-shot generator that ADDS new entries to tests/vectors/*.bin.

Run this ONLY to add a new golden vector for a newly introduced message
shape. NEVER re-run it to regenerate an existing vector: the whole point of
a golden vector is that it is a fixed, historical encoding the CURRENT
schema must still be able to decode (boomlink.md section 15.1,
"old golden vectors decoding with the current schema"). Overwriting one
would silently delete the regression test it exists to provide - it already
skips any filename that exists, and test_compatibility.py's
verify_vector_hashes() independently catches a vector's bytes changing even
if this skip is ever bypassed (e.g. the file was deleted first).

Requires the generated `envelope_pb2` module on PYTHONPATH (see
CMakeLists.txt's `boomlink_python_pb2` target, or `task generate`).

After adding a new vector, add its entry (including a `sha256` of the
resulting file) to vectors_spec.py's GOLDEN_VECTORS.
"""

import envelope_pb2
from vectors_spec import GOLDEN_VECTORS, VECTORS_DIR


def build(spec: dict) -> bytes:
    envelope = envelope_pb2.Envelope()
    envelope.header.protocol_version = spec["protocol_version"]
    envelope.header.request_id = spec["request_id"]
    getattr(envelope.system, spec["kind"]).payload = spec["payload"]
    return envelope.SerializeToString()


def main():
    VECTORS_DIR.mkdir(exist_ok=True)
    for filename, spec in GOLDEN_VECTORS.items():
        path = VECTORS_DIR / filename
        if path.exists():
            print(f"skip (already exists): {filename}")
            continue
        path.write_bytes(build(spec))
        print(f"wrote: {filename}")


if __name__ == "__main__":
    main()
