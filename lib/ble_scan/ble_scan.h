#pragma once
// Passive BLE scan: discover advertising devices without transmitting.
#include <cstdint>
#include <vector>
#include "records.h"

namespace bt {

// Passively scans for `seconds` and returns one BleRecord per device
// discovered. Never sends a SCAN_REQ (see ble_scan.cpp).
std::vector<BleRecord> bleScan(uint32_t seconds);

}  // namespace bt
