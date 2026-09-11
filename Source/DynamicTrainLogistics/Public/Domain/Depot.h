#pragma once

#include "Ids.h"

namespace dtl {

struct Depot {
    StationId station;
    NetworkId network;

    int parkingSpaces  = 1;
    int occupied       = 0;
    int reserved       = 0;   // incoming claims, excluding trains already counted in occupied
    int maxTrainLength = 0;   // 0 = unconstrained
};

}   // namespace dtl
