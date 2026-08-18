#pragma once
// Portable 802.11 management-frame parsing: raw captured bytes -> SniffRecord.
//
// Lives in include/ (not lib/wifi_sniff) on purpose: this is the trickiest part
// of the passive sniffer — header offsets, tagged-parameter walking, length
// checks — and keeping it Arduino-free means it can be unit-tested on the host
// (`pio test -e native`) instead of only by flashing hardware.

#include <cstddef>
#include <cstdint>

#include "config.h"
#include "records.h"

namespace bt {

// ---- 802.11 management frame layout ----
constexpr uint8_t kSubtypeProbeReq = 0x04;
constexpr uint8_t kSubtypeBeacon   = 0x08;

constexpr size_t  kMacHdrLen      = 24;    // mgmt MAC header (fixed part)
constexpr size_t  kBeaconFixedLen = 12;    // timestamp(8) + interval(2) + caps(2)
constexpr uint8_t kTagSsid        = 0x00;  // SSID tagged element id
constexpr size_t  kSsidMaxLen     = 32;

// Offsets inside the mgmt MAC header.
constexpr size_t kOffAddr2 = 10;  // transmitter (the device that sent the frame)
constexpr size_t kOffAddr3 = 16;  // BSSID

// One captured frame as handed from the promiscuous RX callback to the parsing
// task. Deliberately a POD (no std::string, no heap) so the callback can fill
// it and memcpy it through a FreeRTOS queue without allocating.
struct SniffFrame {
    int8_t   rssi = 0;
    uint8_t  channel = 0;
    uint16_t len = 0;                     // valid bytes in payload
    uint8_t  payload[kSniffFrameMaxLen];  // bounded copy of the raw frame
};

// Parses a beacon (subtype 0x8) or probe-request (subtype 0x4) into `out`.
// Returns false for anything else, or for a frame too short/malformed to read.
// Pure: no Arduino/ESP calls, so it is host-testable.
inline bool parseFrame(const SniffFrame& f, SniffRecord& out) {
    // Never trust len: it is attacker-influenced and also clamped by the caller.
    const size_t len = f.len > kSniffFrameMaxLen ? kSniffFrameMaxLen : f.len;
    if (len < kMacHdrLen) return false;

    const uint8_t* p = f.payload;
    const uint16_t fc = static_cast<uint16_t>(p[0]) |
                        (static_cast<uint16_t>(p[1]) << 8);
    const uint8_t ftype    = (fc >> 2) & 0x3;
    const uint8_t fsubtype = (fc >> 4) & 0xF;
    if (ftype != 0x0) return false;  // management frames only

    size_t tagStart;
    if (fsubtype == kSubtypeBeacon) {
        out.type = "BEACON";
        // A beacon carries fixed parameters before the tagged elements.
        tagStart = kMacHdrLen + kBeaconFixedLen;
    } else if (fsubtype == kSubtypeProbeReq) {
        out.type = "PROBE_REQ";
        tagStart = kMacHdrLen;  // probe request: tagged elements start immediately
    } else {
        return false;  // other mgmt subtypes are out of scope for now
    }

    out.src     = macToString(p + kOffAddr2);
    out.bssid   = macToString(p + kOffAddr3);
    out.channel = f.channel;
    out.rssi    = f.rssi;
    out.ssid.clear();

    // Walk the tagged parameters looking for the SSID element (tag 0). The SSID
    // is not guaranteed to be first, so we iterate rather than assume.
    for (size_t i = tagStart; i + 2 <= len; ) {
        const uint8_t tag  = p[i];
        const uint8_t tlen = p[i + 1];
        if (i + 2 + tlen > len) break;  // truncated element: stop, keep what we have
        if (tag == kTagSsid) {
            const uint8_t n = tlen > kSsidMaxLen ? kSsidMaxLen : tlen;
            const uint8_t* ssid = p + i + 2;
            // A hidden network advertises an SSID element that is either zero
            // length or all NUL bytes. Normalize both to an empty SSID so we
            // never put embedded NULs into the record (they would truncate any
            // CSV/JSON/printf output downstream).
            bool allNul = true;
            for (uint8_t k = 0; k < n; ++k) {
                if (ssid[k] != 0) { allNul = false; break; }
            }
            if (!allNul) out.ssid.assign(reinterpret_cast<const char*>(ssid), n);
            break;
        }
        i += 2 + tlen;
    }
    return true;
}

}  // namespace bt
