# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32blue_team is an ESP32 firmware project building a **defensive/forensic security tooling profile** for the ESP32 — i.e., tools aimed at monitoring and protecting a network, not offensive/attack tooling.

`lib/wifi_scan` (active Wi-Fi scan), `lib/ble_scan` (passive BLE scan), `lib/wifi_sniff` (passive promiscuous sniffer, producer/consumer over a FreeRTOS queue + channel hopping), and `lib/ui` (serial menu) are implemented and wired into `src/main.cpp` via the menu's 4 screens. `lib/logger` is still an empty skeleton.

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
- Do not create tests for new functionalities when it is not explicitly demanded.

## Repository layout

- Each v1 feature is its own PlatformIO library under `lib/` (`wifi_scan`, `wifi_sniff`, `ble_scan`, `logger`, `ui`), so modules compile independently. Add a new feature as a new `lib/<name>/` dir.
- Shared, hardware-agnostic code lives in `include/`: `frame_parse.h` (802.11 mgmt-frame parsing: `SniffFrame` POD + `parseFrame`), `records.h` (data types — `ApRecord`/`BleRecord`/`SniffRecord` — + CSV/JSON formatting and `macToString`) and `config.h` (pins, baud, log defaults, feature flags, sniff tunables).
- **Portability rule:** everything in `include/` (`records.h`, `frame_parse.h`, `config.h`, `wifi_auth.h`) must stay Arduino-free (std types only) so it compiles and unit-tests on the `native` env. Put Arduino/ESP-IDF code in `lib/` or `src/`, never in `include/`.
- Intended boundaries (see `docs/architecture.md`): `lib/ui/serial_menu.*` is the only place that does Serial I/O; scan modules return records; `lib/logger` is the only writer to sinks (Serial/SD).

## Claude instructions
- Whenever there is a new relevant characteristic about the project either not stated in this file or contradictory to what is said here, please update this Claude.md. 

## Pipeline de cambios de firmware
Cuando se implemente o modifique funcionalidad en lib/ o src/:
1. embedded-writer implementa.
2. embedded-tester cubre la lógica pura nueva con Unity (si aplica).
3. embedded-reviewer revisa el diff completo (código + tests).
No des una tarea de código por terminada sin pasar por este orden.


## Desmantelamiento de tareas explicitas
En caso de que se pida implementar una tarea, como por ejemplo realizar la Fase 2, se espera que dividas esa tarea en pequenas subtareas y procedas una por una; pidiendo autorizacion al usuario por la CLI para continuar con el proceso. En el ejemplo de la fase 2, deberias saber que hay que realizar un parte de consumidor, otra de productor, drenaje de coalas y salto de canales. Se espera que primero hagas una de ella y que luego pidas seguir al usuario. Esto para mejorar debugging y revision de codigo.