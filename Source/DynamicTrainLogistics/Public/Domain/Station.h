#pragma once

#include <string>
#include <vector>
#include "Bay.h"
#include "Ids.h"

namespace dtl {

struct Station {
    StationId   id;
    std::string name;             // cached for display and re-resolution
    std::string network;
    std::vector<Bay> bays;        // ordered; position is the bay index

    int  maxInbound    = 1;
    int  inbound       = 0;       // destined for or docked here

    bool mixedDelivery = false;   // opt-in; ineligible if any bay is Load
    bool bound         = true;    // false once the underlying station is gone
};

struct BayRef {
    StationId station;
    int       index = 0;
};

}  // namespace dtl
