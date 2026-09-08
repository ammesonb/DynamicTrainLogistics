#pragma once

#include <string>
#include <vector>
#include "BayState.h"
#include "Ids.h"

namespace dtl {

// Read-only bay list; never enters provider/requester indices.
struct DrainStation {
    StationId   station;
    std::string network;
    std::vector<BayState> bays;

    int maxInbound = 1;
    int inbound    = 0;
};

}  // namespace dtl
