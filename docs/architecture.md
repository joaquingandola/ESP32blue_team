# Architecture (intended)

ESP32blue_team is a defensive/forensic recon firmware. v1 will do active Wi-Fi
scanning, passive Wi-Fi sniffing, BLE scanning, logging to Serial/SD in
CSV or JSON, and a serial menu — no offensive/disruptive actions.

## Module map

```
include/config.h      board pins, baud, log defaults, feature flags, sniff tunables
include/records.h     ApRecord / BleRecord / SniffRecord + portable CSV/JSON helpers
include/frame_parse.h SniffFrame POD + parseFrame(): raw 802.11 bytes -> SniffRecord

lib/wifi_scan         active 802.11 scan  -> vector<ApRecord>
lib/wifi_sniff        passive promiscuous capture (RX only, never transmits)
lib/ble_scan          NimBLE passive scan -> vector<BleRecord>
lib/logger            routes records to sinks (Serial/SD), formats via records.h
lib/ui/serial_menu    3-4 screen state machine; the only place doing Serial I/O

src/main.cpp          setup()/loop(): init logger + menu, run the menu
```

## Intended boundaries

- **records.h is the shared contract.** Scan modules and the logger depend on the
  record types, not on each other. Keep it Arduino-free (std types only) so
  formatting can be unit-tested on the host (`pio test -e native`).
- **The logger is the only writer.** Scan modules return records; they never touch
  Serial/SD. Adding a sink or format should be a logger-only change.
- **serial_menu is the only Serial-I/O owner.** Keep its `Screen` enum and
  navigation backend-agnostic so an on-device display view can be added later
  without touching scan or log logic.

## Passive Wi-Fi sniff (lib/wifi_sniff) — design

Unlike the active scans (a blocking call that returns a `vector`), the sniffer is
a *continuous stream*. The ESP32 is put in **promiscuous RX** mode and we listen
to 802.11 management frames without ever transmitting. The load lives in a
callback that runs in the Wi-Fi driver's context, so the module is split into a
**producer** and a **consumer** connected by a FreeRTOS queue.

```
  Wi-Fi driver task                our app core (core 1)              caller loop
 ┌──────────────────┐   rawQueue  ┌───────────────────────┐  outQueue ┌──────────┐
 │ promiscuous RX cb │ ─ SniffFrame → consumer task        │ SniffRec* → sniffPoll │
 │  (producer)       │  (POD, memcpy)│  parse -> SniffRecord │  (heap ptr)│  (drain) │
 └──────────────────┘             └───────────────────────┘           └──────────┘
        ▲  esp_timer (every kSniffDwellMs ~250ms) -> esp_wifi_set_channel(next)
        └──────────────── channel hopping 1..kSniffChannelMax ───────────────────
```

- **Producer — the promiscuous RX callback.** Runs in the Wi-Fi task context (not
  a hardware ISR). It must stay tiny and non-blocking: it filters to management
  frames, reads RSSI + channel from `rx_ctrl`, copies a *bounded* slice of the
  raw frame into a POD `SniffFrame`, and pushes it onto `rawQueue` with a zero
  timeout. No heap and no 802.11 parsing happen here. If the queue is full it
  **drops** (and bumps a counter) rather than stalling the radio.
- **Consumer — a task we own.** Pinned to the app core (core 1) so it doesn't
  compete with the Wi-Fi task (core 0). It blocks on `rawQueue`, parses each
  `SniffFrame` into a `SniffRecord` (SSID/MACs), and hands finished records to
  the caller via `outQueue`. All the `std::string` work happens here, in our
  controlled context.
- **Drain — `sniffPoll()`.** The caller (menu/main) polls `outQueue` from its own
  loop and decides what to do with each record. The module never does Serial I/O
  itself, preserving the boundary above. Records cross `outQueue` as heap
  pointers (a `SniffRecord` holds `std::string`, so it can't be memcpy'd through
  a queue); `sniffPoll` takes ownership and frees them.
- **Channel hopping.** A periodic `esp_timer` (`kSniffDwellMs`, ~250 ms) advances
  the channel across `kSniffChannelMin..kSniffChannelMax`, so we don't go deaf
  camped on channel 1. Dwell/range are config in `include/config.h`.

### Primitives to implement (status)

- [x] `SniffRecord` in `records.h` (+ `macToString`, CSV/JSON helpers) — the
      portable contract for a captured frame.
- [x] `SniffFrame` POD + `rawQueue` — allocation-free producer→consumer handoff.
- [x] Promiscuous RX callback with a mgmt-only filter (`WIFI_PROMIS_FILTER_MASK_MGMT`).
- [x] `parseFrame()` — pure raw-bytes→`SniffRecord` parser for beacon (subtype
      0x8) and probe-request (0x4); walks tagged params for the SSID element.
      Lives in `include/frame_parse.h`, Arduino-free, so the riskiest logic
      (header offsets, element walking, length checks) is host-testable.
- [x] Consumer task pinned to the app core + `outQueue` + `sniffPoll()` drain.
- [x] `esp_timer` channel hop over the configured range.
- [x] `SniffStats` counters (captured / parsed / droppedRaw / droppedOut).
- [x] Host test for `parseFrame()` on canned beacon/probe-req byte blobs
      (`test/test_frame_parse/`): field extraction, hidden/wildcard SSIDs,
      non-first SSID element, ignored subtypes, and truncated/lying lengths.
      Written and compiling; not yet executed (see Build/Toolchain in CLAUDE.md).
- [ ] Menu screen / logger wiring (deferred: menu is at its 4-screen cap; the
      sniffer is currently reachable via the `SNIFF_SMOKE_TEST` build flag).

## Passive constraint

`wifi_sniff` enables promiscuous RX only — it never transmits. No deauth,
injection, or probing beyond the standard active scan.
