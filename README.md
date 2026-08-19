# ESP32blue_team

An ESP32 firmware project focused on **defensive and forensic security tooling** — a "blue team" profile for the ESP32, as opposed to the offensive/red-team tools the platform is more commonly used for.

## Status

**Active development.** Core features implemented:
- ✅ WiFi promiscuous sniffing (802.11 frame capture)
- ✅ BLE device scanning
- ⚠️ Parsing & record output (CSV/JSON) — in progress
- 🔧 Menu UI & SD logging — planned

## v1.0 Prototype Scope

The first prototype targets recon of the ESP32's own network, for defensive visibility and forensic purposes (e.g., spotting unexpected devices or traffic). Features:

- **Active Wi-Fi scan** — standard 802.11 scan reporting SSID, BSSID, channel, RSSI, and encryption type.
- **Passive Wi-Fi sniff** — listen to Wi-Fi traffic without transmitting.
- **BLE scan** — discover nearby BLE devices/advertisements.
- **Logging** — write results to SD card and/or serial, in CSV or JSON.
- **Simple menu UI** — no more than 3-4 screens (e.g. main menu, Wi-Fi scan, BLE scan, log/settings).

No deauthing, injection, or other offensive/disruptive actions — recon and logging only.

## Roadmap

Future profiles/tools will expand on this defensive/forensic focus. Details TBD as the project evolves.
