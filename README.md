# ESP32blue_team

A **defensive and forensic security firmware** for the ESP32 — monitoring and recon tooling rather than offensive capabilities. Designed to spot unexpected network activity and devices from a single ESP32 board.

## Overview

ESP32blue_team provides passive recon capabilities: passive Wi-Fi sniffing (capture and parse 802.11 management frames without transmitting), BLE device scanning, and active Wi-Fi scanning. All results can be logged to Serial or SD card in CSV/JSON format, with a simple menu UI for navigation.

**Core design principles:**
- **No offensive actions** — no deauth, packet injection, or disruptive probing beyond standard 802.11 active scan.
- **Passive first** — listen-only, never initiate transmissions (except during active scan).
- **Portable parsing** — frame parsing logic (`include/frame_parse.h`) is Arduino-free (std C++ only) so it unit-tests on the host (`native` env).
- **Clear boundaries** — scan modules return records; the logger writes to sinks; the menu handles UI. Modules don't cross these boundaries.

## Status

**Active development:**
- ✅ WiFi active scan (802.11 active probing)
- ✅ WiFi passive sniff (promiscuous RX, producer-consumer-parser architecture)
- ✅ BLE device scanning (NimBLE-based)
- ✅ Record types & formatting (ApRecord, BleRecord, SniffRecord; CSV/JSON helpers)
- ✅ Serial menu UI (3-4 screen state machine)
- ⚠️ Parsing & output wiring to menu — in progress
- 🔧 SD card logging — planned

## Architecture

```
include/config.h        Board pins, baud, log defaults, feature flags, sniff tunables
include/records.h       ApRecord / BleRecord / SniffRecord + CSV/JSON formatters
include/frame_parse.h   SniffFrame POD + parseFrame(): raw 802.11 bytes → SniffRecord
include/wifi_auth.h     WiFi auth/encryption type parsing

lib/wifi_scan/          Active 802.11 scan (blocking call → vector<ApRecord>)
lib/wifi_sniff/         Passive promiscuous RX capture + channel hopping
                        Producer (WiFi RX callback) → rawQueue → Consumer task → outQueue → sniffPoll()
lib/ble_scan/           NimBLE passive scan → vector<BleRecord>
lib/logger/             Route records to sinks (Serial/SD), format via records.h
lib/ui/serial_menu/     3-4 screen menu state machine; only place doing Serial I/O

src/main.cpp            setup()/loop(): init logger + menu, run the menu
```

### WiFi Sniffing (Passive)

The sniffer puts the ESP32 in **promiscuous RX mode** and listens to 802.11 management frames (beacons, probe requests) without transmitting. Since the workload runs in the Wi-Fi driver's callback context, it splits into a **producer-consumer** pattern over FreeRTOS queues:

```
WiFi driver task              App core (core 1)           Caller loop
┌──────────────────┐  rawQueue  ┌────────────────────┐  outQueue  ┌──────────┐
│ promiscuous RX   │ ─ SniffFrame → consumer task   │ SniffRec*  → sniffPoll
│ callback         │  (POD)      │ parse → SniffRecord │ (heap ptr) │ (drain)
└──────────────────┘            └────────────────────┘            └──────────┘
     ▲ esp_timer: kSniffDwellMs (~250ms) → esp_wifi_set_channel(next)
     └───────────────── channel hopping 1..13 ──────────────────────
```

- **Producer (RX callback):** Filters to management frames only, reads RSSI/channel metadata, memcpy's bounded raw frame into POD `SniffFrame`, pushes to `rawQueue` with zero timeout. No malloc, no parsing — drops frames if queue is full (bumps counter).
- **Consumer (FreeRTOS task):** Pinned to app core (core 1, separate from Wi-Fi core 0). Polls `rawQueue`, parses each `SniffFrame` into a full `SniffRecord` (SSID extraction, MAC parsing, etc.), hands to caller via `outQueue`.
- **Drain (`sniffPoll()`):** Caller polls `outQueue` and decides what to do (log, display, etc.). No Serial I/O in the sniffer itself — that's the logger's job.
- **Channel hopping:** A periodic timer advances the sniffer across channels (configurable range), so we don't get stuck on channel 1.

## Libraries & Dependencies

- **PlatformIO** — build system and package manager.
- **Arduino framework** — ESP32 core, Serial, GPIO abstractions.
- **ESP-IDF** — low-level ESP32 APIs (Wi-Fi, BLE, timers, FreeRTOS).
- **NimBLE** — BLE scanning (lighter than the full Bluetooth stack).
- **FreeRTOS** — task scheduler, queues, semaphores, mutexes (already part of ESP-IDF).
- **std::string, std::vector, std::atomic** — C++ standard library (available on ESP32, used for portability).

## Build & Test

**Build:**
```bash
pio run                              # Build (default env: esp32dev)
pio run -t upload                    # Build + flash over USB
pio device monitor                   # Open serial console (115200 baud)
```

**Host tests (portable code only):**
```bash
pio test -e native                   # Run unit tests for include/ code (requires g++)
```

**Test the sniffer in isolation:**
```bash
PLATFORMIO_BUILD_FLAGS=-DSNIFF_SMOKE_TEST pio run
# Runs passive sniffer bring-up (streams parsed beacons to Serial) instead of menu
```

## v1.0 Scope

Recon of the ESP32's own network for defensive visibility (e.g., spot unexpected devices/traffic):
- **Active Wi-Fi scan** — standard 802.11 scan, reports SSID/BSSID/channel/RSSI/encryption.
- **Passive Wi-Fi sniff** — continuous 802.11 frame capture and parsing, channel hopping.
- **BLE scan** — discover nearby BLE devices and advertisements.
- **Logging** — write records to Serial and/or SD card, CSV or JSON format.
- **Menu UI** — simple 3-4 screen navigation (main menu, Wi-Fi scan, BLE scan, log/settings).

**Not included:** deauth, packet injection, active probing beyond standard 802.11 scan, offensive capabilities.

## Future

This v1.0 focuses on network recon and monitoring. Future versions may expand with additional defensive/forensic tooling profiles, depending on project direction.