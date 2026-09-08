#pragma once

#include <unordered_map>
#include "Ids.h"
#include "Quantity.h"

namespace dtl {

enum class BayMode : uint8_t { None, Load, Unload };

// World-derived. Rebuilt from the platform chain each read; never persisted.
struct BayState {
    int      index    = 0;
    BayMode  mode     = BayMode::None;
    bool     isFluid  = false;

    std::unordered_map<ItemId, Quantity> contents;
    Quantity capacity;
    Quantity free;               // room to accept, item units
    bool     canPartialUnload = false;
};

}  // namespace dtl
