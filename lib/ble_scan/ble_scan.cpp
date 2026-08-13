#include "ble_scan.h"
#include <NimBLEDevice.h>

namespace bt {

namespace {

void ensureBleInit() {
    static bool initialized = false;
    if (!initialized) {
        NimBLEDevice::init("");
        initialized = true;
    }
}

}  // namespace

std::vector<BleRecord> bleScan(uint32_t seconds) {
    ensureBleInit();

    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setActiveScan(false);  // passive: listen only, never send SCAN_REQ
    pScan->setInterval(100);
    pScan->setWindow(100);        // window == interval: scan continuously

    NimBLEScanResults found = pScan->start(seconds, false);  // blocking

    std::vector<BleRecord> results;
    results.reserve(found.getCount());
    for (int i = 0; i < found.getCount(); ++i) {
        NimBLEAdvertisedDevice d = found.getDevice(i);
        BleRecord r;
        r.address = d.getAddress().toString().c_str();
        r.name    = d.haveName() ? d.getName().c_str() : "";
        r.rssi    = d.getRSSI();
        r.adv     = bytesToHex(d.getPayload(), d.getPayloadLength());
        results.push_back(std::move(r));
    }

    pScan->clearResults();
    return results;
}

}  // namespace bt
