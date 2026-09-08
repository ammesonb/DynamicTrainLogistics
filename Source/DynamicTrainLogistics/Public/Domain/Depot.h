#pragma once

#include <string>
#include "Ids.h"

namespace dtl {

struct Depot {
    StationId   station;
    std::string network;

    int parkingSpaces   = 1;
    int occupied        = 0;
    int maxTrainLength  = 0;   // 0 = unconstrained
};

}  // namespace dtl
