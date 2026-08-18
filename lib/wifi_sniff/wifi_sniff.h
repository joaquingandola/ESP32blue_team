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
#include "records.h"

namespace bt {

    struct SniffStats {
    uint32_t capturedRaw = 0;  // frames handed to us by the driver callback
    uint32_t droppedRaw  = 0;  // dropped: raw queue full (consumer too slow)
    uint32_t parsed      = 0;  // frames parsed into a SniffRecord
    uint32_t droppedOut  = 0;  // dropped: out queue full (caller not draining)
};

// Brings up the Wi-Fi driver in promiscuous mode, creates the queues + consumer
// task, installs the RX callback, and starts channel hopping. Returns false
// (leaving everything torn down) on any setup failure. Returns true if already
// running.
bool sniffStart();

// Tears everything down: stops hopping, disables promiscuous RX, deletes the
// consumer task, and frees both queues. Safe to call if not started.
void sniffStop();

// Pops one parsed record into `out`. Returns false if none are pending. Never
// blocks; call repeatedly from your loop to drain.
bool sniffPoll(SniffRecord& out);

// Snapshot of the capture/drop counters.
SniffStats sniffStats();

}  // namespace bt
