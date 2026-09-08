#pragma once

#include "Ids.h"
#include "Quantity.h"
#include <unordered_map>

namespace dtl {

enum class BayMode : uint8_t { None, Load, Unload };

// World-derived. Rebuilt from the platform chain each read; never persisted.
// Room-for-item is item-dependent for solids: compute at call site as
//   emptySlots * catalog.stackSize(item)
// (partial-stack contribution ignored; safe underestimate).
// Fluid bays use fluidHeadroom directly.
struct BayState {
    int     index   = 0;
    BayMode mode    = BayMode::None;
    bool    isFluid = false;

    std::unordered_map<ItemId, Quantity> contents;
    Quantity                             capacity;

    int      emptySlots = 0;   // solids only
    Quantity fluidHeadroom;    // fluids only
    bool     canPartialUnload = false;
};

}   // namespace dtl
