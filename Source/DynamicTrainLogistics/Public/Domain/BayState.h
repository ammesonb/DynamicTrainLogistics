#pragma once

#include "Ids.h"
#include "Quantity.h"
#include <unordered_map>

namespace dtl {

enum class BayMode : uint8_t { None, Load, Unload };

// World-derived. Rebuilt from the platform chain each read; never persisted.
// isFluid dictates which room field is meaningful (emptySlots for solids,
// fluidHeadroom for fluids). The other stays zero.
//
// Solid room-for-item is item-dependent; compute at call site as
//   emptySlots * catalog.stackSize(item)
// Partial-stack room is ignored on purpose: vanilla requires a fully empty
// slot to unload partially, so emptySlots == 0 already means the bay can't
// accept a partial delivery — the shortcut and canPartialUnload agree.
struct BayState {
    int     index   = 0;
    BayMode mode    = BayMode::None;
    bool    isFluid = false;

    std::unordered_map<ItemId, Quantity> contents;
    Quantity                             capacity;

    int      emptySlots = 0;
    Quantity fluidHeadroom;
    bool     canPartialUnload = false;
};

}   // namespace dtl
