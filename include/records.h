#pragma once
// Shared data types produced by the scan modules and consumed by the logger.
// Host-portable on purpose: std types only, NO Arduino dependencies, so these
// records and their formatting helpers can be unit-tested on the native env.

#include <cstdint>
#include <cstddef>
#include <string>

namespace bt {

// A Wi-Fi access point observed by an active scan.
struct ApRecord {
    std::string ssid;
    std::string bssid;       // "AA:BB:CC:DD:EE:FF"
    uint8_t     channel = 0;
    int32_t     rssi = 0;    // dBm
    std::string encryption;  // e.g. "OPEN", "WPA2_PSK"
};

// A BLE device/advertisement observed by a scan.
struct BleRecord {
    std::string address;     // "AA:BB:CC:DD:EE:FF"
    std::string name;
    int32_t     rssi = 0;    // dBm
    std::string adv;         // advertisement payload, hex-encoded
};

// ---- Formatting helpers (portable; used by the logger) ----

namespace detail {
// Minimal JSON string escaping (quote + backslash). Sufficient for the fields
// we emit; extend if fields can contain control characters.
inline std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}
}  // namespace detail

// Hex-encodes raw bytes, e.g. {0xDE, 0xAD} -> "dead". Used to store a BLE
// advertisement payload as a printable/loggable string.
inline std::string bytesToHex(const uint8_t* data, size_t len) {
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out += kHex[(data[i] >> 4) & 0x0F];
        out += kHex[data[i] & 0x0F];
    }
    return out;
}

inline std::string csvHeaderAp() {
    return "ssid,bssid,channel,rssi,encryption";
}

inline std::string toCsv(const ApRecord& r) {
    // NOTE: no CSV quoting yet — SSIDs containing commas/quotes will need it.
    return r.ssid + "," + r.bssid + "," + std::to_string(r.channel) + "," +
           std::to_string(r.rssi) + "," + r.encryption;
}

inline std::string toJson(const ApRecord& r) {
    return "{\"ssid\":\"" + detail::jsonEscape(r.ssid) +
           "\",\"bssid\":\"" + detail::jsonEscape(r.bssid) +
           "\",\"channel\":" + std::to_string(r.channel) +
           ",\"rssi\":" + std::to_string(r.rssi) +
           ",\"encryption\":\"" + detail::jsonEscape(r.encryption) + "\"}";
}

inline std::string csvHeaderBle() {
    return "address,name,rssi,adv";
}

inline std::string toCsv(const BleRecord& r) {
    return r.address + "," + r.name + "," + std::to_string(r.rssi) + "," + r.adv;
}

inline std::string toJson(const BleRecord& r) {
    return "{\"address\":\"" + detail::jsonEscape(r.address) +
           "\",\"name\":\"" + detail::jsonEscape(r.name) +
           "\",\"rssi\":" + std::to_string(r.rssi) +
           ",\"adv\":\"" + detail::jsonEscape(r.adv) + "\"}";
}

}  // namespace bt
