#pragma once

#include "Car.h"
#include "Ids.h"
#include <vector>

namespace dtl {

struct Train {
    TrainId          id;
    NetworkId        network;
    std::vector<Car> cars;      // position is the bay index it meets
    StationId        atDepot;   // valid() == available at a depot
    OrderId          order;     // valid() == on a delivery
};

}   // namespace dtl
