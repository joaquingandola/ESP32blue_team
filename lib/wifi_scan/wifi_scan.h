#pragma once
// Active 802.11 scan: enumerate nearby access points.
#include <vector>
#include "records.h"

namespace bt {

// Performs a blocking active scan and returns one ApRecord per AP found.
// Returns empty on error or if none are found.
std::vector<ApRecord> wifiActiveScan();

}  // namespace bt
