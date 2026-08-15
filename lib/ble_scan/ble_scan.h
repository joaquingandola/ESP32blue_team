#pragma once
// Passive BLE scan: discover advertising devices without transmitting.
#include <cstdint>
#include <vector>
#include "records.h"

namespace bt {
std::vector<BleRecord> bleScan(uint32_t seconds);

// Releases the BLE controller/stack if it was initialized. Call before deep
// sleep so the radio doesn't stay powered mid-transition.
void bleShutdown();

}