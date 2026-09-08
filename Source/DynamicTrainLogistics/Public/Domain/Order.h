#pragma once

#include "Ids.h"
#include "Quantity.h"
#include <vector>

namespace dtl {

enum class OrderStage : uint8_t {
    ToProvider,
    Loading,
    ToRequester,
    Unloading,
    ToDepot,
    Done,
};

// Delivery order (provider -> requester -> depot).
// Drain trips use DrainTrip; they have no provider and no wanted/delivered.
struct Order {
    OrderId          id;
    ItemId           item;
    StationId        provider;
    StationId        requester;
    std::vector<int> indices;   // bay indices in play at both ends
    Quantity         wanted;
    Quantity         delivered;
    TrainId          train;
    OrderStage       stage = OrderStage::ToProvider;
};

}   // namespace dtl
