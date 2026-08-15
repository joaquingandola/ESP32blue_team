#include "wifi_scan.h"
#include <WiFi.h>

#include "wifi_auth.h"

namespace bt {

std::vector<ApRecord> wifiActiveScan() {
    std::vector<ApRecord> results;

    WiFi.mode(WIFI_STA);
    int n = WiFi.scanNetworks();  // blocking active scan
    if (n <= 0) {
        WiFi.scanDelete();
        return results;
    }

    results.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        ApRecord r;
        r.ssid       = WiFi.SSID(i).c_str();
        r.bssid      = WiFi.BSSIDstr(i).c_str();
        r.channel    = static_cast<uint8_t>(WiFi.channel(i));
        r.rssi       = WiFi.RSSI(i);
        r.encryption = wifiAuthModeToString(static_cast<int>(WiFi.encryptionType(i)));
        results.push_back(std::move(r));
    }

    WiFi.scanDelete();  // free the internal scan result buffer
    return results;
}

}  // namespace bt
