#pragma once

#include "Bay.h"
#include "Ids.h"
#include <string>
#include <vector>

namespace dtl {

struct Station {
    StationId        id;
    std::string      name;   // cache, for display and re-resolution
    NetworkId        network;
    std::vector<Bay> bays;   // ordered, position is the bay index

    int maxInbound = 1;
    int inbound    = 0;   // destined for or docked here

    bool bound         = true;    // false once the underlying station is gone
};

}   // namespace dtl
