#include "serial_menu.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>

#include "records.h"
#include "wifi_scan.h"
#include "ble_scan.h"

namespace bt {

namespace {


typedef void (*ActionFn)();
typedef void (*PrintFn)();

struct ScreenStrategy {
    ActionFn onRun;
    PrintFn  onPrint;
};

enum Screen { SCREEN_MAIN, SCREEN_WIFI_SCAN, SCREEN_BLE_SCAN, SCREEN_SETTINGS, SCREEN_shutdownAndSleep };

constexpr uint32_t kBleScanSeconds = 5;

Screen screen = SCREEN_MAIN;

void printMainMenu() {
    Serial.println();
    Serial.println("=== ESP32blue_team ===");
    Serial.println("1) Wi-Fi active scan");
    Serial.println("2) BLE passive scan");
    Serial.println("3) Settings / info");
    Serial.println("4) Shutdown and Sleep");
    Serial.println("Select an option:");
}

void printRunAgainMenu() {
    Serial.println("[r] Run again  [b] Back to menu");
}

void printApTable(const std::vector<ApRecord>& results) {
    Serial.printf("%-20s %-17s %-3s %-6s %s\n", "SSID", "BSSID", "CH", "RSSI", "ENCRYPTION");
    for (const auto& ap : results) {
        Serial.printf("%-20.20s %-17s %-3u %-6d %s\n",
                       ap.ssid.c_str(), ap.bssid.c_str(), ap.channel, ap.rssi,
                       ap.encryption.c_str());
    }
}

void printBleTable(const std::vector<BleRecord>& results) {
    Serial.printf("%-17s %-20s %-6s %s\n", "ADDRESS", "NAME", "RSSI", "ADV");
    for (const auto& dev : results) {
        const std::string name = dev.name.empty() ? "(unknown)" : dev.name;
        Serial.printf("%-17s %-20.20s %-6d %s\n",
                       dev.address.c_str(), name.c_str(), dev.rssi, dev.adv.c_str());
    }
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
    } else {
        printApTable(results);
    }
    printRunAgainMenu();
}

void runBleScan() {
    Serial.println("--- BLE scan (passive) ---");
    auto results = bleScan(kBleScanSeconds);
    if (results.empty()) {
        Serial.println("No devices found");
    } else {
        printBleTable(results);
    }
    printRunAgainMenu();
}

void shutdownAndSleep() {
    Serial.println("shutdownAndSleeping ... (power-cycle or press EN/RESET to use the menu again)");
    Serial.flush();

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    bleShutdown();

    esp_deep_sleep_start();
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
        case '4':
            screen = SCREEN_shutdownAndSleep;
            shutdownAndSleep();
            break;
        default:
            printMainMenu();
            break;
    }
}

void handleScreenInput(
    char c,
    const ScreenStrategy& strategy) {
        switch (c) {
        case 'r':
        case 'R':
            if(strategy.onRun)
                {strategy.onRun();}
            else
                {strategy.onPrint();}
            break;
        case 'b': 
        case 'B':
            screen = SCREEN_MAIN;
            printMainMenu();
            break;
        default: 
            strategy.onPrint();
            break;
        }
    }

static const ScreenStrategy wifiScanStrategy = { runWifiScan, printRunAgainMenu };
static const ScreenStrategy bleScanStrategy  = { runBleScan,  printRunAgainMenu };
static const ScreenStrategy settingsStrategy = { nullptr,     printSettings };

void handleWifiScanInput(char c) { handleScreenInput(c, wifiScanStrategy); }
void handleBleScanInput(char c) { handleScreenInput(c, bleScanStrategy); }
void handleSettingsInput(char c) { handleScreenInput(c, settingsStrategy); }

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
