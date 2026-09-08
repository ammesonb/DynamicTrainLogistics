#pragma once

#include <optional>
#include "Ids.h"
#include "Quantity.h"

namespace dtl {

struct Car {
    bool                  isFluid  = false;
    std::optional<ItemId> item;
    Quantity              amount;
    Quantity              capacity;
};

}  // namespace dtl
