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

// Delivery assignment (provider -> requester -> depot).
// The future save adapter must preserve assignment intent (see docs/TRAIN_STATES.md).
// Drain trips do not have requesters, so need no provider and are handled by DrainTrip instead.
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
