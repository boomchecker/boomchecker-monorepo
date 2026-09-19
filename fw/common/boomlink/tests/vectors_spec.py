"""Single source of truth for tests/vectors/*.bin - imported by both
generate_vectors.py (which adds NEW vectors, never touches existing ones)
and test_compatibility.py (which checks existing ones still decode
correctly). Previously this dict was written out twice, with nothing
checking the two copies agreed.

Every value here, including `payload`'s length, is a literal - never a
reference to nanopb/system.options' current max_size (or any other
runtime-configurable bound). These are frozen historical encodings; if a
future PR raises or lowers that bound, values here must not silently follow
it along - see the "protocol compatibility rules" in README.md.
"""

import hashlib
import pathlib

VECTORS_DIR = pathlib.Path(__file__).parent / "vectors"

GOLDEN_VECTORS = {
    "ping_basic.bin": {
        "protocol_version": 1,
        "request_id": 1,
        "kind": "ping",
        "payload": b"",
        "sha256": "e816ea70dbf7e9725b106034357c9f4c1a83570c2778124d961ace0d5a98e3af",
    },
    "ping_with_payload.bin": {
        "protocol_version": 1,
        "request_id": 2,
        "kind": "ping",
        "payload": bytes.fromhex("deadbeef"),
        "sha256": "bf35b3c7e1767e475c4779d406c60c8903bc5a2d3a5d71452bb6edb651ea4994",
    },
    "pong_basic.bin": {
        "protocol_version": 1,
        "request_id": 3,
        "kind": "pong",
        "payload": b"",
        "sha256": "78babceeae581ade8d214dc8bc7d2d55d89275aff73712e34b57bd00aee5828e",
    },
    "pong_max_payload.bin": {
        "protocol_version": 1,
        "request_id": 4,
        "kind": "pong",
        "payload": b"\xaa" * 192,
        "sha256": "f718015ae70c0087f48bbed3108d916a0eb870d7c6b30d3024fac9827f82d85b",
    },
}


def verify_vector_hashes():
    """Raises AssertionError if any committed vector's bytes differ from the
    hash recorded here - catches a vector being silently replaced (e.g.
    `rm vectors/foo.bin && python generate_vectors.py` producing a fresh
    "historical" encoding) that would otherwise still decode successfully
    and pass every other check, defeating the whole point of a golden
    vector. If a vector genuinely needs to change, that is a new vector
    under a new name, not an edit to this one."""
    for filename, spec in GOLDEN_VECTORS.items():
        path = VECTORS_DIR / filename
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != spec["sha256"]:
            raise AssertionError(
                f"{filename} does not match its recorded sha256 (expected "
                f"{spec['sha256']}, got {actual}) - if this vector's bytes "
                "were intentionally regenerated, that defeats its purpose as "
                "a historical regression check. Add a new vector instead of "
                "replacing this one."
            )
