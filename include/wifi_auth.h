#pragma once
// Wi-Fi auth mode -> human-readable string. Host-portable on purpose: takes a
// plain int (mirroring the numeric values of ESP-IDF's wifi_auth_mode_t)
// instead of the enum itself, so it can be unit-tested on the native env
// without pulling in esp_wifi_types.h.

#include <string>

namespace bt {

// Numeric values mirror ESP-IDF's wifi_auth_mode_t (esp_wifi_types.h).
enum WifiAuthMode {
    kWifiAuthOpen           = 0,
    kWifiAuthWep            = 1,
    kWifiAuthWpaPsk         = 2,
    kWifiAuthWpa2Psk        = 3,
    kWifiAuthWpaWpa2Psk     = 4,
    kWifiAuthWpa2Enterprise = 5,
    kWifiAuthWpa3Psk        = 6,
    kWifiAuthWpa2Wpa3Psk    = 7,
    kWifiAuthWapiPsk        = 8,
};

inline std::string wifiAuthModeToString(int mode) {
    switch (mode) {
        case kWifiAuthOpen:           return "OPEN";
        case kWifiAuthWep:            return "WEP";
        case kWifiAuthWpaPsk:         return "WPA_PSK";
        case kWifiAuthWpa2Psk:        return "WPA2_PSK";
        case kWifiAuthWpaWpa2Psk:     return "WPA_WPA2_PSK";
        case kWifiAuthWpa2Enterprise: return "WPA2_ENTERPRISE";
        case kWifiAuthWpa3Psk:        return "WPA3_PSK";
        case kWifiAuthWpa2Wpa3Psk:    return "WPA2_WPA3_PSK";
        case kWifiAuthWapiPsk:        return "WAPI_PSK";
        default:                      return "UNKNOWN";
    }
}

}  // namespace bt
