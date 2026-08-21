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

// Consumer task shutdown: sniffStop() signals a cooperative stop rather than
// killing the task with vTaskDelete() from outside -- doing that mid `new`/
// std::string could leave the newlib heap allocator mutex held forever.
// kSniffConsumerPollMs bounds the queue-receive timeout the consumer uses so
// it notices the stop flag promptly; kSniffConsumerStopTimeoutMs bounds how
// long sniffStop() waits for the consumer to confirm it exited before giving
// up (rather than blocking forever).
constexpr uint32_t kSniffConsumerPollMs        = 50;
constexpr uint32_t kSniffConsumerStopTimeoutMs = 500;

// ---- Serial menu (lib/ui/serial_menu) ----
// How long the "Wi-Fi passive sniff" screen sleeps between sniffPoll() drain
// passes while waiting for new records / a stop keypress.
constexpr uint32_t kSniffMenuPollMs = 10;
// Upper bound on records drained (and printed) per outer-loop pass in
// runWifiSniff(). Serial.printf at kSerialBaud blocks once the TX buffer
// fills (~150 records/s ceiling); with several active APs the out queue can
// produce at or above that rate, so draining it unconditionally would starve
// the Serial.available() stop-key check and hang the menu. Bounding the
// drain per pass guarantees we check for 'b'/'B' at least this often.
constexpr uint32_t kSniffMaxDrainPerPass = 12;

}  // namespace bt
