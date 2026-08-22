#pragma once
// Passive promiscuous 802.11 capture (RX only — never transmits).
//
// Design: a producer/consumer split.
//   * Producer: the Wi-Fi driver's promiscuous RX callback (runs in the Wi-Fi
//     task context, NOT a hardware ISR). It does the minimum — copies a bounded
//     slice of each management frame plus RSSI/channel into a POD and pushes it
//     onto a FreeRTOS queue, non-blocking. No heap and no parsing in that hot
//     path; it drops on a full queue rather than stalling the Wi-Fi task.
//   * Consumer: a task we own (pinned to the app core) that blocks on the raw
//     queue, parses beacons/probe-requests into SniffRecord, and enqueues them
//     on a drain queue the caller polls with sniffPoll().
// A periodic esp_timer hops channels (kSniffDwellMs) so we don't stay deaf on
// a single channel.

#include <cstdint>
#include <cstdio>
#include <string>
#include "records.h"

namespace bt {

    struct SniffStats {
    uint32_t capturedRaw = 0;  // frames handed to us by the driver callback
    uint32_t droppedRaw  = 0;  // dropped: raw queue full (consumer too slow)
    uint32_t parsed      = 0;  // frames parsed into a SniffRecord
    uint32_t droppedOut  = 0;  // dropped: out queue full (caller not draining)
    uint32_t stopTimeouts = 0; // sniffStop() gave up waiting for the consumer
                                // task to confirm a clean exit (see sniffStop)
    bool     poisoned    = false;  // true once a stop timeout has occurred:
                                    // the consumer task and both queues were
                                    // deliberately leaked rather than torn
                                    // down unsafely, and sniffStart() will
                                    // keep refusing to run until reboot.
};

// Brings up the Wi-Fi driver in promiscuous mode, creates the queues + consumer
// task, installs the RX callback, and starts channel hopping. Returns false
// (leaving everything torn down) on any setup failure. Returns true if already
// running. Also returns false without doing anything if a previous sniffStop()
// timed out and poisoned the module (stats.poisoned) -- see sniffStop().
bool sniffStart();

// Tears everything down: stops hopping, disables promiscuous RX, signals the
// consumer task to stop and waits (with a bounded timeout) for it to exit on
// its own, then deletes the task and frees both queues. On the normal path
// the consumer is never killed externally -- see the design note on
// consumerTaskFn in wifi_sniff.cpp. If the wait times out (stats.stopTimeouts),
// there is no point at which it's provably safe to delete the task or free
// the queues, so instead the module deliberately leaks the task and both
// queues and marks itself poisoned (stats.poisoned): sniffStart() will refuse
// to run again until the firmware reboots. Safe to call if not started.
void sniffStop();

// Pops one parsed record into `out`. Returns false if none are pending. Never
// blocks; call repeatedly from your loop to drain.
bool sniffPoll(SniffRecord& out);

// Snapshot of the capture/drop counters.
SniffStats sniffStats();

// One-line human-readable rendering of a SniffStats snapshot, used by the
// serial menu's "Wi-Fi passive sniff" screen (lib/ui/serial_menu.cpp). No
// trailing newline; caller decides how to emit it.
inline std::string formatSniffStats(const SniffStats& s) {
    char buf[160];
    std::snprintf(buf, sizeof(buf),
                  "[sniff] captured=%u parsed=%u droppedRaw=%u droppedOut=%u "
                  "stopTimeouts=%u%s",
                  s.capturedRaw, s.parsed, s.droppedRaw, s.droppedOut,
                  s.stopTimeouts, s.poisoned ? " POISONED" : "");
    return std::string(buf);
}

}  // namespace bt