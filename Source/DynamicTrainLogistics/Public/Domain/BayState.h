#pragma once

#include "CargoCapacity.h"
#include "Ids.h"
#include "Quantity.h"
#include <unordered_map>

namespace dtl {

enum class BayMode : uint8_t { None, Load, Unload };

// Wholly world-derived, so no need to persist.
// Refreshed by a platform-chain read.
// emptySlots and fluidHeadroom describe physical room, not request thresholds.
// canPartialUnload is determined by Satisfactory conditions, based on empty slots and headroom.
struct BayState {
    int     index   = 0;
    BayMode mode    = BayMode::None;
    bool    isFluid = false;

    std::unordered_map<ItemId, Quantity> contents;
    CargoCapacity                        capacity;

    int      emptySlots = 0;
    Quantity fluidHeadroom;
    bool     canPartialUnload = false;
};

}   // namespace dtl
