# BoomProtocol

BoomProtocol is the message layer: what a node can actually say. Every application
message is one Protobuf `Envelope`, encoded with [Nanopb](https://github.com/nanopb/nanopb)
and carried as the payload of a BoomLink `DATA` frame (see [BoomLink](boomlink.md)).
Schemas live under `fw/common/boomlink/proto/*.proto` and are shared between the
firmware (C, via Nanopb) and any host tooling (Python, via `protoc`) — one definition,
never two that could drift apart.

## Envelope

```protobuf
message Envelope {
  MessageHeader header = 1;

  oneof payload {
    DetectionMessage detection = 10;
    ConfigMessage config = 11;
    CommandMessage command = 12;
    TelemetryMessage telemetry = 13;
    SystemMessage system = 14;
  }
}
```

Message groups are deliberately coarse — a new kind of detection message is added
*inside* `DetectionMessage`, not as a new top-level `Envelope` field.

### MessageHeader

```protobuf
message MessageHeader {
  uint32 protocol_version = 1;
  uint32 request_id = 2;
}
```

| Field | Meaning |
|---|---|
| `protocol_version` | BoomProtocol compatibility version — currently `1`. |
| `request_id` | Correlates a request with its response at the application level. Zero when unused (an unsolicited message like a broadcast `WakeupRequest` or a `DetectionEvent`). |

An `Envelope` with no header at all, or `protocol_version == 0`, is malformed and
dropped before decoding even starts. Sender identity, RSSI/SNR and similar are **not**
duplicated in here — they're link-layer metadata delivered alongside the decoded
`Envelope`, not inside it (see [BoomLink](boomlink.md)).

## Implementation status

| Message group | Status | What that means |
|---|---|---|
| [Commands](#commands) | ✅ Implemented | Real actions behind most command types. |
| [Configuration](#configuration) | ✅ Implemented | Full read/write, versioning, persisted to flash, revert-on-timeout for risky fields. |
| [System messages](#system-messages) | ✅ Implemented | Ping/Pong echo and fleet-discovery Wakeup. |
| [Detection](#detection) | ⚠️ Schema only | The dispatcher recognizes and counts `DetectionEvent`/etc.; no detection algorithm exists yet to generate one. |
| [Telemetry](#telemetry) | ⚠️ Schema only | Same as Detection — recognized and counted, nothing populates or sends a `TelemetryReport` yet. |

For anything marked **schema only**: a received message of that type is decoded,
counted in the dispatcher's stats, and produces no response and no other effect. There
is currently no way to *send* one either — nothing in the firmware calls
`boomlink_build_detection_event()`/`boomlink_build_telemetry_report()` (the one-way
envelope builders that already exist for exactly this) because nothing generates the
underlying data yet.

## Commands

Immediate actions, as opposed to configuration (persistent state) — `command.proto`.

```protobuf
message CommandMessage {
  oneof message {
    CommandRequest request = 1;
    CommandResponse response = 2;
  }
}

message CommandRequest {
  CommandType type = 1;
}

message CommandResponse {
  CommandType type = 1;
  CommandResult result = 2;
  string diagnostic = 3;   // free-form, bounded, for humans — result is authoritative
}
```

`CommandType`, and what actually happens on this board when it's requested:

| Type | Implemented? | Real action |
|---|---|---|
| `REBOOT` | ✅ | Arms a deferred `HAL_NVIC_SystemReset()` (never synchronous, so the response reaches the requester first). Rejected outright if addressed to broadcast. |
| `SELF_TEST` | ✅ | Reports whether the radio is ready. |
| `CLEAR_STATISTICS` | ✅ | Resets radio and link statistics counters. |
| `REQUEST_DIAGNOSTICS` | ✅ | One-line summary: node ID, TX/RX counts, TX failures, RX CRC errors. |
| `IDENTIFY` | ❌ `COMMAND_RESULT_UNSUPPORTED` | This board has no LED to blink. |
| `START_DETECTION` / `STOP_DETECTION` | ❌ `COMMAND_RESULT_UNSUPPORTED` | No detection algorithm exists yet to start or stop. |

`CommandResult`: `OK`, `UNSUPPORTED`, `BUSY`, `FAILED` (plus the proto3 zero value,
`UNSPECIFIED`, never sent for a real response).

## Configuration

Persistent, versioned node settings — `config.proto`. `NodeConfig` is the whole
persisted structure, split into six groups.

```protobuf
message ConfigMessage {
  oneof message {
    ConfigGetRequest get_request = 1;
    ConfigGetResponse get_response = 2;
    ConfigSetRequest set_request = 3;
    ConfigSetResponse set_response = 4;
  }
}
```

**Get** — a bool per group, requesting only what's needed:

```protobuf
message ConfigGetRequest {
  bool include_general = 1;
  bool include_link = 2;
  bool include_radio = 3;
  bool include_detection = 4;
  bool include_gnss = 5;
  bool include_telemetry = 6;
}
```

`ConfigGetResponse` mirrors that shape with the actual `config_version` plus each
requested group populated (an unrequested group is simply absent, not zeroed).

**Set** — always a **whole-group replacement**, never a per-field patch (proto3 can't
tell "left at zero" apart from "meant zero" for a scalar field, so a patch built on
presence would silently reset any field the caller didn't restate):

```protobuf
message ConfigSetRequest {
  uint32 expected_config_version = 1;   // optimistic concurrency
  GeneralConfig general = 2;
  LinkConfig link = 3;
  RadioConfig radio = 4;
  DetectionConfig detection = 5;
  GnssConfig gnss = 6;
  TelemetryConfig telemetry = 7;
}

message ConfigSetResponse {
  ConfigSetResult result = 1;
  uint32 config_version = 2;   // authoritative, post-request version either way
}
```

| `ConfigSetResult` | Meaning |
|---|---|
| `OK` | Accepted and applied immediately. |
| `VERSION_CONFLICT` | `expected_config_version` didn't match — someone else changed the config first. |
| `INVALID` | A field's requested value is structurally invalid (e.g. `node_id` set to the reserved `0x00000000`/`0xFFFFFFFF`, or `magic` out of its one-byte range) — or the whole request was addressed to broadcast, which is always refused outright. |
| `PENDING_CONFIRMATION` | A hazardous field (`node_id`, `magic`, or the radio profile) was applied but not yet confirmed — see below. |
| `APPLY_IN_PROGRESS` | A previous hazardous change is still inside its own confirmation window; this request is rejected rather than merged with it. |

!!! info "Revert-on-timeout for risky fields"
    Changing `node_id`, `magic`, or the radio profile live can strand a node — it can no
    longer be reached at the old address/profile, or a peer can no longer reach it at
    the new one. These three fields apply immediately but only *commit* once a
    confirmation exchange succeeds within a bounded window (a few seconds); if nothing
    confirms in time, the change reverts automatically to the last known-good value.

Config groups, in full:

=== "GeneralConfig"

    | Field | Type | Notes |
    |---|---|---|
    | `node_id` | `uint32` | This node's link-layer address. |
    | `default_destination_id` | `uint32` | Where this node sends unsolicited traffic by default. |
    | `receive_enabled` | `bool` | |
    | `transmit_enabled` | `bool` | |
    | `usb_forward_enabled` | `bool` | Gateway behaviour — not yet implemented. |
    | `promiscuous_monitor_enabled` | `bool` | Debug/monitor mode; doesn't change normal address acceptance. |

=== "LinkConfig"

    | Field | Type | Notes |
    |---|---|---|
    | `magic` | `uint32` | Network ID — one byte on the wire (see [BoomLink](boomlink.md)). Hazardous. |
    | `ack_timeout_margin_ms` | `uint32` | |
    | `max_attempts` | `uint32` | |
    | `backoff_min_ms` / `backoff_max_ms` | `uint32` | |
    | `tx_jitter_max_ms` | `uint32` | |

=== "RadioConfig"

    | Field | Type | Notes |
    |---|---|---|
    | `frequency_mhz` | `float` | Hazardous. |
    | `bandwidth_khz` | `float` | |
    | `spreading_factor` | `uint32` | |
    | `coding_rate_denom` | `uint32` | |
    | `tx_power_dbm` | `int32` | |
    | `preamble_symbols` | `uint32` | |
    | `sync_word` | `uint32` | |

=== "DetectionConfig"

    | Field | Type | Notes |
    |---|---|---|
    | `detection_enabled` | `bool` | |
    | `drone.confidence_threshold_percent` | `uint32` | |
    | `gunshot.confidence_threshold_percent` | `uint32` | |

=== "GnssConfig"

    | Field | Type | Notes |
    |---|---|---|
    | `gnss_enabled` | `bool` | A GNSS module is wired on this board but has no driver yet. |

=== "TelemetryConfig"

    | Field | Type | Notes |
    |---|---|---|
    | `report_interval_s` | `uint32` | Not acted on yet — see [Telemetry](#telemetry). |

## System messages

Protocol housekeeping — `system.proto`. Two independent uses share the same
`SystemMessage` envelope:

```protobuf
message SystemMessage {
  oneof message {
    Ping ping = 1;
    Pong pong = 2;
    WakeupRequest wakeup_request = 3;
    WakeupResponse wakeup_response = 4;
  }
}
```

**Ping/Pong** — a liveness/round-trip probe, answered synchronously:

```protobuf
message Ping {
  bytes payload = 1;   // optional, for exercising a specific on-air packet size
}
message Pong {
  bytes payload = 1;   // echoed back verbatim
}
```

`MessageHeader.request_id` correlates the `Pong` to its `Ping` — nothing else is
needed.

**Wakeup — fleet discovery.** There's no join/registration protocol and no special
master address in BoomLink (see [LoRa → Naming](index.md#naming)) — so there was no way
to ask "who's actually reachable on this network" without already knowing every
`node_id`. Wakeup closes that gap:

```protobuf
message WakeupRequest {
  uint32 window_s = 1;
}

enum DeviceType {
  DEVICE_TYPE_UNSPECIFIED = 0;
  DEVICE_TYPE_STMNODE = 1;
}

message WakeupResponse {
  uint32 node_id           = 1;
  DeviceType device_type   = 2;
  uint32 fw_version_major  = 3;
  uint32 fw_version_minor  = 4;
  uint32 fw_version_patch  = 5;
}
```

Any node broadcasts a `WakeupRequest{window_s}`. Every node that receives it draws a
delay uniformly from `[0, window_s]` seconds and replies with a `WakeupResponse`
*after* that delay, addressed back to the requester — not broadcast, and not
synchronous the way Ping/Pong is. Spreading the replies out this way avoids every node
answering the same broadcast at the same instant (the same collision shape [BoomLink's
TX jitter](boomlink.md#random-jitter-collision-avoidance) exists for, just orders of
magnitude larger). `window_s == 0` is valid — reply immediately, useful for a
point-to-point sanity check. A second `WakeupRequest` arriving while a reply is still
pending **restarts** the window with a fresh draw, since a re-broadcast usually means
the first round didn't hear back from enough nodes. `window_s` is capped at 3600 s
regardless of what a request asks for.

On this firmware:

```text
> wakeup 10
wakeup: broadcast sent (window 10 s)
wakeup: 0xCD5B9231 responded (stmnode, fw v0.1.0)
```

## Detection

`detection.proto` — schema only today (see [Implementation status](#implementation-status)).

```protobuf
message DetectionMessage {
  oneof message {
    DetectionEvent event = 1;
    DetectionStatus status = 2;
    DetectionStatistics statistics = 3;
  }
}
```

`DetectionEvent` — deliberately no per-type detail fields (a gunshot's peak level, a
drone's rotor count, ...): no detection algorithm exists yet to say what it would
actually need.

| Field | Type | Notes |
|---|---|---|
| `event_id` | `uint32` | |
| `detection_timestamp_us` | `uint64` | Microseconds since Unix epoch, captured at detection time — not transmission time. |
| `timestamp_source` | `TimestampSource` | `GNSS_DISCIPLINED` / `LOCAL_RTC` / `NETWORK_FALLBACK`. |
| `timestamp_uncertainty_us` | `uint32` | `0` only for a perfectly disciplined source. |
| `type` | `DetectionType` | `OTHER` / `DRONE` / `GUNSHOT` / `EXPLOSION`. |
| `confidence_percent` | `uint32` | 0–100. |
| `position_valid`, `latitude_e7`, `longitude_e7` | `bool`, `int32`, `int32` | 1e-7 degrees. |
| `bearing_valid`, `azimuth_centideg`, `elevation_centideg` | `bool`, `uint32`, `int32` | Centidegrees. |
| `detector_id`, `detector_version` | `uint32` | Opaque to BoomLink — meaningful only to whatever ran the detection. |

`DetectionStatus{detection_enabled}` and `DetectionStatistics` (per-type event counters)
round out the group.

## Telemetry

`telemetry.proto` — schema only today (see [Implementation status](#implementation-status)).

```protobuf
message TelemetryMessage {
  oneof message {
    TelemetryReport report = 1;
  }
}
```

| Field | Type | Notes |
|---|---|---|
| `uptime_s` | `uint32` | |
| `supply_voltage_mv` | `uint32` | `0` if unmeasured. |
| `temperature_valid`, `temperature_c` | `bool`, `float` | |
| `gnss_fix_valid`, `gnss_accuracy_cm` | `bool`, `uint32` | |
| `last_rssi_dbm`, `last_snr_db` | `float`, `float` | Mirrors the link engine's own stats. |
| `tx_packets`, `rx_packets`, `radio_errors` | `uint32` | |
| `detection_count`, `reset_count` | `uint32` | |
