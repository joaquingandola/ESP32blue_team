#pragma once
// Serial-driven menu: the only module that does Serial I/O. Owns top-level
// mode selection and dispatches into the scan libraries. Screen state
// machine capped at 4 screens: main, Wi-Fi scan, BLE scan, settings/info
// (see CLAUDE.md "Menu UI").

namespace bt {

// Prints the welcome banner and main menu. Call once from setup(), after
// Serial.begin().
void menuInit();

// Reads any pending Serial input and advances the menu state machine.
// Call every loop() iteration. Non-blocking except while a scan itself runs
// (scans are inherently blocking today).
void menuLoop();

}  // namespace bt
