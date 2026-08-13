# Architecture (intended)

ESP32blue_team is a defensive/forensic recon firmware. v1 will do active Wi-Fi
scanning, passive Wi-Fi sniffing, BLE scanning, logging to Serial/SD in
CSV or JSON, and a serial menu — no offensive/disruptive actions.

## Module map

```
include/config.h      board pins, baud, log defaults, feature flags
include/records.h     ApRecord / BleRecord + portable CSV/JSON helpers

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

## Passive constraint

`wifi_sniff` should enable promiscuous RX only — never transmit. No deauth,
injection, or probing beyond the standard active scan.
