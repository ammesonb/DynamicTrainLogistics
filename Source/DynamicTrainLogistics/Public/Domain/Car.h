#pragma once

#include "CargoCapacity.h"
#include "Ids.h"
#include "Quantity.h"

namespace dtl {

struct Car {
    bool          isFluid = false;
    ItemId        item;
    Quantity      amount;
    CargoCapacity capacity;
};

}   // namespace dtl
