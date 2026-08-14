//main.cpp
#include <Arduino.h>
#include <esp_wifi_types.h>  // wifi_promiscuous_pkt_type_t (used by the sniff callback stub below)
#include "records.h"
#include "wifi_scan.h"
#include "ble_scan.h"

enum AppMode { MODE_WIFI_SCAN, MODE_BLE_SCAN, MODE_WIFI_SNIFF, MODE_MENU };
AppMode currentMode = MODE_MENU;

void wifi_sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type){
    //parse beacon/ probe request, extract MAC + RSSI + SSID

}

void HelloWorld() {
    Serial.println("Hello, world!");
}


// Prints raw scan results over Serial. Temporary until the logger/menu exist.
void doWifiScan() {
    Serial.println("--- Wi-Fi active scan ---");
    for (const auto& ap : bt::wifiActiveScan()) {
        Serial.println(bt::toCsv(ap).c_str());
    }
}

void doBleScan() {
    Serial.println("--- BLE scan (passive) ---");
    for (const auto& dev : bt::bleScan(5)) {
        Serial.println(bt::toCsv(dev).c_str());
    }
}

void sniffWifi() {
    // TODO: out of scope for this change (Wi-Fi passive sniff feature)
}

void handleMenuInput() {
    // TODO: out of scope for this change (serial menu feature)
}

void setup() {
    Serial.begin(115200);
    //init display , buttons, 50
}

void loop() {
    switch (currentMode) {
    case MODE_BLE_SCAN: doBleScan(); break;
    case MODE_WIFI_SCAN: doWifiScan(); break;
    case MODE_WIFI_SNIFF: sniffWifi(); break;
    case MODE_MENU: handleMenuInput(); break;
    }
}
