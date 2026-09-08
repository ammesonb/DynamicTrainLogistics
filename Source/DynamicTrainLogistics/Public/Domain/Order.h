#pragma once

#include <vector>
#include "Ids.h"
#include "Quantity.h"

namespace dtl {

enum class OrderStage : uint8_t {
    ToProvider,
    Loading,
    ToRequester,
    Unloading,
    ToDepot,
    Done,
};

struct Order {
    OrderId          id;
    ItemId           item;
    StationId        provider;
    StationId        requester;
    std::vector<int> indices;      // bay indices in play at both ends
    Quantity         wanted;
    Quantity         delivered;
    TrainId          train;
    OrderStage       stage = OrderStage::ToProvider;
};

}  // namespace dtl
