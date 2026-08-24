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
and deliberately outside this communication roadmap. The message body carries the
precise timestamp of the detected event itself (section 12.2), so link delivery
latency — queueing, TX jitter, backoff, retries — has no effect on localization
timing. When and in what order the message arrives does not matter.

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

What the link frame actually landed as, where it differs from this section's original
sketch:

- The C header is `boomlink_linkframe.h`, not `boomlink_frame.h`, matching
  `boomlink_codec.h`'s naming.
- There is no separate `linkframe/linkframe.md`: section 7.3 below already *is* that
  specification, and a second copy beside the code would be free to drift from it.
- The sketch listed a header only; there is also a `boomlink_linkframe.c`, since the
  encode/parse logic has to live somewhere both the firmware and the host tests link.
- The host parser sits next to the C as `boomlink_linkframe.py`. Neither this section nor
  PR 3's scope entry said where it should live; here is chosen because PR 5's
  `stm32node-cli` will import it, so it is shipped code rather than a test helper.
- `boomlink_linkframe.h` is not purely C, and deliberately so. The encoder's output buffer
  is declared `uint8_t out[static 20]`, which turns an undersized caller buffer into a
  compile error - including the case that matters most, a short array *inside* a larger
  struct, where the overflow stays within a valid allocation and AddressSanitizer sees
  nothing. `[static N]` is not valid C++, and the caller that will actually encode frames
  is the C++ radio layer, so the header restores the same guarantee for C++ through a
  small template wrapper. Consequences worth knowing before writing that caller: the
  header must be included **bare**, never inside `extern "C" { }` (a template cannot have
  C language linkage, and the radio layer's existing `.cpp` files all wrap their C
  includes that way), and a C++ caller holding a bare `uint8_t *` rather than an array
  has to go through `boomlink_linkframe_detail::` explicitly.
- Both bounds are checked by the `boomlink_linkframe_c_bound` and
  `boomlink_linkframe_cxx_bound` tests, which compile a deliberately-undersized caller and
  require the compiler to reject it *for the right reason*, at more than one optimization
  level. They exist because no target in either build exercises either bound - every
  library and tool here is C and none passes a short buffer, and the C++ caller does not
  exist yet - so both could be deleted outright with the whole suite green. Verified.
- Section 9.5's ACK **field mapping** landed here rather than with the link engine, even
  though the rest of 9.5 did not. Reasoning and the split are recorded in section 9.5
  itself; PR 3's scope line "implement unicast ACK as a link frame type" is therefore
  partly done in this phase — the frame, not the delivery logic.
- The struct grew a `boomlink_linkframe_header_init()`. `boomlink_linkframe_header_t h =
  {0}` is a trap the encoder cannot refuse: magic 0, version 0 and frame type 0 are each
  invalid, so a zeroed header encodes 20 bytes no receiver will accept, and the encoder
  has no failure path by design. The Python reference had carried these three as dataclass
  defaults from the start, so the two implementations were not equally easy to misuse.

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
│       │   └── <name>.options      # one per .proto that needs bounds, auto-
│       │                           # discovered by name (avoids a spurious
│       │                           # Nanopb warning on every .proto that
│       │                           # doesn't need one - PR 2 started with a
│       │                           # single shared boomlink.options and hit
│       │                           # exactly that warning)
│       ├── linkframe/               # NO Nanopb dependency - see section 9
│       │   ├── boomlink_linkframe.h
│       │   ├── boomlink_linkframe.c
│       │   └── boomlink_linkframe.py  # independent host parser, not a binding
│       ├── tests/
│       │   ├── test_encode_decode.py
│       │   ├── test_compatibility.py
│       │   ├── test_linkframe.py
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

An Envelope with no `header` at all, or with `protocol_version == 0` (proto3's default
for a field that was never actually set), is malformed and must be dropped rather than
decoded - `fw/common/boomlink/boomlink_codec.c`'s `boomlink_decode_envelope()` already
enforces this at the shared codec layer, so no per-PR receiver code needs to repeat the
check.

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

The implementation adds one refinement the two rules above leave implicit: a node whose
**own** address is not in `0x00000001..0xFFFFFFFE` accepts nothing at all, broadcast
included. Both ends of that range matter. Without excluding `0x00000000`, a factory-fresh
node acts on traffic addressed to the invalid address, since `destination_id ==
local_node_id` holds when both are 0. Without excluding `0xFFFFFFFF`, a node misconfigured
to the broadcast address matches every broadcast frame and would answer on behalf of the
whole network. Acting on traffic before knowing who you are is the failure mode in both
cases.

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
2       1     flags                     (bit 0: ack_requested; bit 1: more_fragments)
3       1     fragment_index            (0)
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
transmission. `flags` bit 1 (`more_fragments`) and the `fragment_index` byte are
reserved wire-format space for a later PR to add fragmentation/reassembly without a
breaking header change - a sender that never fragments always sends `more_fragments =
0` and `fragment_index = 0`, indistinguishable from today's unfragmented frame.

`fragment_index` is normatively a **0-based ascending index of the fragment within the
message** (0, 1, 2, ... in send order) - not a "fragments remaining" countdown. This is
required, not stylistic: a "remaining" encoding would make the *last* fragment carry
`fragment_index = 0`, identical to `more_fragments = 0` on an unfragmented frame, which
is exactly the ambiguity the rule below exists to prevent. Whichever PR implements
fragmentation must follow this encoding.

Until fragmentation is implemented, every receiver must drop (not attempt partial
reassembly of) any frame where `more_fragments = 1` **or** `fragment_index != 0` -
checking `more_fragments` alone is not enough: a fragmented message's *last* fragment
correctly sets `more_fragments = 0` (no more follow) while `fragment_index` is
non-zero (per the ascending encoding above), and a receiver that only looks at
`more_fragments` would accept that fragment as if it were a complete, standalone
Envelope instead of the tail of one it never reassembled. Checking both fields together
is what keeps an old (non-fragmenting) receiver from misinterpreting fragmented traffic
from a newer sender.

`flags` bits 2-7 are likewise reserved and always 0 until a future PR assigns them, but
follow the opposite receiver rule from `more_fragments`/`fragment_index`: an unrecognized
bit among 2-7 must be **ignored**, not treated as a reason to drop the frame - ordinary
forward compatibility, unlike the fragmentation fields above, which are a deliberate
exception because misreading them risks handing a decoder a fragment instead of a whole
message. Any future bit that would change how the payload itself must be framed or
interpreted (i.e. behaves more like `more_fragments` than like an independent, ignorable
flag) must be gated behind the header's version nibble instead of added as a bare flag
bit, so an old receiver's default "ignore it" behavior can never be the unsafe choice.

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

A frame from a source already in the table but carrying a *different* `session_id` means
that peer rebooted (section 9.3), and its entry is **reused** rather than a second one
added: a peer that reboots a few times would otherwise evict unrelated peers with
sessions that are already dead. The cost is symmetrical with the eviction note above —
a straggler still in flight across the peer's reboot resets what the current session's
window remembers, so one already-delivered frame can be delivered twice. Both are the
same trade, and both are preferable to letting one rebooting peer occupy two slots.

If a duplicate packet is received:

- do not dispatch it to the application again;
- if it is **unicast** and requests ACK, transmit the ACK again — a broadcast frame is
  never acknowledged at all (section 9.5's receiver-side responsibilities), so the
  scoping here matches the rule list there rather than reading as a second, laxer one;
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

**Where this section is implemented.** It is split across two layers, deliberately, along
the same stateless/stateful line as the rest of section 9:

- The **field mapping above** is `boomlink_linkframe_make_ack()`, in the link frame layer
  (`fw/common/boomlink/linkframe/`). It is a pure function of one received header plus the
  local node ID, so it belongs with the header codec — and that layer is the only one
  whose tests can catch how it goes wrong. Every field is copied or moved, four are
  32-bit, and a transposition produces a structurally perfect ACK addressed to the wrong
  node or carrying a swapped `(session_id, sequence)`. On air that is an ACK timeout,
  retries and a TX failure, which gets diagnosed as an RF or timing fault. Crucially, an
  engine that both **builds** and **matches** ACKs can transpose a pair on both sides and
  pass every one of its own delivery tests while interoperating with nothing — so this
  mapping is pinned to a byte-exact vector taken from the field list above, not to a
  second implementation. Verified: transposing the pair in the C and the Python reference
  simultaneously still fails.
- The **rules below** belong to the link engine, because they need TX-pipeline and
  duplicate state. One exception: "ACK packets never request another ACK" is satisfied by
  construction in the mapping, since it clears the flag rather than relying on the engine
  to remember.

`make_ack()` deliberately does not decide *whether* to acknowledge — that is an engine
question, and there are two more of those than the rule list below spells out. Neither can
be answered in the frame layer, and both are load-bearing:

- **Do not acknowledge a frame addressed to the broadcast address.** This is the
  ACK-storm vector: one such frame with `ack_requested` set is answered by every
  *configured* node in range, and `make_ack()` builds each of those ACKs without
  complaint, because the frame's *source* is an ordinary unicast node and nothing about
  it is malformed. The rule "broadcast packets never request ACK" below is textually a
  constraint on senders, and a homogeneous network of compliant nodes satisfies PR 3's
  "broadcast causes no ACK storm" criterion on the sending side alone — so this is a
  proposed receiver-side hardening against a non-compliant or spoofed peer, not
  something the existing rules already mandate.
- **Do not acknowledge a frame accepted in promiscuous mode** — *if* promiscuous mode is
  defined to cross networks, which the spec does not currently say. The ACK echoes the
  received frame's magic rather than a configured one, which is correct for a deployment
  on a non-default network ID: defaulting would have its ACKs dropped by the sender on
  the magic check. But `parse()` takes the expected magic as a parameter, so a caller
  *can* pass a foreign one, and echoing there would inject an ACK into a deployment the
  node is not a member of. Section 7.2 defines promiscuous monitoring only in terms of
  address acceptance, and section 10 only names the `promiscuous_monitor_enabled` flag,
  so whether the mode also crosses network IDs is an open decision — and it decides
  whether this responsibility exists at all. Either way the frame layer cannot tell the
  two cases apart, having no configuration to compare against.

`make_ack()` does refuse one thing, on frame-validity grounds rather than policy: an ACK
whose either end would be the broadcast or unconfigured address, since section 7.2 makes
both something no node can *be*. Such an ACK is also unusable by anyone — the matching
rule below requires an ACK's `destination_id` to equal the receiving node's own ID, which
`0xFFFFFFFF` never does, so a compliant matcher discards it. Note this is **not** an
airtime defence: a broadcast-addressed ACK is a single 20-byte transmission, exactly a
unicast ACK's airtime. The storm case is the first bullet above and is not handled here.

**Open question for the link engine: where the ACK matcher lives.** The builder above is
pinned to this section's field list by a byte-exact test. Its counterpart — deciding
whether an arriving ACK acknowledges the frame currently awaiting one, section 9.2's
"match ACK frames against the pending TX" — does not exist yet.

Note first what does *not* argue for pinning it. The build/match cancel-out that put the
builder here — misread the same field pair on both sides, and every self-consistent
delivery test still passes — cannot happen to a matcher alone: an engine matcher that
transposes `(session_id, sequence)` simply fails to match ACKs from the pinned builder,
and the engine's own fake-radio delivery test catches it on the first try.

What does argue for pinning it is the opposite error, an **over-permissive** matcher. One
that compares only `(session_id, sequence)` and ignores the addressing accepts ACKs it
must not — another node's ACK for its own traffic, or a broadcast-addressed one. Every
self-consistent delivery test still passes, because a correct ACK matches too; what
breaks is only rejection, which no delivery test exercises. That is the same class as the
builder's transposition, and it survives exactly the tests the engine will have. Two
options:

1. `boomlink_linkframe_ack_matches(pending, ack, local_node_id)` in the link frame layer.
   It is a pure function of two headers plus the node ID, so it sits on the same side of
   the stateless/stateful line as the builder. Applying the same spec-pinned technique is
   not free, though: it needs an independent Python mirror and a new `linkframe_tool`
   subcommand, since that is how every other pinned behaviour here is reached from
   pytest.
2. Keep it in the engine, and cover the rejection cases explicitly in its own tests — a
   fake-radio test that feeds a *mismatched* ACK and asserts the frame is still pending.

**Decided: option 1, and option 2 as well rather than instead.** The matcher lives in the
frame layer as `boomlink_linkframe_ack_matches()`, pinned against near-miss vectors by
the Python mirror, because the over-permissive failure above is invisible to any test the
engine alone can have. The engine then *also* feeds mismatched ACKs — wrong source, wrong
sequence, wrong session, and a DATA frame wearing the right fields — and asserts the frame
is still pending, because pinning the predicate does not prove the engine consults it.
The two are cheap together and neither substitutes for the other: the frame layer's tests
say the answer is right, the engine's say the question was asked.

The matching rule itself is not a new decision either way. It is the inverse of the
mapping above, for a **unicast** pending frame — 9.6 scopes ACK waits to unicast, so
`pending.destination_id` is always a real node: frame type ACK, `session_id` and
`sequence` equal to the awaited frame's, `source_id` equal to that frame's
`destination_id`, and `destination_id` equal to the local node ID.

Rules:

- ACK packets never request another ACK;
- broadcast packets never request ACK;
- receiving a valid duplicate of an ACK-requested **unicast** packet causes the ACK to be
  resent — section 9.4's duplicate rule is scoped the same way, since a broadcast frame
  is not acknowledged at all;
- a frame addressed to the broadcast address is never acknowledged, whatever its flags
  say (see the receiver-side responsibilities above);
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

**What counts as an attempt.** The attempt budget counts transmissions the radio
*accepted*, not calls to it. A driver that refuses a send — `radio_send()` reports busy
while a transmission is in progress, and reports the same for absent hardware — radiated
nothing, so the frame keeps its slot in the pipeline, keeps its already-assigned
`(session_id, sequence)`, and is offered again on a later poll. Two consequences worth
stating, because both look like bugs and neither is:

- The frame is **held**, not popped and re-queued. An implementation that dequeues before
  transmitting and drops the item on a refused send lets a busy radio silently destroy
  queued traffic, which is the failure this paragraph exists to rule out.
- Refusals are **not** bounded by a count. At SF12 a single frame legitimately collects
  thousands of "busy" refusals from a superloop polling every millisecond, so any bound
  low enough to detect a dead radio would shed live traffic constantly. A radio refusing
  forever is a dead link however this is written; what matters is that it is visible,
  which the *TX failures* counter (section 9.10) provides.

**Final failure is not the same as no delivery.** An unacknowledged frame may well have
arrived — the ACK is what was lost. The outcome reported to the caller therefore
distinguishes *acknowledged* (the only outcome that means delivery), *sent* (transmitted
with no ACK requested, so the link knows nothing more) and *no ACK* (every attempt
transmitted, none acknowledged). Success is reported as well as failure: a caller told
only about failures cannot tell delivery from a frame still waiting behind a retry
sequence, which at this traffic rate can be seconds.

### 9.7 Random backoff and simultaneous detections

Gunshots or other common acoustic events may be detected by several nodes at almost the
same time. If every node transmits immediately, collision probability is high.

The link layer therefore supports configurable randomized TX jitter for event messages.
The original detection timestamp is captured before this delay, so localization timing
is not changed by radio scheduling.

Retry backoff must also include jitter. Later MAC improvements may add CAD or time slots,
but they are not required for the first prototype.

**How "for event messages" is implemented.** The jitter is configurable per node
(`tx_jitter_max_ms`, drawn uniformly over `[0, max]`, applied before a frame's *first*
transmission only — a retransmission uses the backoff range instead) and it applies to
**every queued frame**, not only to detections. The link layer cannot identify an event
message: section 9 forbids it from decoding the payload, so a detection and a telemetry
reading are the same opaque bytes to it. Narrowing the delay would mean either the caller
passing a flag the link layer would only forward, or inferring intent from priority —
both of which put an application concern into the link layer to save latency on traffic
that is not latency-sensitive.

The cost is a few tens of milliseconds on queued traffic, which this section already
accepts for the traffic it *is* aimed at. Two things are unaffected: ACKs, which never
pass through the queue, and the detection timestamp, which is captured before the delay
so localization timing does not move. Setting the maximum to 0 disables the delay
entirely, which is the right configuration for a node that is the only transmitter in its
own conversation. If one class of traffic ever genuinely needs to skip the delay,
per-priority jitter is the extension point.

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

**"Reorder only the queue" is not a preemption guarantee, and worth stating precisely so
it isn't read as a stronger one.** Once a frame leaves the queue into section 9.6's single
global pipeline slot, nothing can displace it — a `LOW`-priority frame already mid-retry
holds that slot for the rest of its retry/backoff cycle even if a `HIGH`-priority
detection is queued behind it, bounded by `max_attempts` and the backoff range, so single
low digits of seconds at this deployment's traffic rate. "Low-priority telemetry must not
block urgent traffic" above is about the queue's *ordering*, which this respects — the
urgent frame is served next, not last — not about *preempting* whatever the pipeline is
already doing. A guarantee of the second kind would need CAD or time slots (the "later MAC
improvements" this section already defers) to interrupt an in-flight transmission or
retry, which is a materially larger mechanism than a priority queue and is out of scope
here.

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

**Additions the implementation found necessary.** Every counter in the list above
describes something that happened *after* a frame was queued, or to a frame that parsed.
Four drops fall outside that and would otherwise be invisible — a link discarding traffic
with every listed counter reading zero:

- **unmatched ACK** — an ACK addressed to this node that acknowledges nothing pending:
  late (the frame already timed out and was retried) or forged. Distinct from *ACK
  received*, which counts the useful ones. An ACK addressed to *someone else* is
  deliberately **not** counted here: on a shared medium that arrives constantly and is
  ordinary overheard traffic, so it belongs with *packets ignored for another
  destination*. Conflating the two buries the interesting case in the ordinary one.
- **invalid source** — a frame whose `source_id` is the unconfigured address, the
  broadcast address, or *this node's own ID*. The last is a reflection, a second board
  flashed with the same ID, or a spoof; accepting it would have the node acknowledge
  itself and file the frame in its own duplicate cache under its own key, so its own
  later traffic could be suppressed as a duplicate of it. Counted by neither the
  malformed nor the wrong-destination statistic, since the frame is well-formed and
  correctly addressed.
- **oversize packet** — longer than the port declared it can carry, so only a prefix was
  staged. Note this is the *active profile's* limit, not a fixed 255: on a reduced radio
  profile a buffer-sized check would accept packets the radio could not have produced.
- **dropped before queueing** and **shed for more urgent traffic** — a frame the send
  call refused (full queue, oversize payload, forbidden destination), and lower-priority
  traffic evicted under section 9.8's policy. Kept apart on purpose: the first is a node
  generating more than the link can carry, the second is the drop policy working as
  designed, and a node protecting detections should not read as a node in trouble.

One clarification on the listed counters, since two readings are possible: *TX envelopes*
counts each envelope's **first** transmission and *TX retries* counts retransmissions of
one, so transmissions radiated is their sum and neither double-counts the other. *TX
failures* covers both a frame that exhausted section 9.6's attempts unacknowledged and
every transmission the radio itself refused — the second is not a lost frame (it stays
queued and is retried) but it is airtime that did not happen, and a radio refusing
constantly is what an operator needs to see.

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

### 14.1 What the unauthenticated link is knowingly open to

Recorded so these are understood as deferred rather than overlooked. The first four are
closed by message authentication (PR 6) and by nothing short of it — each one is an
attacker getting a frame *accepted*, which is exactly what authentication prevents. The
last is not: reading a valid frame needs no forgery, so only encryption closes it, which
this section already scopes to "where confidentiality is required" rather than making it
unconditional.

- **Duplicate-window poisoning.** One forged frame carrying a sequence far ahead of a
  peer's real one moves that peer's duplicate window forward, and the peer's genuine
  frames are then discarded as stale until it changes session. Deliberately *not*
  mitigated with a heuristic bound on how far a sequence may jump: such a bound trades a
  certain replay hole (a jump just under the limit still works) for an uncertain liveness
  one (a peer whose sequence legitimately advances during a radio outage goes deaf), and
  choosing a number for it without field data would be guessing. Authentication makes the
  question moot; a heuristic would make it worse and harder to reason about.
- **Session reset.** A forged frame from a real source with an unfamiliar `session_id`
  reuses that peer's cache entry (section 9.4), discarding the window it had built.
- **Forged ACKs.** An ACK matching a pending frame's `(session_id, sequence)` and
  addressed correctly completes that frame early, so a lost frame is reported delivered.
  The matcher already rejects everything weaker than an exact match, which is what keeps
  this to *guessing a live sequence* rather than *any ACK will do* — but a listener can
  hear the sequence it needs to guess.
- **Traffic injection generally.** Any node in radio range can put a well-formed frame on
  the air and have it accepted. The network ID (section 7.3) is a filter for accidental
  coexistence, not a credential.
- **Traffic observation.** Any node in radio range can read every byte of every frame,
  including the `(session_id, sequence)` pair the forged-ACK entry above needs. This is
  the one item authentication does not touch — it requires transmitting nothing at all —
  and it is what makes the other four easier rather than being a consequence of them.

None of the first four is reachable through malformed input: they are all *valid* frames
from an attacker who can transmit, which is why hardening the parser further does not move
this boundary. The fifth needs no transmission and no frame of its own.

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

Tracked by issue [#72](https://github.com/boomchecker/boomchecker-monorepo/issues/72).

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

Tracked by issue [#73](https://github.com/boomchecker/boomchecker-monorepo/issues/73).

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

Tracked by issue [#74](https://github.com/boomchecker/boomchecker-monorepo/issues/74).

**Goal:** turn raw RadioLib packets into a usable addressed P2P link.

Scope:

- define the fixed binary link frame header (magic/network ID, version, frame type,
  flags, addressing, session/sequence) as spec + shared C header under
  `fw/common/boomlink/linkframe/`, with a host Python parser;
- implement runtime `node_id` and destination filtering;
- implement `session_id` and monotonically increasing `sequence` assigned at dequeue;
- implement bounded duplicate suppression;
- implement unicast ACK as a link frame type — the frame's field mapping already exists
  as `boomlink_linkframe_make_ack()`; the delivery logic, the two receiver-side
  responsibilities the frame layer cannot take on, and where the ACK matcher lives are
  all implemented and decided (section 9.5);
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

Tracked by issue [#75](https://github.com/boomchecker/boomchecker-monorepo/issues/75).

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

Tracked by issue [#76](https://github.com/boomchecker/boomchecker-monorepo/issues/76).

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

Tracked by issue [#77](https://github.com/boomchecker/boomchecker-monorepo/issues/77).

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

Tracked by issue [#78](https://github.com/boomchecker/boomchecker-monorepo/issues/78).

Potential follow-up after the direct P2P system is measured with realistic node counts:

- Channel Activity Detection (CAD);
- smarter contention/backoff;
- time slots for synchronized deployments;
- message aggregation/coalescing;
- airtime-aware telemetry scheduling.

These changes belong below BoomProtocol and should not change Detection/Config/Command
schemas unless application semantics genuinely change.

## PR 8 — Optional multi-hop/mesh, only if required by deployment

Tracked by issue [#79](https://github.com/boomchecker/boomchecker-monorepo/issues/79).

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