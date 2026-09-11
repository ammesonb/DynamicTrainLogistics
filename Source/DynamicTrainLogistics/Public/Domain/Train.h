#pragma once

#include "Ids.h"
#include "TrainVehicle.h"
#include <cstddef>
#include <vector>

namespace dtl {

struct Train {
    TrainId   id;
    NetworkId network;
    // Ordered from the locomotive that will dock, including every engine.
    // vehicles[0] meets the station, vehicles[bay + 1] meets that platform.
    // The adapter must establish the same alignment at both delivery stops.
    std::vector<TrainVehicle> vehicles;
    StationId                 atDepot;   // physical location only - may already have an assignment
    OrderId                   order;     // valid() == on a delivery

    bool hasDockingLocomotive() const {
        return !vehicles.empty() && std::holds_alternative<Locomotive>(vehicles.front());
    }

    // Number of platform positions occupied behind the docking engine.
    size_t platformSpan() const { return vehicles.empty() ? 0 : vehicles.size() - 1; }

    // Null means an engine, a bay past the end of the train, or an invalid makeup.
    const Car* cargoAtBay(size_t index) const {
        if (!hasDockingLocomotive() || index >= platformSpan())
            return nullptr;
        return std::get_if<Car>(&vehicles[index + 1]);
    }

    Car* cargoAtBay(size_t index) {
        if (!hasDockingLocomotive() || index >= platformSpan())
            return nullptr;
        return std::get_if<Car>(&vehicles[index + 1]);
    }
};

}   // namespace dtl
