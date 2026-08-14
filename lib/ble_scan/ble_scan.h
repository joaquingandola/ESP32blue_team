#pragma once
// Passive BLE scan: discover advertising devices without transmitting.
#include <cstdint>
#include <vector>
#include "records.h"

namespace bt {
std::vector<BleRecord> bleScan(uint32_t seconds);

}