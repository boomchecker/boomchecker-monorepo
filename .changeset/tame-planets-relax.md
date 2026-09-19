---
"fw-common-boomlink": minor
"fw-bom-stm32node": patch
---

PR 2 — BoomProtocol + Nanopb foundation: shared `common`/`header`/`envelope`/
`system` Protobuf schema (Ping/Pong only for now), a pinned Nanopb dependency,
a target-agnostic `boomlink_codec` Envelope encode/decode wrapper, and a
host-native test build (C self-test + Python/Nanopb cross-language and
protocol-compatibility tests via CTest). `bom-stm32node` now links the
generated `boomlink_protocol` library into the firmware; no application
message groups (Detection/Config/Command/Telemetry) or BoomLink transport yet.
