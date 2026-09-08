#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "Depot.h"
#include "DrainStation.h"
#include "Order.h"
#include "Problem.h"
#include "Station.h"
#include "Train.h"

namespace dtl {

// Dispatcher's working set. Only Station.bays[i].{item,threshold} + Depot/Drain
// player-config fields persist; everything else is rederived on load.
struct WorldState {
    std::unordered_map<StationId, Station>      stations;
    std::unordered_map<StationId, Depot>        depots;
    std::unordered_map<StationId, DrainStation> drainStations;
    std::unordered_map<TrainId,   Train>        trains;

    std::unordered_map<OrderId, Order> orders;      // active only
    std::vector<Order>                 history;     // ring buffer, size configurable

    std::unordered_map<ItemId, std::vector<BayRef>> providerIndex;
    std::unordered_map<ItemId, std::vector<BayRef>> requesterIndex;
    std::unordered_set<ItemId>                      noProviderFor;

    std::vector<Problem>       problems;
    std::unordered_set<TrainId> residueTrains;      // any train present here has residue
};

}  // namespace dtl
