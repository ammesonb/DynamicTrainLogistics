#pragma once

#include "BayState.h"
#include "Ids.h"
#include <vector>

namespace dtl {

// Drain stations process leftover cargo, so do not request specific items
// and therefore do not need to be configured by players.
struct DrainStation {
    StationId             station;
    NetworkId             network;
    std::vector<BayState> bays;

    int maxInbound = 1;
    int inbound    = 0;
};

}   // namespace dtl
