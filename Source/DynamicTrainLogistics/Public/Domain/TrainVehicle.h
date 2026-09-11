#pragma once

#include "Car.h"
#include <variant>

namespace dtl {

// Engines occupy a physical position but have no cargo inventory.
struct Locomotive {};
using TrainVehicle = std::variant<Locomotive, Car>;

}   // namespace dtl
