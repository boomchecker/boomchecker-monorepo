---
"fw-bom-stm32node": minor
"fw-common-boomlink": patch
---

PR 4 Phase C: wire the BoomProtocol dispatcher (command + config services)
into `bom-stm32node`'s firmware. `App/protocol/protocol_service.c/.h` loads
persisted `NodeConfig` at boot, feeds the resolved identity into
`link_service_init()`, and dispatches inbound Command/Config requests over
the link engine, including a hazardous-field stage → commit → confirm →
revert-on-timeout apply cycle and real command actions (`Reboot`/
`SelfTest`/`ClearStatistics`/`RequestDiagnostics`) backed by this board's
actual radio/link state. Eight rounds of review hardened the broadcast
guards, config persistence, and defaults along the way, including a new
`boomlink_config_service_get_persistable_config()` accessor in
`fw-common-boomlink` that a caller persisting config to flash must use
instead of the plain current-state accessor.
