#!/usr/bin/env python3
"""One-shot generator that ADDS new entries to tests/vectors/*.bin.

Run this ONLY to add a new golden vector for a newly introduced message
shape. NEVER re-run it to regenerate an existing vector: the whole point of
a golden vector is that it is a fixed, historical encoding the CURRENT
schema must still be able to decode (boomlink.md section 15.1,
"old golden vectors decoding with the current schema"). Overwriting one
would silently delete the regression test it exists to provide - it already
skips any filename that exists, and vectors_spec.py's
verify_vector_hashes() (called from test_compatibility.py) independently
catches a vector's bytes changing even if this skip is ever bypassed (e.g.
the file was deleted first).

Requires the generated `envelope_pb2` module on PYTHONPATH (see
CMakeLists.txt's `boomlink_python_pb2` target, or `task generate`).

Workflow for adding a new vector (the sha256 cannot be known before the file
is generated, so this is necessarily two steps, not one):
  1. Add a new entry to vectors_spec.py's GOLDEN_VECTORS with a "sha256" of
     "" (or any placeholder) - it will not match yet, and that's expected.
  2. Run this script (`task generate`). It writes the new file(s) and PRINTS
     the real sha256 of each - paste that into the entry's "sha256" field.
"""

import hashlib

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
        data = build(spec)
        path.write_bytes(data)
        digest = hashlib.sha256(data).hexdigest()
        print(f"wrote: {filename}  sha256={digest}")
        print(f'  -> paste into vectors_spec.py: "sha256": "{digest}",')


if __name__ == "__main__":
    main()
