#pragma once

#include "Ids.h"
#include "Quantity.h"

namespace dtl {

struct Car {
    bool     isFluid = false;
    ItemId   item;   // valid() == carrying something
    Quantity amount;
    Quantity capacity;
};

}   // namespace dtl
