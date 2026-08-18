# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32blue_team is an ESP32 firmware project building a **defensive/forensic security tooling profile** for the ESP32 — i.e., tools aimed at monitoring and protecting a network, not offensive/attack tooling.

`lib/wifi_scan` (active Wi-Fi scan), `lib/ble_scan` (passive BLE scan), and `lib/ui` (serial menu) are implemented and wired into `src/main.cpp`. `lib/wifi_sniff` (passive promiscuous sniffer, producer/consumer over a FreeRTOS queue + channel hopping) is implemented but **not** wired into the menu yet — it's reachable standalone via the `SNIFF_SMOKE_TEST` build flag (see below). `lib/logger` is still an empty skeleton.

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
- `pio test -e native` — run host-side unit tests (no hardware) for the portable code in `include/` (`test_records`, `test_wifi_auth`, `test_frame_parse`). **Requires a host C++ compiler**: this machine has `gcc` but not `g++`/`cc1plus`, so the native env currently errors out with `g++: command not found`. Install it with `sudo dnf install gcc-c++` (Fedora) to run them.
- `PLATFORMIO_BUILD_FLAGS=-DSNIFF_SMOKE_TEST pio run` (or add `-DSNIFF_SMOKE_TEST` to `build_flags`) — builds the firmware with `src/main.cpp` running the passive Wi-Fi sniffer bring-up (streams parsed beacons/probe-requests to Serial) instead of the menu. Default builds run the menu.
- Do not create tests for new functionalities when it is not explicitly demanded.

## Repository layout

- Each v1 feature is its own PlatformIO library under `lib/` (`wifi_scan`, `wifi_sniff`, `ble_scan`, `logger`, `ui`), so modules compile independently. Add a new feature as a new `lib/<name>/` dir.
- Shared, hardware-agnostic code lives in `include/`: `frame_parse.h` (802.11 mgmt-frame parsing: `SniffFrame` POD + `parseFrame`), `records.h` (data types — `ApRecord`/`BleRecord`/`SniffRecord` — + CSV/JSON formatting and `macToString`) and `config.h` (pins, baud, log defaults, feature flags, sniff tunables).
- **Portability rule:** everything in `include/` (`records.h`, `frame_parse.h`, `config.h`, `wifi_auth.h`) must stay Arduino-free (std types only) so it compiles and unit-tests on the `native` env. Put Arduino/ESP-IDF code in `lib/` or `src/`, never in `include/`.
- Intended boundaries (see `docs/architecture.md`): `lib/ui/serial_menu.*` is the only place that does Serial I/O; scan modules return records; `lib/logger` is the only writer to sinks (Serial/SD).

## Claude instructions
- Whenever there is a new relevant characteristic about the project either not stated in this file or contradictory to what is said here, please update this Claude.md. 