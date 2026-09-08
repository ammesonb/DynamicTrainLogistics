#pragma once

#include <optional>
#include <string>
#include <vector>
#include "Car.h"
#include "Ids.h"

namespace dtl {

struct Train {
    TrainId                  id;
    std::string              network;
    std::vector<Car>         cars;         // position is the bay index it meets
    std::optional<StationId> atDepot;      // set means available
    std::optional<OrderId>   order;
};

}  // namespace dtl
