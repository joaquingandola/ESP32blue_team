#include "wifi_sniff.h"

#include <atomic>
#include <cstring>
#include <new>
#include <utility>

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "config.h"
#include "frame_parse.h"
#include "records.h"

namespace bt {
namespace {

QueueHandle_t      rawQueue      = nullptr;  // holds SniffFrame
QueueHandle_t      outQueue      = nullptr;  // holds SniffRecord* (heap, consumer-owned)
TaskHandle_t       consumerTask  = nullptr;
SemaphoreHandle_t  consumerExited = nullptr;  // consumer -> sniffStop(): clean exit ack
esp_timer_handle_t hopTimer      = nullptr;
bool               running       = false;
uint8_t            currentChannel = kSniffChannelMin;

// Set once by sniffStop() if it ever times out waiting for the consumer to
// confirm a clean exit (see sniffStop). At that point the consumer task and
// both queues are deliberately leaked rather than torn down unsafely, so the
// module is no longer reusable -- sniffStart() refuses to run again until
// the next reboot.
bool poisoned = false;

// Cooperative stop signal for the consumer task. sniffStop() sets it and
// waits (bounded) on consumerExited instead of killing the task, so any
// in-flight new/std::string in consumerTaskFn always runs to completion.
std::atomic<bool> stopping{false};

// Diagnostics counters. Written from the cb / consumer, read from sniffStats().
// 32-bit aligned, so single-word reads/writes are atomic enough here.
volatile uint32_t sCapturedRaw = 0;
volatile uint32_t sDroppedRaw  = 0;
volatile uint32_t sParsed      = 0;
volatile uint32_t sDroppedOut  = 0;
volatile uint32_t sStopTimeouts = 0;  // sniffStop() gave up waiting for the consumer

// ---- Producer: promiscuous RX callback -------------------------------------
// Drops on a full queue rather than stalling the Wi-Fi task.
void rxCb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT || buf == nullptr) return;

    const auto* pkt = static_cast<const wifi_promiscuous_pkt_t*>(buf);
    const uint16_t sigLen = pkt->rx_ctrl.sig_len;
    if (sigLen < kMacHdrLen) return;

    SniffFrame f;
    f.rssi    = static_cast<int8_t>(pkt->rx_ctrl.rssi);
    f.channel = static_cast<uint8_t>(pkt->rx_ctrl.channel);
    f.len     = sigLen > kSniffFrameMaxLen ? kSniffFrameMaxLen : sigLen;
    memcpy(f.payload, pkt->payload, f.len);

    sCapturedRaw++;
    if (rawQueue == nullptr || xQueueSend(rawQueue, &f, 0) != pdTRUE) {
        sDroppedRaw++;
    }
}

// ---- Consumer: our own task, our own context -------------------------------
// Never killed from outside while it's running: sniffStop() signals a
// cooperative stop and waits for consumerExited (see below). This task polls
// the raw queue with a bounded timeout so it notices `stopping` promptly,
// always finishes any new/xQueueSend it started before checking the flag
// again, then signals consumerExited and suspends itself (vTaskSuspend) --
// so on the happy path there is never a window where the task is torn down
// mid-allocation. sniffStop() confirms the task has actually reached
// eSuspended before deleting it. If it never responds in time, sniffStop()
// leaks the task on purpose instead of forcing a delete -- see sniffStop().
void consumerTaskFn(void*) {
    SniffFrame f;
    for (;;) {
        if (stopping.load(std::memory_order_acquire)) break;

        if (xQueueReceive(rawQueue, &f, pdMS_TO_TICKS(kSniffConsumerPollMs)) != pdTRUE) {
            continue;
        }

        SniffRecord rec;
        if (!parseFrame(f, rec)) continue;
        sParsed++;

        auto* heap = new (std::nothrow) SniffRecord(std::move(rec));
        if (heap == nullptr) { sDroppedOut++; continue; }
        if (xQueueSend(outQueue, &heap, 0) != pdTRUE) {
            delete heap;   // out queue full: caller isn't draining fast enough
            sDroppedOut++;
        }
    }

    if (consumerExited) xSemaphoreGive(consumerExited);
    vTaskSuspend(nullptr);
}

// ---- Channel hopping -----------------------------------------------
void hopTimerCb(void*) {
    currentChannel = (currentChannel >= kSniffChannelMax) ? kSniffChannelMin
                                                          : currentChannel + 1;
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
}

}  // namespace

bool sniffStart() {
    if (poisoned) return false;  // sniffStop() timed out previously; unsafe to reuse until reboot
    if (running) return true;

    stopping.store(false, std::memory_order_release);
    if (consumerExited == nullptr) consumerExited = xSemaphoreCreateBinary();
    else xSemaphoreTake(consumerExited, 0);  // clear any stale signal from a prior cycle
    if (consumerExited == nullptr) return false;

    rawQueue = xQueueCreate(kSniffRawQueueDepth, sizeof(SniffFrame));
    outQueue = xQueueCreate(kSniffOutQueueDepth, sizeof(SniffRecord*));
    if (rawQueue == nullptr || outQueue == nullptr) { sniffStop(); return false; }

    // Bring the Wi-Fi driver up (STA, unassociated) so esp_wifi_* is usable.
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();  // make sure we're not associated to anything

    wifi_promiscuous_filter_t filter = {};
    filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;  // only mgmt frames reach rxCb
    if (esp_wifi_set_promiscuous_filter(&filter) != ESP_OK) { sniffStop(); return false; }
    if (esp_wifi_set_promiscuous_rx_cb(&rxCb) != ESP_OK)     { sniffStop(); return false; }
    if (esp_wifi_set_promiscuous(true) != ESP_OK)            { sniffStop(); return false; }

    currentChannel = kSniffChannelMin;
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);

    // Consumer on the app core (core 1), away from the Wi-Fi task (core 0).
    if (xTaskCreatePinnedToCore(consumerTaskFn, "sniff_cons", 4096, nullptr, 1,
                                &consumerTask, APP_CPU_NUM) != pdPASS) {
        consumerTask = nullptr;
        sniffStop();
        return false;
    }

    esp_timer_create_args_t targs = {};
    targs.callback = &hopTimerCb;
    targs.arg      = nullptr;
    targs.name     = "sniff_hop";
    if (esp_timer_create(&targs, &hopTimer) != ESP_OK) { sniffStop(); return false; }
    if (esp_timer_start_periodic(hopTimer,
            static_cast<uint64_t>(kSniffDwellMs) * 1000) != ESP_OK) {
        sniffStop();
        return false;
    }

    running = true;
    return true;
}

void sniffStop() {
    if (poisoned) return;  // already leaked/unusable; a second call must not
                            // resurrect a stale consumerExited signal and
                            // undo the deliberate leak (see below)
    running = false;

    // Stop producing first: kill the hop timer and the RX callback.
    if (hopTimer) {
        esp_timer_stop(hopTimer);
        esp_timer_delete(hopTimer);
        hopTimer = nullptr;
    }
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);

    // Signal the cooperative stop flag and wait (bounded) for the consumer
    // to confirm it exited on its own. This is the normal path and never
    // kills the task mid `new`/std::string.
    //
    // If that wait times out, the consumer may still be alive and touching
    // rawQueue/outQueue -- it could be blocked in xQueueReceive(rawQueue,..),
    // or mid `new`/xQueueSend(outQueue,..). There is no point we can prove
    // it has reached quiescence, so forcing a vTaskDelete() here and then
    // freeing the queues below would be a real use-after-free / kernel-heap
    // corruption risk, not something "safe to free". Instead we deliberately
    // leak the consumer task and both queues and mark the module `poisoned`:
    // a bounded, intentional leak is strictly safer than corrupting the heap
    // or FreeRTOS's internal task lists. sniffStart() refuses to run again
    // once poisoned -- a firmware reboot is required to recover.
    // sStopTimeouts tracks how often this happens (should be rare).
    if (consumerTask) {
        stopping.store(true, std::memory_order_release);
        if (consumerExited == nullptr ||
            xSemaphoreTake(consumerExited, pdMS_TO_TICKS(kSniffConsumerStopTimeoutMs)) != pdTRUE) {
            sStopTimeouts++;
            poisoned = true;
            return;  // leak consumerTask/rawQueue/outQueue on purpose: no safe teardown here
        }

        // Happy path: the consumer calls xSemaphoreGive(consumerExited)
        // right before vTaskSuspend(nullptr), so there's a short window
        // after we wake up here where it hasn't actually suspended yet
        // (this task and the consumer can be running on different cores).
        // Wait for it to actually reach eSuspended before deleting it --
        // deleting a task that hasn't finished suspending itself yet
        // corrupts FreeRTOS's internal task lists.
        // TODO(review): taskYIELD() only yields to equal/higher-priority
        // tasks on this core -- if sniffStop() is ever called from a
        // higher-priority task pinned to the consumer's core, this spins
        // forever (Task WDT reset). Replace with a bounded vTaskDelay(1)
        // loop that falls back to the `poisoned` path if it never reaches
        // eSuspended, instead of taskYIELD() with no bound.
        while (eTaskGetState(consumerTask) != eSuspended) taskYIELD();
        vTaskDelete(consumerTask);
        consumerTask = nullptr;
        stopping.store(false, std::memory_order_release);
    }
    // TODO(review): rxCb (Wi-Fi driver task, other core) checks
    // `rawQueue == nullptr` before xQueueSend, but esp_wifi_set_promiscuous
    // (false) above doesn't guarantee an in-flight callback has returned --
    // there's a cross-core TOCTOU window where rxCb can still see a valid
    // rawQueue right as it's deleted here. Null the pointer first, add a
    // short grace delay before vQueueDelete, and make rawQueue
    // std::atomic<QueueHandle_t> (it's read/written across cores with no
    // protection today).
    if (rawQueue) {
        vQueueDelete(rawQueue);
        rawQueue = nullptr;
    }
    if (outQueue) {
        SniffRecord* p = nullptr;
        while (xQueueReceive(outQueue, &p, 0) == pdTRUE) delete p;
        vQueueDelete(outQueue);
        outQueue = nullptr;
    }
}

bool sniffPoll(SniffRecord& out) {
    if (outQueue == nullptr) return false;
    SniffRecord* p = nullptr;
    if (xQueueReceive(outQueue, &p, 0) != pdTRUE) return false;
    out = std::move(*p);
    delete p;
    return true;
}

SniffStats sniffStats() {
    SniffStats s;
    s.capturedRaw = sCapturedRaw;
    s.droppedRaw  = sDroppedRaw;
    s.parsed      = sParsed;
    s.droppedOut  = sDroppedOut;
    s.stopTimeouts = sStopTimeouts;
    s.poisoned    = poisoned;
    return s;
}

}  // namespace bt
