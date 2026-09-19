---
"fw-bom-stm32node": minor
---

PR 1 — SX1262 / RadioLib bring-up: raw SX1262 P2P radio communication on
bom-stm32node via a vendored RadioLib (pinned to 7.7.1), with corrected SPI1
timing/NSS/framing, a DIO1-interrupt-driven RX/TX path, EBYTE E22-900M22S
power/RF-switch/TCXO sequencing, a C-facing `radio.h` API, and `radio
status`/`radio ping` CLI commands. No BoomProtocol/BoomLink layers yet.
