#include "wifi_scan.h"
#include <WiFi.h>

namespace bt {

namespace {

std::string encryptionTypeToString(wifi_auth_mode_t enc) {
    switch (enc) {
        case WIFI_AUTH_OPEN:            return "OPEN";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA_WPA2_PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2_ENTERPRISE";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3_PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2_WPA3_PSK";
        case WIFI_AUTH_WAPI_PSK:        return "WAPI_PSK";
        default:                        return "UNKNOWN";
    }
}

}  // namespace

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
        r.encryption = encryptionTypeToString(WiFi.encryptionType(i));
        results.push_back(std::move(r));
    }

    WiFi.scanDelete();  // free the internal scan result buffer
    return results;
}

}  // namespace bt
