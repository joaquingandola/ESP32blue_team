//main.cpp
#include <Arduino.h>
#include "serial_menu.h"

#ifdef SNIFF_SMOKE_TEST
// Standalone bring-up path for lib/wifi_sniff. Build with -DSNIFF_SMOKE_TEST to
// run the passive sniffer instead of the menu and stream parsed frames to
// Serial. Success metric: you should see PROBE_REQ frames from your own phone
// as it looks for known networks.
#include "wifi_sniff.h"
#include "records.h"
#endif

void setup() {
    Serial.begin(115200);
#ifdef SNIFF_SMOKE_TEST
    Serial.println();
    Serial.println("[sniff] passive Wi-Fi sniffer bring-up");
    if (!bt::sniffStart()) {
        Serial.println("[sniff] start FAILED");
    }
#else
    //init display , buttons, 50
    bt::menuInit();
#endif
}

void loop() {
#ifdef SNIFF_SMOKE_TEST
    bt::SniffRecord r;
    while (bt::sniffPoll(r)) {
        Serial.println(bt::formatSniffLine(r).c_str());
    }
    static uint32_t lastStats = 0;
    if (millis() - lastStats > 5000) {
        lastStats = millis();
        const bt::SniffStats s = bt::sniffStats();
        Serial.println(bt::formatSniffStats(s).c_str());
    }
    delay(10);
#else
    bt::menuLoop();
#endif
}
