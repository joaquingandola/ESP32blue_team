# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32blue_team is an ESP32 firmware project building a **defensive/forensic security tooling profile** for the ESP32 — i.e., tools aimed at monitoring and protecting a network, not offensive/attack tooling.

`lib/wifi_scan` (active Wi-Fi scan), `lib/ble_scan` (passive BLE scan), and `lib/ui` (serial menu) are implemented and wired into `src/main.cpp`. `lib/wifi_sniff`, `lib/logger`, and `test/test_records` are still empty skeletons.

## v1.0 Prototype Scope

Recon of the ESP32's own network, for defensive visibility and forensic purposes. Planned features:

- **Active Wi-Fi scan**: standard 802.11 scan reporting SSID, BSSID, channel, RSSI, encryption type.
- **Passive Wi-Fi sniff**: listen to Wi-Fi traffic without transmitting.
- **BLE scan**: discover nearby BLE devices/advertisements.
- **Logging**: SD card and/or serial output, in CSV or JSON.
- **Menu UI**: simple, capped at 3-4 screens (e.g. main menu, Wi-Fi scan, BLE scan, log/settings) — don't grow the UI beyond this without checking with the user first.

Hard constraint: no deauthing, packet injection, or other offensive/disruptive actions. "Active scan" here means standard probe-based Wi-Fi scanning only, not an offensive capability — keep this distinction in mind when implementing or reviewing recon features.

## Build/Toolchain

**PlatformIO** (Arduino framework, board `esp32dev`). Config in `platformio.ini`.

- `pio run` — build firmware (default env `esp32dev`).
- `pio run -t upload` — build + flash over USB.
- `pio device monitor` — open serial console (115200 baud).
- `pio test -e native` — run host-side unit tests (no hardware); intended for the portable code in `include/records.h`. `test/test_records/test_records.cpp` is still an empty skeleton, so this currently fails to link (no `main`) until it's written.

## Repository layout

- Each v1 feature is its own PlatformIO library under `lib/` (`wifi_scan`, `wifi_sniff`, `ble_scan`, `logger`, `ui`), so modules compile independently. Add a new feature as a new `lib/<name>/` dir.
- Shared, hardware-agnostic code lives in `include/`: `records.h` (data types + CSV/JSON formatting) and `config.h` (pins, baud, log defaults, feature flags).
- **Portability rule:** `include/records.h` must stay Arduino-free (std types only) so it compiles and unit-tests on the `native` env. Put Arduino/ESP-IDF code in `lib/` or `src/`, never in `include/records.h`.
- Intended boundaries (see `docs/architecture.md`): `lib/ui/serial_menu.*` is the only place that does Serial I/O; scan modules return records; `lib/logger` is the only writer to sinks (Serial/SD).
