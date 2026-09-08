#pragma once

#include "BayState.h"
#include "Ids.h"
#include <vector>

namespace dtl {

// Read-only bay list; never enters provider/requester indices.
struct DrainStation {
    StationId             station;
    NetworkId             network;
    std::vector<BayState> bays;

    int maxInbound = 1;
    int inbound    = 0;
};

}   // namespace dtl
