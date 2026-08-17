#!/usr/bin/env python3
"""One-shot generator for tests/vectors/*.bin.

Run this ONLY to add a new golden vector for a newly introduced message
shape. NEVER re-run it to regenerate an existing vector: the whole point of
a golden vector is that it is a fixed, historical encoding the CURRENT
schema must still be able to decode (boomlink.md section 15.1,
"old golden vectors decoding with the current schema"). Overwriting one
would silently delete the regression test it exists to provide.

Requires the generated `envelope_pb2` module on PYTHONPATH (see
CMakeLists.txt's `boomlink_python_pb2` target, or run
`task generate` first).
"""

import pathlib

import envelope_pb2

VECTORS_DIR = pathlib.Path(__file__).parent / "vectors"

VECTORS = {
    "ping_basic.bin": {"protocol_version": 1, "request_id": 1, "kind": "ping", "payload": b""},
    "ping_with_payload.bin": {
        "protocol_version": 1,
        "request_id": 2,
        "kind": "ping",
        "payload": bytes.fromhex("deadbeef"),
    },
    "pong_basic.bin": {"protocol_version": 1, "request_id": 3, "kind": "pong", "payload": b""},
    "pong_max_payload.bin": {
        "protocol_version": 1,
        "request_id": 4,
        "kind": "pong",
        "payload": b"\xaa" * 192,  # nanopb/boomlink.options' max_size
    },
}


def build(spec: dict) -> bytes:
    envelope = envelope_pb2.Envelope()
    envelope.header.protocol_version = spec["protocol_version"]
    envelope.header.request_id = spec["request_id"]
    getattr(envelope.system, spec["kind"]).payload = spec["payload"]
    return envelope.SerializeToString()


def main():
    VECTORS_DIR.mkdir(exist_ok=True)
    for filename, spec in VECTORS.items():
        path = VECTORS_DIR / filename
        if path.exists():
            print(f"skip (already exists): {filename}")
            continue
        path.write_bytes(build(spec))
        print(f"wrote: {filename}")


if __name__ == "__main__":
    main()
