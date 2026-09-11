#pragma once

#include "BayState.h"

namespace dtl {

// Player-configured layer on top of BayState.
// Only item + threshold persist, state is refreshed each cycle.
struct Bay {
    BayState state;
    ItemId   item;
    Quantity threshold;   // minimum available stock (Load) or missing stock (Unload)
    Quantity committed;   // reserved by orders, not yet loaded
    Quantity inflight;    // en route to this bay
};

}   // namespace dtl
