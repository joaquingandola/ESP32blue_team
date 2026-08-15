#include "ble_scan.h"
#include <NimBLEDevice.h>

namespace bt {

namespace {

bool g_bleInitialized = false;

void ensureBleInit() {
    if (!g_bleInitialized) {
        NimBLEDevice::init("");
        g_bleInitialized = true;
    }
}

}

std::vector<BleRecord> bleScan(uint32_t seconds) {
    ensureBleInit();

    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setActiveScan(false);  // passive
    pScan->setInterval(100);
    pScan->setWindow(100);        // window

    NimBLEScanResults found = pScan->getResults(seconds * 1000, false);

    std::vector<BleRecord> results;
    results.reserve(found.getCount());
    for (int i = 0; i < found.getCount(); ++i) {
        const NimBLEAdvertisedDevice* d = found.getDevice(i);
        BleRecord r;
        r.address = d->getAddress().toString().c_str();
        r.name    = d->haveName() ? d->getName().c_str() : "";
        r.rssi    = d->getRSSI();
        const std::vector<uint8_t>& payload = d->getPayload();
        r.adv     = bytesToHex(payload.data(), payload.size());
        results.push_back(std::move(r));
    }

    pScan->clearResults();
    return results;
}

void bleShutdown() {
    if (g_bleInitialized) {
        NimBLEScan* pScan = NimBLEDevice::getScan();
        if(pScan && pScan->isScanning()) {
            pScan->stop();
        }
        NimBLEDevice::deinit(true);
        g_bleInitialized = false;
    }
}

}  // namespace bt
