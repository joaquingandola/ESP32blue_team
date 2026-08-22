#pragma once
// Shared data types produced by the scan modules and consumed by the logger.
// Host-portable on purpose: std types only, NO Arduino dependencies, so these
// records and their formatting helpers can be unit-tested on the native env.

#include <cstdint>
#include <cstddef>
#include <cstdio>
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

// An 802.11 management frame observed passively (beacon or probe request) by
// the promiscuous sniffer (lib/wifi_sniff).
struct SniffRecord {
    std::string type;        // "BEACON" | "PROBE_REQ"
    std::string src;         // transmitter MAC (addr2)
    std::string bssid;       // BSSID (addr3)
    std::string ssid;        // network name; empty for a wildcard probe request
    uint8_t     channel = 0;
    int32_t     rssi = 0;    // dBm
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

// Formats 6 raw MAC bytes as "AA:BB:CC:DD:EE:FF". Portable so the sniff frame
// parser (and its host tests) can share it.
inline std::string macToString(const uint8_t mac[6]) {
    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    out.reserve(17);
    for (int i = 0; i < 6; ++i) {
        if (i) out += ':';
        out += kHex[(mac[i] >> 4) & 0x0F];
        out += kHex[mac[i] & 0x0F];
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

inline std::string csvHeaderSniff() {
    return "type,src,bssid,ssid,channel,rssi";
}

inline std::string toCsv(const SniffRecord& r) {
    // NOTE: no CSV quoting yet — SSIDs containing commas/quotes will need it.
    return r.type + "," + r.src + "," + r.bssid + "," + r.ssid + "," +
           std::to_string(r.channel) + "," + std::to_string(r.rssi);
}

inline std::string toJson(const SniffRecord& r) {
    return "{\"type\":\"" + detail::jsonEscape(r.type) +
           "\",\"src\":\"" + detail::jsonEscape(r.src) +
           "\",\"bssid\":\"" + detail::jsonEscape(r.bssid) +
           "\",\"ssid\":\"" + detail::jsonEscape(r.ssid) +
           "\",\"channel\":" + std::to_string(r.channel) +
           ",\"rssi\":" + std::to_string(r.rssi) + "}";
}

// One-line human-readable rendering of a SniffRecord, used by the serial
// menu's "Wi-Fi passive sniff" screen (lib/ui/serial_menu.cpp). No trailing
// newline; caller decides how to emit it.
inline std::string formatSniffLine(const SniffRecord& r) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%-9s src=%s bssid=%s ch=%2u rssi=%4d ssid=%s",
                  r.type.c_str(), r.src.c_str(), r.bssid.c_str(),
                  r.channel, r.rssi,
                  r.ssid.empty() ? "<broadcast>" : r.ssid.c_str());
    return std::string(buf);
}

}  // namespace bt
