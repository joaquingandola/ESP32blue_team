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
// Normally never killed from outside: sniffStop() signals a cooperative stop
// and waits for consumerExited (see below). This task polls the raw queue
// with a bounded timeout so it notices `stopping` promptly, always finishes
// any new/xQueueSend it started before checking the flag again, and
// self-deletes once it has signaled consumerExited -- so on the happy path
// there is never a window where the task is torn down mid-allocation.
// (sniffStop() does keep a forced vTaskDelete() fallback for the rare case
// where this task doesn't respond in time -- see sniffStop() for why.)
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
    vTaskDelete(nullptr);
}

// ---- Channel hopping -----------------------------------------------
void hopTimerCb(void*) {
    currentChannel = (currentChannel >= kSniffChannelMax) ? kSniffChannelMin
                                                          : currentChannel + 1;
    esp_wifi_set_channel(currentChannel, WIFI_SECOND_CHAN_NONE);
}

}  // namespace

bool sniffStart() {
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
    running = false;

    // Stop producing first: kill the hop timer and the RX callback.
    if (hopTimer) {
        esp_timer_stop(hopTimer);
        esp_timer_delete(hopTimer);
        hopTimer = nullptr;
    }
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(nullptr);

    // Then stop consuming: signal the cooperative stop flag and wait
    // (bounded) for the consumer to confirm it exited on its own. This is
    // the normal path and never kills the task mid `new`/std::string.
    //
    // If that wait times out, the consumer is still potentially alive and
    // touching rawQueue/outQueue -- deleting the queues out from under it
    // would be a use-after-free. As a last-resort fallback we forcibly
    // vTaskDelete() it here (same risk the original synchronous-delete
    // design always carried) so the queues below are provably safe to free.
    // This should be rare; sStopTimeouts tracks how often it happens.
    if (consumerTask) {
        stopping.store(true, std::memory_order_release);
        if (consumerExited == nullptr ||
            xSemaphoreTake(consumerExited, pdMS_TO_TICKS(kSniffConsumerStopTimeoutMs)) != pdTRUE) {
            sStopTimeouts++;
            vTaskDelete(consumerTask);  // fallback: force it dead before freeing queues
        }
        consumerTask = nullptr;
        stopping.store(false, std::memory_order_release);
    }
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
    return s;
}

}  // namespace bt
