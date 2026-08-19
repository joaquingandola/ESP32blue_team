#pragma once
// Central build/runtime configuration: board/serial defaults and per-feature
// tunables. Keep this dependency-light (std types only) so lib/ modules and
// src/ can all include it.

#include <cstdint>

namespace bt {

// ---- Serial ----
constexpr uint32_t kSerialBaud = 115200;

// ---- Passive Wi-Fi sniff (lib/wifi_sniff) ----
// Channel hopping: dwell on each channel this long before advancing. ~250 ms
// keeps us from going deaf on channel 1 while still catching bursty probe reqs.
constexpr uint32_t kSniffDwellMs      = 250;
constexpr uint8_t  kSniffChannelMin   = 1;
constexpr uint8_t  kSniffChannelMax   = 13;

// Producer -> consumer raw-frame queue (driver callback -> parsing task).
constexpr uint32_t kSniffRawQueueDepth = 32;
// Max raw management-frame bytes copied per event. 24B MAC hdr + 12B beacon
// fixed params + SSID IE fit comfortably; we don't need the whole frame.
constexpr uint16_t kSniffFrameMaxLen   = 128;

// Consumer -> caller parsed-record drain queue (drained via sniffPoll()).
constexpr uint32_t kSniffOutQueueDepth = 64;

}  // namespace bt
