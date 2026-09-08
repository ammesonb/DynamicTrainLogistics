#pragma once

#include "Ids.h"

namespace dtl {

struct Depot {
    StationId station;
    NetworkId network;

    int parkingSpaces  = 1;
    int occupied       = 0;
    int maxTrainLength = 0;   // 0 = unconstrained
};

}   // namespace dtl
