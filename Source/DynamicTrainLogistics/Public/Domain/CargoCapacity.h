#pragma once

#include "Quantity.h"

namespace dtl {

// Physical storage, converts solid slots to quantity using the stack size of the item.
struct CargoCapacity {
    int      slots = 0;
    Quantity volume;

    Quantity forItem(bool isFluid, Quantity stackSize) const { return isFluid ? volume : stackSize * slots; }
};

}   // namespace dtl
