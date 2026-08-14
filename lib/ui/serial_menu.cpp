#include "serial_menu.h"

#include <Arduino.h>

#include "records.h"
#include "wifi_scan.h"
#include "ble_scan.h"

namespace bt {

namespace {

enum Screen { SCREEN_MAIN, SCREEN_WIFI_SCAN, SCREEN_BLE_SCAN, SCREEN_SETTINGS };

constexpr uint32_t kBleScanSeconds = 5;

Screen screen = SCREEN_MAIN;

void printMainMenu() {
    Serial.println();
    Serial.println("=== ESP32blue_team ===");
    Serial.println("1) Wi-Fi active scan");
    Serial.println("2) BLE passive scan");
    Serial.println("3) Settings / info");
    Serial.println("Select an option:");
}

void printWifiScanMenu() {
    Serial.println("[r] Run again  [b] Back to menu");
}

void printBleScanMenu() {
    Serial.println("[r] Run again  [b] Back to menu");
}

void printSettings() {
    Serial.println();
    Serial.println("=== Settings / info ===");
    Serial.print("BLE scan duration: ");
    Serial.print(kBleScanSeconds);
    Serial.println("s");
    Serial.println("Logging: not yet implemented (results print to Serial)");
    Serial.println("[b] Back to menu");
}

void runWifiScan() {
    Serial.println("--- Wi-Fi active scan ---");
    auto results = wifiActiveScan();
    if (results.empty()) {
        Serial.println("No access points found");
    }
    for (const auto& ap : results) {
        Serial.println(toCsv(ap).c_str());
    }
    printWifiScanMenu();
}

void runBleScan() {
    Serial.println("--- BLE scan (passive) ---");
    auto results = bleScan(kBleScanSeconds);
    if (results.empty()) {
        Serial.println("No devices found");
    }
    for (const auto& dev : results) {
        Serial.println(toCsv(dev).c_str());
    }
    printBleScanMenu();
}

void handleMainInput(char c) {
    switch (c) {
        case '1':
            screen = SCREEN_WIFI_SCAN;
            runWifiScan();
            break;
        case '2':
            screen = SCREEN_BLE_SCAN;
            runBleScan();
            break;
        case '3':
            screen = SCREEN_SETTINGS;
            printSettings();
            break;
        default:
            printMainMenu();
            break;
    }
}

void handleWifiScanInput(char c) {
    switch (c) {
        case 'r':
        case 'R':
            runWifiScan();
            break;
        case 'b':
        case 'B':
            screen = SCREEN_MAIN;
            printMainMenu();
            break;
        default:
            printWifiScanMenu();
            break;
    }
}

void handleBleScanInput(char c) {
    switch (c) {
        case 'r':
        case 'R':
            runBleScan();
            break;
        case 'b':
        case 'B':
            screen = SCREEN_MAIN;
            printMainMenu();
            break;
        default:
            printBleScanMenu();
            break;
    }
}

void handleSettingsInput(char c) {
    switch (c) {
        case 'b':
        case 'B':
            screen = SCREEN_MAIN;
            printMainMenu();
            break;
        default:
            printSettings();
            break;
    }
}

}  // namespace

void menuInit() {
    screen = SCREEN_MAIN;
    printMainMenu();
}

void menuLoop() {
    if (!Serial.available()) return;

    char c = static_cast<char>(Serial.read());
    if (c == '\r' || c == '\n') return;

    switch (screen) {
        case SCREEN_MAIN:       handleMainInput(c); break;
        case SCREEN_WIFI_SCAN:  handleWifiScanInput(c); break;
        case SCREEN_BLE_SCAN:   handleBleScanInput(c); break;
        case SCREEN_SETTINGS:   handleSettingsInput(c); break;
    }
}

}  // namespace bt
