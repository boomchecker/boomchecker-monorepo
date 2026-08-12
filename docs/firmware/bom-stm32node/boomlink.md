# BoomLink radio architecture and implementation roadmap

This document is the implementation source of truth for LoRa P2P communication on
`fw/bom-stm32node` (STM32H563 + EBYTE E22-900M22S / SX1262).

The goal is to get a small, testable P2P network running quickly without locking the
firmware into a one-off packet format. All STM32 nodes run the **same firmware**. Node
identity, gateway behaviour, detection parameters, telemetry and radio/link behaviour
are runtime configuration.

The design deliberately separates three concerns:

1. **Radio** — hardware access and LoRa PHY through RadioLib.
2. **BoomLink** — P2P link behaviour: addressing, ACK, retry, duplicate suppression,
   backoff and TX scheduling.
3. **BoomProtocol** — typed application messages defined in Protocol Buffers and
   encoded on STM32 by Nanopb.

The first implementation is **not a mesh network**. Nodes communicate directly over
LoRa P2P. Routing/relaying may be added later without changing the application message
schema.

---

## 1. Goals

The first production-quality architecture should provide:

- one firmware image for all nodes;
- runtime-configurable node identity and behaviour;
- direct P2P LoRa communication between STM32 nodes;
- one or more nodes optionally acting as a USB gateway/monitor;
- typed, versionable messages generated from `.proto` files;
- detection-event reporting without transporting raw audio over LoRa;
- remote commands and runtime detection-parameter changes;
- ACK/retry for messages that require delivery confirmation;
- duplicate suppression so retransmissions are safe;
- broadcast messages where appropriate;
- deterministic memory use suitable for embedded firmware;
- host-side decoding and protocol tests using the same `.proto` definitions;
- an architecture that can later support authentication, encryption, CAD, TDMA or
  mesh routing without rewriting the application protocol.

### 1.1 Design assumptions

The first deployment target is:

- **5–10 nodes** within a few hundred metres of each other;
- nodes powered from a ~4000 mAh battery with an endurance requirement of **hours**
  (field test), not weeks;
- always-on RX is therefore acceptable; low-power duty-cycled listening is out of scope
  until a longer-endurance deployment is defined;
- traffic is event-driven and low rate (detections, occasional commands, periodic
  telemetry), far below the airtime available in the selected band, so SF7–SF9 profiles
  are expected to be sufficient.

Queue depths, duplicate-cache sizes and the retry policy are sized for this scale, not
for large networks.

Time synchronization (GNSS/1PPS discipline, timer capture) is a **separate work track**
and deliberately outside this communication roadmap. Detection consumes whatever time
base is available and reports its source and uncertainty (section 12.2).

## 2. Non-goals for the first implementation

Do **not** add these to the initial BoomLink implementation:

- audio recording transfer over LoRa;
- multi-hop routing or mesh;
- LoRaWAN;
- dynamic memory allocation for protocol messages;
- automatic frequency hopping;
- a complex distributed network coordinator;
- OTA firmware update over LoRa;
- a second firmware variant for a gateway/master node;
- low-power duty-cycled receive or battery-lifetime optimization (see design
  assumptions in section 1.1).

Raw/bulk audio remains on the existing USB path. LoRa carries event metadata,
telemetry, configuration and commands.

---

## 3. Layered architecture

```text
Application
│
├── Detection subsystem
├── Telemetry subsystem
├── Runtime configuration
└── Command handlers
        │
        ▼
Message services
│
├── DetectionService
├── TelemetryService
├── ConfigService
├── CommandService
└── SystemService
        │
        ▼
BoomProtocol
│
├── Envelope + MessageHeader
├── Protobuf schemas
├── Nanopb codec
└── protocol dispatcher
        │
        ▼
BoomLink
│
├── link frame header codec
├── node addressing
├── sequence/session tracking
├── ACK handling
├── retry policy
├── duplicate suppression
├── randomized backoff
└── priority TX queue
        │
        ▼
Radio abstraction
        │
        ▼
RadioLib
        │
        ▼
STM32 RadioLib HAL adapter
        │
        ▼
SPI1 + GPIO/EXTI
        │
        ▼
EBYTE E22-900M22S / SX1262
```

The dependency direction is one-way. Application code must not call RadioLib directly,
and the radio layer must not know anything about Protobuf message types.

### 3.1 Naming

- **BoomProtocol**: application-level message contract and serialization.
- **BoomLink**: LoRa P2P link layer carrying encoded BoomProtocol envelopes inside a
  small fixed binary link frame (section 7.3). BoomLink never decodes Protobuf.
- **Radio**: thin hardware/radio abstraction implemented with RadioLib.

This distinction matters because BoomProtocol should later be usable over USB or another
transport without inheriting LoRa ACK/retry behaviour.

---

## 4. Proposed repository layout

The `.proto` files are shared protocol definitions and must not live only inside the
STM32 target. The STM32-specific radio/link implementation stays in `bom-stm32node`.

```text
fw/
├── common/
│   └── boomlink/
│       ├── proto/
│       │   ├── common.proto
│       │   ├── header.proto
│       │   ├── envelope.proto
│       │   ├── detection.proto
│       │   ├── config.proto
│       │   ├── command.proto
│       │   ├── telemetry.proto
│       │   └── system.proto
│       ├── nanopb/
│       │   └── boomlink.options
│       ├── linkframe/
│       │   ├── linkframe.md        # link frame header spec (section 7.3)
│       │   └── boomlink_frame.h    # shared C header, mirrored by host parser
│       ├── tests/
│       │   ├── test_encode_decode.py
│       │   ├── test_compatibility.py
│       │   └── vectors/
│       ├── CMakeLists.txt
│       └── README.md
│
├── bom-stm32node/
│   ├── Core/                  # CubeMX/HAL and existing code
│   ├── App/
│   │   ├── radio/
│   │   │   ├── radio.h
│   │   │   ├── radio.cpp
│   │   │   ├── e22_radio.cpp
│   │   │   ├── stm32_radiolib_hal.hpp
│   │   │   └── stm32_radiolib_hal.cpp
│   │   ├── link/
│   │   │   ├── boomlink.h
│   │   │   ├── boomlink.c
│   │   │   ├── boomlink_tx.c
│   │   │   ├── boomlink_rx.c
│   │   │   └── boomlink_queue.c
│   │   ├── protocol/
│   │   │   ├── protocol_codec.h
│   │   │   ├── protocol_codec.c
│   │   │   ├── protocol_dispatcher.h
│   │   │   ├── protocol_dispatcher.c
│   │   │   ├── envelope_builder.h
│   │   │   └── envelope_builder.c
│   │   ├── services/
│   │   │   ├── detection_service.c/.h
│   │   │   ├── telemetry_service.c/.h
│   │   │   ├── config_service.c/.h
│   │   │   ├── command_service.c/.h
│   │   │   └── system_service.c/.h
│   │   └── storage/
│   │       └── config_store.c/.h
│   ├── third_party/
│   │   ├── embedded-cli/
│   │   ├── RadioLib/
│   │   └── nanopb/
│   └── tests/
│       ├── protocol/
│       ├── link/
│       └── services/
│
└── apps/
    └── stm32node-cli/
```

The exact split of small `.c` files may evolve, but the layer boundaries should remain.

### Repository rules

- `.proto` files are the source of truth for the application wire format.
- Generated `.pb.c` / `.pb.h` files are generated during the build and are not edited
  manually.
- Third-party dependencies are pinned to reviewed versions; builds must not silently
  consume moving `master` branches. This applies retroactively: the already-vendored
  `embedded-cli` has no recorded version/commit and one must be added.
- The link frame header layout is defined once under `fw/common/boomlink/linkframe/`
  (spec + shared C header) and mirrored by a small parser in the host CLI.
- New application subsystems go under `App/`; avoid growing CubeMX-generated files with
  protocol logic.
- RadioLib C++ stays behind a small C-facing radio interface. The rest of the firmware
  does not need to become C++.

---

## 5. Dependencies

### 5.1 RadioLib

RadioLib provides the SX1262 driver and LoRa PHY operations. The STM32 port implements
RadioLib's platform HAL using STM32 HAL primitives.

Responsibilities of the STM32 RadioLib adapter:

- SPI transfers;
- GPIO read/write;
- pin mode;
- timing/delays;
- interrupt attach/detach;
- RadioLib-facing platform services only.

The higher layers must use `radio.h`, not RadioLib classes directly.

### 5.2 Nanopb

Nanopb is the embedded Protocol Buffers runtime.

Rules:

- no protocol `malloc` in the target implementation;
- size strings, bytes and repeated fields in `.options`;
- prefer bounded integer representations over large strings/floats where practical;
- reject an encoded envelope that exceeds the configured radio payload limit;
- use the same `.proto` files for host-side generated Python classes and tests.

---

## 6. STM32 / E22 radio integration

The current board routes the E22/SX1262 to SPI1 and exposes:

- `LORA_NSS`;
- `LORA_SCK`;
- `LORA_MISO`;
- `LORA_MOSI`;
- `LORA_NRST`;
- `LORA_BUSY`;
- `LORA_DIO1`;
- `LORA_DIO2`;
- `LORA_RXEN`;
- `LORA_TXEN`;
- `EN_LORA`.

Before RadioLib bring-up, SPI1 must be configured for the SX1262 transport:

- master mode;
- 2-line SPI;
- 8-bit data;
- CPOL low;
- CPHA first edge;
- MSB first;
- software-controlled NSS;
- SPI clock at or below **16 MHz** (SX1262 maximum; with the 125 MHz SPI1 kernel clock
  this means prescaler ≥ 8 → 15.625 MHz);
- `LORA_NSS` controlled as GPIO by the radio adapter.

The current CubeMX-generated configuration is wrong in **three** ways and all three must
change: 4-bit data size, hardware-input NSS, and prescaler 2 (62.5 Mbit/s).

`LORA_DIO1` is configured as an EXTI pin, but the EXTI interrupt is **not enabled in the
NVIC and no handler exists** — interrupt-driven receive requires adding both. The radio
EXTI priority must be numerically higher (less urgent) than the audio GPDMA interrupt
(priority 5) so radio activity can never cause a microphone overrun.

E22-900M22S module specifics that must be handled during bring-up:

- the module's **TCXO is powered from SX1262 DIO3** — RadioLib must be configured with
  the correct TCXO voltage or the radio will not transmit/receive on frequency;
- `LORA_RXEN`/`LORA_TXEN` drive the RF switch and must follow RX/TX state;
- `EN_LORA` powers the module and `LORA_NRST` resets it; the power-enable and reset
  timing sequence must be respected before the first SPI access;
- use a **private LoRa sync word** (not the public/LoRaWAN one) so foreign LoRa traffic
  is mostly rejected at PHY level.

DIO interrupts must only signal deferred work. Do not perform Protobuf decoding, USB
printing or long radio operations inside the ISR.

The basic IRQ flow is:

```text
DIO1 EXTI
   │
   ▼
set radio event flag
   │
   ▼
return from ISR
   │
   ▼
main/task context
   │
   ▼
RadioLib reads IRQ status + packet
   │
   ▼
BoomLink RX processing
```

### 6.1 Band plan and regulatory constraints (CZ/EU)

The E22-900M22S covers 850–930 MHz and up to +22 dBm, which is wider and stronger than
what is legal in the Czech Republic / EU (863–870 MHz SRD band). BoomLink v1 uses:

- centre frequency inside **869.4–869.65 MHz** (the ERC 70-03 sub-band with the most
  permissive limits: up to **500 mW ERP** and **10 % duty cycle**);
- TX power configured so that radiated power stays within 500 mW ERP (module output
  plus antenna gain must be accounted for);
- an antenna tuned for the 868/869 MHz band.

Consequences for the implementation:

- the default radio profile must not allow out-of-band frequencies or power above the
  legal limit without an explicit lab/experimental override;
- BoomLink statistics include a cumulative TX airtime counter so duty-cycle compliance
  can be verified during tests; automatic duty-cycle enforcement may be added later if
  measurements show it is needed at this scale.

### 6.2 Execution model

The firmware is a bare-metal cooperative superloop (no RTOS) and BoomLink must fit that
model:

- DIO1 EXTI sets a flag and returns; all radio, link and protocol processing runs in
  the main loop (a `boomlink_process()` service function called from the superloop);
- transmission is initiated from the main loop (for example, the detection subsystem
  calls a send function after an event); nothing transmits from interrupt context;
- ACK timeouts, retry backoff and TX jitter use soft timers derived from the HAL 1 ms
  tick — millisecond resolution is sufficient for ACK timeouts in the hundreds of
  milliseconds;
- ACK timeout values must include worst-case main-loop iteration latency.

Known limitation: the `stream N` USB audio command blocks the main loop for the whole
stream duration. While a stream is active the radio is not serviced — RX packets may be
missed and ACK timeouts may expire. This is accepted for the MVP because streaming is a
development/diagnostic feature; document it in the CLI help and revisit if streaming
becomes an operational mode.

---

## 7. BoomProtocol message model

Every application message is encoded as one `Envelope`.

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

Message groups are intentionally coarse. A new detection message does not add another
field to the top-level envelope; it is added inside `DetectionMessage`.

### 7.1 Message header

The Protobuf header carries **application-level concerns only**. Addressing, link
identity and ACK signalling live in the fixed binary link frame header (section 7.3),
not in Protobuf.

```protobuf
message MessageHeader {
  uint32 protocol_version = 1;
  uint32 request_id = 2;
}
```

Field semantics:

- `protocol_version`: BoomProtocol compatibility version, initially `1`;
- `request_id`: correlates request/response at the application level; zero when unused.

Services that need the sender identity (gateway forwarding, response routing) receive
it as RX metadata passed alongside the decoded Envelope — it is not duplicated inside
the Protobuf payload.

Do not duplicate message category in the header. Protobuf `oneof` already identifies
the payload category.

### 7.2 Address space

Addresses are carried in the link frame header (section 7.3). Initial addressing rules:

```text
0x00000000  invalid / unconfigured node
0x00000001..0xFFFFFFFE  normal node IDs
0xFFFFFFFF  broadcast
```

A node accepts a packet when:

```text
destination_id == local_node_id
OR
destination_id == 0xFFFFFFFF
```

Promiscuous monitoring is a separate runtime/debug mode and must not change normal
address acceptance rules.

There is no special "master" address. A USB gateway is simply a normally addressed
node with gateway/forwarding behaviour enabled in runtime configuration.

### 7.3 Packet framing — link frame

For LoRa P2P v1 every LoRa packet is one BoomLink frame:

```text
+--------------------+----------------------------------+
| link frame header  | serialized Protobuf Envelope     |
| (fixed binary)     | (Nanopb; DATA frames only)       |
+--------------------+----------------------------------+
```

The link frame header is a small fixed-layout binary structure owned by BoomLink. It is
deliberately **not Protobuf**: BoomLink must be able to filter, acknowledge and
deduplicate packets without invoking Nanopb, and foreign traffic must be rejectable by
inspecting a few leading bytes. This is the only hand-packed binary structure on the
LoRa air interface.

Initial layout (little-endian, 20 bytes):

```text
offset  size  field
0       1     magic / network ID        (runtime-configurable, default e.g. 0xB0)
1       1     version (high nibble) | frame type (low nibble)
2       1     flags                     (bit 0: ack_requested)
3       1     reserved (0)
4       4     destination_id
8       4     source_id
12      4     session_id
16      4     sequence
```

Frame types:

```text
DATA  1   header + serialized Envelope payload
ACK   2   header only, no payload (section 9.5)
```

Rules:

- a packet whose magic/network ID or version does not match is dropped and counted
  before any further processing;
- LoRa PHY CRC is enabled; the link frame adds no CRC of its own;
- `session_id + sequence` (scoped to `source_id`) is the unique link-level packet
  identity; `session_id` is generated randomly at boot so packets after a reboot are
  not treated as duplicates when the sequence counter restarts.

No fragmentation is implemented in the MVP. An oversized frame is rejected before
transmission.

Do not design application messages that depend on filling the radio's theoretical
maximum payload. Keep normal messages small to preserve airtime and reliability.

Bulk audio is explicitly outside BoomLink.

---

## 8. Message groups

### 8.1 Detection

```protobuf
message DetectionMessage {
  oneof message {
    DetectionEvent event = 1;
    DetectionStatus status = 2;
    DetectionStatistics statistics = 3;
  }
}
```

`DetectionEvent` carries metadata, not audio. Initial event types should cover:

- drone;
- gunshot;
- explosion;
- other/unknown.

Useful common fields include:

- event ID;
- detection timestamp;
- timestamp source and uncertainty;
- detection type;
- confidence;
- local GNSS position when valid;
- estimated azimuth/elevation when available;
- detector/model identifier and version;
- type-specific compact metadata.

For location and direction prefer bounded integer units where practical, for example
latitude/longitude in `1e-7` degrees and angle in centidegrees.

### 8.2 Configuration

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

`NodeConfig` should be split by subsystem:

```text
NodeConfig
├── GeneralConfig
├── LinkConfig
├── RadioConfig
├── DetectionConfig
│   ├── DroneDetectionConfig
│   └── GunshotDetectionConfig
├── GnssConfig
└── TelemetryConfig
```

Configuration is persistent state. Commands are immediate actions.

Examples:

```text
set gunshot threshold       -> configuration
set telemetry interval      -> configuration
set default destination     -> configuration
reboot now                  -> command
run self-test               -> command
identify this unit          -> command
```

Configuration updates must use a monotonically increasing `config_version` and support
optimistic concurrency through `expected_config_version`.

A remote write with an outdated expected version is rejected instead of silently
overwriting newer settings.

Radio settings that would break the current link require special apply semantics. They
must not be applied before the response confirming the change has been transmitted.
Because that response can itself be lost, apply must use **revert-on-timeout**: the
node applies the new radio profile, waits for a confirmation exchange on the new
profile within a bounded window, and reverts to the previous profile if none arrives.
This prevents stranding a remote node on a profile nobody else uses.
A later implementation may add scheduled activation for coordinated network-wide radio
profile changes.

### 8.3 Commands

Initial command set:

- reboot;
- identify (LED/debug indication);
- run self-test;
- start detection;
- stop detection;
- clear statistics;
- request diagnostics;
- optional time-sync fallback.

Commands use `request_id` and return a correlated response with a typed result/error
code.

### 8.4 Telemetry

Telemetry should stay compact and low priority. Candidate fields:

- uptime;
- supply/battery voltage;
- MCU/board temperature;
- GNSS fix state and accuracy;
- last RX RSSI/SNR;
- TX/RX packet counters;
- radio error counters;
- detection count;
- reset/error counters.

Telemetry interval is runtime configuration. Telemetry must never delay a pending
DetectionEvent or command response.

### 8.5 System messages

System messages are protocol housekeeping and node state, for example:

- ping/pong;
- boot/ready status;
- protocol error;
- firmware/hardware version information.

ACK is **not** a system message — it is a link frame type handled entirely inside
BoomLink (section 9.5).

---

## 9. BoomLink specification

BoomLink operates on link frames: a fixed binary header plus an opaque serialized
Envelope payload (section 7.3). Everything BoomLink needs — addressing, packet
identity, ACK signalling — is in the frame header. BoomLink never decodes the Protobuf
payload and has no Nanopb dependency.

### 9.1 TX pipeline

```text
Application message
      │
      ▼
BoomProtocol builds Envelope
      │
      ▼
Nanopb encode
      │
      ▼
BoomLink TX queue
      │
      ├── enforce destination rules
      ├── select priority
      └── queue encoded envelope
      │
      ▼
dequeue for transmission
      │
      ├── assign session/sequence   (at dequeue, so the on-air
      │                              sequence stays monotonic even
      │                              when priorities reorder the queue)
      ├── build link frame header
      └── track ACK state when required
      │
      ▼
Radio send
```

A retransmission reuses the already-assigned `(session_id, sequence)`; only the first
transmission assigns a new sequence number.

### 9.2 RX pipeline

```text
Radio packet
    │
    ▼
BoomLink RX
    │
    ├── validate magic/version + frame length
    ├── match ACK frames against the pending TX
    ├── validate destination
    ├── duplicate check
    ├── generate ACK when required
    └── update link statistics
    │
    ▼
BoomProtocol codec (Nanopb decode of the payload)
    │
    ▼
Protocol dispatcher
    │
    └── target service
```

Malformed packets are dropped and counted — a bad link header at the BoomLink layer, a
failed Protobuf decode at the BoomProtocol layer. Neither may reach application
handlers, and the two failure classes are counted separately.

### 9.3 Sequence and session

Each node maintains:

```text
session_id  generated once per boot
sequence    incremented for each transmitted envelope
```

The receiver identifies a packet by:

```text
(source_id, session_id, sequence)
```

Sequence wrap must be handled safely. A fresh `session_id` on reboot makes reboot
behaviour explicit.

### 9.4 Duplicate suppression

Retransmission can result in the same valid packet being received more than once.
Application handlers must see an ACKed message at most once.

For each recently active source/session, maintain a bounded duplicate window/cache.
Concrete initial shape:

- a statically allocated table of the N most recent `(source_id, session_id)` pairs
  (N = 16 is ample for 5–10 nodes);
- each entry tracks the highest accepted sequence plus a small bitmap window of recently
  accepted sequences below it (tolerates minor reordering);
- LRU eviction when the table is full. A very stale retransmission from an evicted
  source may be delivered twice — acceptable at this scale and traffic rate.

If a duplicate packet is received:

- do not dispatch it to the application again;
- if it requests ACK, transmit the ACK again;
- update duplicate statistics.

The cache must be statically bounded. No unbounded map or dynamic allocation.

### 9.5 ACK

ACK is a BoomLink delivery acknowledgement, not an application response.

A command may therefore produce both:

1. ACK — "the packet was received";
2. CommandResponse — "the command was executed and this was the result".

ACK is a **link frame type** (section 7.3), not a Protobuf message. An ACK frame has no
payload; it identifies the original packet by reusing the header fields:

```text
frame type      = ACK
destination_id  = original source_id
source_id       = acknowledging node
session_id      = original packet's session_id
sequence        = original packet's sequence
ack_requested   = 0
```

Duplicate ACK frames are harmless and need no duplicate suppression.

Rules:

- ACK packets never request another ACK;
- broadcast packets never request ACK;
- receiving a valid duplicate of an ACK-requested packet causes the ACK to be resent;
- ACK confirms link delivery only, not semantic success of the payload;
- application request/response correlation uses `request_id`, not sequence number.

### 9.6 Retry

BoomLink v1 is **stop-and-wait**: at most one ACK-pending frame is outstanding at any
time, globally. While waiting for an ACK the TX queue is held — the radio is
half-duplex, and transmitting another frame during the ACK wait window would prevent
hearing the ACK. Frames that do not request ACK simply wait in the queue behind the
pending one. At this network's traffic rate the throughput cost is irrelevant and the
state machine stays trivial.

For unicast packets with `ack_requested = true`:

```text
TX
 │
 ▼
wait for matching ACK
 │
 ├── ACK received -> success
 │
 └── timeout
       │
       ▼
   randomized backoff
       │
       ▼
      retry
```

Initial policy:

- bounded retry count, runtime-configurable;
- default target: 3 total transmission attempts;
- ACK timeout derived from/configured for the active radio profile rather than assuming
  one fixed timeout for every spreading factor and packet size;
- randomized retry backoff;
- retransmission uses the **same** `(session_id, sequence)` so the receiver can suppress
  duplicate delivery;
- final failure is surfaced to the caller and counted in link statistics.

Do not retry forever.

### 9.7 Random backoff and simultaneous detections

Gunshots or other common acoustic events may be detected by several nodes at almost the
same time. If every node transmits immediately, collision probability is high.

The link layer therefore supports configurable randomized TX jitter for event messages.
The original detection timestamp is captured before this delay, so localization timing
is not changed by radio scheduling.

Retry backoff must also include jitter. Later MAC improvements may add CAD or time slots,
but they are not required for the first prototype.

### 9.8 Priority TX queue

At minimum use three logical priorities:

```text
HIGH    ACK, command response, critical system message
NORMAL  detection events, configuration responses
LOW     periodic telemetry, non-critical diagnostics
```

The exact mapping can be adjusted, but low-priority telemetry must not block urgent
traffic.

The queue is statically bounded. When full, the drop policy should prefer dropping or
coalescing low-priority telemetry before detection or command traffic.

Priorities reorder only the queue. Sequence numbers are assigned at dequeue (section
9.1), so the on-air sequence remains monotonic per session regardless of priority
reordering, and the receiver's duplicate window stays simple.

### 9.9 Broadcast

Broadcast destination:

```text
0xFFFFFFFF
```

Rules:

- all normally receiving nodes may accept it;
- broadcast never requests link ACK;
- commands that are dangerous when broadcast should be rejected by the application
  service unless explicitly designed for broadcast;
- a broadcast ConfigSet must not be added casually because simultaneous responses and
  coordinated radio-profile changes require a separate design.

### 9.10 Link statistics

Expose at least:

- TX envelopes;
- RX envelopes;
- TX retries;
- TX failures;
- RX duplicates;
- malformed packets;
- packets ignored for another destination;
- ACK sent/received;
- packets rejected by magic/network ID or version;
- cumulative TX airtime (for duty-cycle verification, section 6.1);
- last RSSI;
- last SNR.

---

## 10. Runtime node behaviour

Every board uses the same firmware image.

Example runtime configuration:

```text
GeneralConfig
├── node_id
├── default_destination_id
├── receive_enabled
├── transmit_enabled
├── usb_forward_enabled
└── promiscuous_monitor_enabled (debug only)
```

A normal sensor node may be:

```text
node_id                 = 17
default_destination_id  = 100
receive_enabled         = true
transmit_enabled        = true
usb_forward_enabled     = false
```

A USB gateway may use the same binary with:

```text
node_id                 = 100
receive_enabled         = true
transmit_enabled        = true
usb_forward_enabled     = true
```

There is no compile-time `MASTER` firmware.

### 10.1 Persistent configuration

Persist `NodeConfig` in internal flash using a small storage wrapper around the Nanopb
blob, for example:

```text
magic
storage_format_version
protobuf_length
CRC
serialized NodeConfig
```

Boot behaviour:

```text
load stored config
   │
   ├── valid -> validate -> apply
   │
   └── missing/invalid -> load safe defaults
```

A configuration write must be validated before it becomes active or persistent.

---

## 11. USB integration

Do not replace the existing `PCM1` USB audio streaming protocol just because BoomProtocol
exists. Audio is a bulk data path and has different requirements.

The USB interface may expose both:

```text
USB CDC
├── existing human-readable CLI / PCM1 audio stream
└── BoomProtocol control/event framing (later or alongside CLI)
```

The host CLI should eventually use generated Protobuf classes for machine-readable
control and event messages.

A gateway with `usb_forward_enabled` forwards received application envelopes/events to
the host without changing their semantic content. Each forwarded envelope is wrapped in
a small host-link frame carrying RX metadata — source node ID, RSSI, SNR and a receive
timestamp — because the host needs link quality per event and the envelope itself no
longer contains addressing. LoRa link ACK/retry metadata remains a radio-link concern
and is not blindly replayed over USB.

Note on the current USB implementation: CLI text and PCM1 audio share **one** CDC bulk
IN endpoint via time-division multiplexing (the host resynchronizes on the `PCM1`
magic). Adding BoomProtocol framing to the same pipe requires an explicit framing
design (length-prefixed frames with a magic) or a second CDC interface. This decision
belongs to PR 5 and must not be improvised.

---

## 12. Detection-specific requirements

Detection is the primary application use case.

### 12.1 Do not send audio over BoomLink

Normal LoRa messages carry only compact metadata. If a node stores an audio snapshot,
the event may contain a local recording/storage identifier and metadata describing the
recording. The recording itself is retrieved over USB/SD/another future high-bandwidth
transport.

### 12.2 Timestamp quality

Detection events should represent timestamp quality explicitly, because localization
algorithms need to distinguish GNSS/1PPS-quality timing from degraded local timing.

Recommended model:

```text
Timestamp
├── value_us
├── source
└── uncertainty_us
```

Candidate sources:

- GNSS/1PPS disciplined;
- local RTC/timer;
- network fallback.

Transmission time is not the detection timestamp.

---

## 13. Error handling

Use typed result codes for command/config responses. Do not make host software parse
human-readable error strings to determine behaviour.

Candidate result classes:

```text
OK
INVALID_ARGUMENT
NOT_SUPPORTED
BUSY
CONFLICT
UNAUTHORIZED
INTERNAL_ERROR
LINK_UNAVAILABLE
```

An optional bounded diagnostic string may accompany a result for humans, but the enum
is authoritative.

Protocol decode errors, link delivery errors and application execution errors are
separate concepts and should remain separate in counters/logging.

---

## 14. Security boundary

Raw LoRa P2P does not by itself make remote configuration/commands trusted.

For lab bring-up, BoomLink may initially operate without application-layer encryption,
but **unauthenticated remote control is not acceptable for field deployment**.

Before field use, add message authentication and replay protection; encryption should be
added where confidentiality is required. The security layer should protect the serialized
BoomProtocol payload while preserving the layering above.

Security is intentionally a separate implementation PR so it does not block the first
radio bring-up, but it is a deployment requirement, not an optional polish item.

---

## 15. Testing strategy

The protocol and link behaviour must be testable without RF hardware.

### 15.1 Protocol tests

Native/host tests must cover:

- encode -> decode round-trip for every message group;
- maximum bounded field sizes;
- malformed/truncated input;
- unknown fields for forward compatibility;
- old golden vectors decoding with the current schema;
- Python Protobuf encode -> Nanopb decode;
- Nanopb encode -> Python Protobuf decode.

Store representative golden packets under:

```text
fw/common/boomlink/tests/vectors/
```

Never reuse a removed Protobuf field number. Mark removed numbers/names as `reserved`.

### 15.2 BoomLink tests with a fake radio

The link layer should depend on a small radio interface so a fake backend can test:

- link frame header encode/parse round-trip;
- rejection of frames with wrong magic/network ID or version;
- unicast delivery;
- wrong destination rejection;
- broadcast acceptance;
- ACK matching;
- ACK timeout;
- retry count;
- duplicate suppression;
- duplicate ACK resend;
- sequence/session behaviour across reboot simulation;
- queue priority;
- queue overflow policy;
- randomized backoff bounds;
- malformed packet rejection.

### 15.3 Hardware integration tests

At minimum keep repeatable two-board tests for:

1. raw RadioLib ping/pong;
2. BoomLink unicast + ACK;
3. forced lost ACK -> retry -> no duplicate application delivery;
4. broadcast without ACK storm;
5. DetectionEvent from node to gateway;
6. ConfigGet/ConfigSet round trip;
7. runtime role change without rebuilding firmware;
8. USB gateway output.

Record RSSI/SNR and retry counters so RF issues can be separated from protocol issues.

---

## 16. Agent implementation rules

Agents implementing this design should follow these constraints unless the architecture
is deliberately revised in a separate PR:

1. Do not create separate master/gateway and sensor firmware variants.
2. Do not add a custom hand-packed application struct when the data belongs in
   BoomProtocol. The only hand-packed binary structure on the air interface is the
   BoomLink frame header defined in section 7.3.
3. Do not bypass `radio.h` to call RadioLib from application services.
4. Do not put ACK/retry logic into detection/config handlers.
5. Do not perform heavy work in DIO/EXTI callbacks.
6. Do not use unbounded dynamic allocation in protocol/link code.
7. Do not add fragmentation in the MVP.
8. Do not send raw audio over BoomLink.
9. Do not implement mesh routing in the MVP.
10. Do not reuse Protobuf field numbers.
11. Add tests for every protocol/link behaviour introduced by a PR.
12. Keep each implementation PR focused on one layer/milestone and preserve the layer
    boundaries defined here.

---

# 17. Implementation roadmap

Each milestone below should be a separate, reviewable PR. Later PRs may depend on earlier
ones, but avoid mixing unrelated application features into radio bring-up.

## PR 1 — SX1262 / RadioLib bring-up

**Goal:** prove reliable raw P2P RF communication between two `bom-stm32node` boards.

Scope:

- fix SPI1 configuration for SX1262 — three changes: 8-bit data size (currently
  4-bit), software NSS (currently hardware input), clock ≤ 16 MHz (currently
  prescaler 2 = 62.5 Mbit/s);
- add DIO1 EXTI NVIC enable and IRQ handler (the pin is configured for EXTI but the
  interrupt is currently neither enabled nor handled); priority less urgent than the
  audio GPDMA interrupt;
- enable C++ in the STM32 CMake project (`enable_language(CXX)`; the toolchain file is
  already prepared) while keeping the existing firmware C code;
- pin/vendor RadioLib under `third_party/` with a recorded version;
- implement `stm32_radiolib_hal`;
- implement the E22 radio adapter including EN_LORA power-up and NRST reset sequencing,
  BUSY handling, RXEN/TXEN RF-switch control and **TCXO configuration (DIO3)**;
- configure a private LoRa sync word;
- expose a small C-facing `radio.h` API;
- add minimal debug/CLI commands for radio status and raw ping/pong;
- document the tested LoRa PHY profile and its compliance with the 869.4–869.65 MHz
  band limits (section 6.1).

Acceptance criteria:

- two physical boards can exchange raw packets in both directions;
- DIO-driven receive works without polling-only hacks;
- RSSI and SNR are observable;
- no RadioLib types leak into application code;
- existing microphone/USB build remains functional.

Not in scope: Protobuf, ACK/retry, runtime network config.

## PR 2 — BoomProtocol + Nanopb foundation

**Goal:** establish the shared typed wire protocol and reproducible code generation.

Scope:

- add pinned Nanopb dependency;
- create `fw/common/boomlink/proto/`;
- add `common.proto`, `header.proto`, `envelope.proto`, `system.proto`;
- define `MessageHeader`, `Envelope`, `SystemMessage` and Ping/Pong schema (ACK is a
  link frame, not Protobuf — it belongs to PR 3);
- add Nanopb `.options` with bounded fields;
- create the native/host test build (dual-target CMake for host `gcc` + unit tests) —
  **no host-side C test harness exists today**, this PR creates the infrastructure;
- integrate `.proto` -> Nanopb generation into build/tasks;
- generate host Python Protobuf classes for tests without committing generated target
  code unnecessarily;
- add native encode/decode and golden-vector tests;
- add protocol compatibility rules to README/docs.

Acceptance criteria:

- STM32 build can encode/decode an Envelope using Nanopb;
- Python and Nanopb interoperate on the same test vectors;
- no dynamic allocation is required for normal protocol messages;
- CI detects schema/code-generation breakage.

Not in scope: reliable radio delivery or application detection/config messages.

## PR 3 — BoomLink P2P reliability MVP

**Goal:** turn raw RadioLib packets into a usable addressed P2P link.

Scope:

- define the fixed binary link frame header (magic/network ID, version, frame type,
  flags, addressing, session/sequence) as spec + shared C header under
  `fw/common/boomlink/linkframe/`, with a host Python parser;
- implement runtime `node_id` and destination filtering;
- implement `session_id` and monotonically increasing `sequence` assigned at dequeue;
- implement bounded duplicate suppression;
- implement unicast ACK as a link frame type;
- implement stop-and-wait delivery (single outstanding ACK-pending frame);
- implement bounded retry and ACK timeout;
- implement randomized retry backoff;
- implement broadcast with no ACK;
- implement bounded priority TX queue;
- expose BoomLink statistics;
- add fake-radio/native tests for all behaviours;
- expose ping/pong over BoomLink on hardware.

Acceptance criteria:

- addressed unicast works between two nodes;
- lost ACK causes retransmission;
- retransmission does not deliver the application message twice;
- broadcast causes no ACK storm;
- retry terminates after the configured maximum;
- link logic can be tested without STM32/RF hardware.

Not in scope: mesh, CAD/TDMA, detection payloads, persistent config.

## PR 4 — Detection, telemetry, command and configuration API

**Goal:** make BoomLink useful for the actual sensor-node application while keeping one
firmware image for every node.

Scope:

- add `detection.proto`;
- add `telemetry.proto`;
- add `command.proto`;
- add `config.proto`;
- implement protocol dispatcher and per-domain services;
- implement DetectionEvent with timestamp quality, type, confidence and optional compact
  localization metadata;
- implement compact telemetry;
- implement Reboot, Identify and SelfTest commands;
- implement ConfigGet and ConfigSet;
- add `config_version` / `expected_config_version` conflict handling;
- add runtime general/link/radio/detection/telemetry configuration;
- persist validated NodeConfig to flash with CRC/version wrapper;
- implement safe defaults when stored config is missing/corrupt;
- support `usb_forward_enabled` gateway behaviour without a separate firmware build.

Acceptance criteria:

- the same binary can boot as different node IDs/roles from persistent config;
- a detector event reaches a gateway as a typed message;
- detection parameters can be changed at runtime and persisted;
- ConfigGet reports the active configuration;
- stale ConfigSet is rejected using config versioning;
- reboot does not require rebuilding to preserve role/identity.

## PR 5 — Host CLI and end-to-end tooling

**Goal:** make development, testing and field diagnostics convenient from a PC.

Scope:

- integrate generated Python Protobuf classes into `fw/apps/stm32node-cli`;
- retain existing PCM1 audio streaming;
- add machine-readable BoomProtocol framing on USB or a clearly separated CLI bridge —
  including the explicit decision how it coexists with the current single-endpoint
  CLI/PCM1 time-division multiplexing (section 11);
- define the gateway-to-host forwarding wrapper carrying source node ID, RSSI/SNR and
  receive timestamp alongside the forwarded envelope;
- add commands such as:

```text
node status
node config get
node config set ...
node identify
node self-test
radio status
radio ping <node-id>
radio monitor
```

- display source, RSSI/SNR, retries and message type for received events;
- add end-to-end tests using recorded/golden frames;
- document common bring-up/debug workflows.

Acceptance criteria:

- a developer can configure a node without rebuilding firmware;
- a gateway can display DetectionEvents from another node over USB;
- protocol decoding in the host is generated from the same `.proto` schema;
- PCM audio streaming remains backward compatible unless deliberately versioned.

## PR 6 — Message authentication and field-security baseline

**Goal:** make remote commands/configuration acceptable outside a trusted lab.

Scope should be designed in a dedicated security review and include at least:

- authenticated messages;
- replay protection compatible with session/sequence handling;
- key provisioning/rotation strategy;
- authenticated command/config handling;
- encryption where confidentiality is required;
- negative tests for tampering and replay.

Do not invent a custom cryptographic primitive.

Acceptance criteria are defined by the security design/review before implementation.

## PR 7 — Optional MAC improvements, only if measurements justify them

Potential follow-up after the direct P2P system is measured with realistic node counts:

- Channel Activity Detection (CAD);
- smarter contention/backoff;
- time slots for synchronized deployments;
- message aggregation/coalescing;
- airtime-aware telemetry scheduling.

These changes belong below BoomProtocol and should not change Detection/Config/Command
schemas unless application semantics genuinely change.

## PR 8 — Optional multi-hop/mesh, only if required by deployment

Do not implement this pre-emptively.

If field measurements show direct node-to-gateway coverage is insufficient, evaluate a
routing layer above/beside BoomLink. The application should still send the same
BoomProtocol envelopes.

A mesh PR must define:

- routing identity versus application destination;
- TTL/hop limit;
- loop prevention;
- duplicate handling across relays;
- route discovery/maintenance;
- gateway selection;
- airtime/collision impact;
- interaction with ACK semantics.

---

## 18. Definition of the first usable prototype

The first useful milestone is reached after PR 4 when the following scenario works with
two or more physical boards:

```text
Sensor node
  detects event
      │
      ▼
DetectionEvent + timestamp
      │
      ▼
BoomProtocol / Nanopb
      │
      ▼
BoomLink unicast + ACK/retry
      │
      ▼
RadioLib / SX1262
      │
      ~~~ LoRa ~~~
      │
      ▼
Gateway node (same firmware)
      │
      ▼
USB
      │
      ▼
Host
```

At the same time the host/gateway can send a typed configuration or command message back
to the sensor node, and the change can take effect at runtime without rebuilding the
firmware.

That is the baseline. Mesh, high-level orchestration and advanced RF scheduling are
follow-up optimizations, not prerequisites for proving the node network.