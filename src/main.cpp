//main.cpp
#include <Arduino.h>
#include <esp_wifi_types.h>  // wifi_promiscuous_pkt_type_t (used by the sniff callback stub below)
#include "serial_menu.h"

void wifi_sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type){
    //parse beacon/ probe request, extract MAC + RSSI + SSID

}

void HelloWorld() {
    Serial.println("Hello, world!");
}

void sniffWifi() {
    // TODO: out of scope for this change (Wi-Fi passive sniff feature)
}

void setup() {
    Serial.begin(115200);
    //init display , buttons, 50
    HelloWorld();
    bt::menuInit();
}

void loop() {
    bt::menuLoop();
}