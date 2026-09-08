#pragma once

#include <optional>
#include "BayState.h"

namespace dtl {

// Player-configured layer on top of BayState.
// Only these fields (item, threshold) are persisted; state is refreshed each cycle.
struct Bay {
    BayState              state;
    std::optional<ItemId> item;
    Quantity              threshold;
    Quantity              committed;   // reserved by orders, not yet loaded
    Quantity              inflight;    // en route to this bay
};

}  // namespace dtl
