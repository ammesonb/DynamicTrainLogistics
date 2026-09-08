#pragma once

#include "Depot.h"
#include "DrainStation.h"
#include "DrainTrip.h"
#include "Order.h"
#include "Problem.h"
#include "Station.h"
#include "Train.h"
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace dtl {

// Dispatcher's working set. Only Station.bays[i].{item,threshold} + Depot/Drain
// player-config fields + networkNames persist; everything else is rederived on load.
struct WorldState {
    std::unordered_map<StationId, Station>      stations;
    std::unordered_map<StationId, Depot>        depots;
    std::unordered_map<StationId, DrainStation> drainStations;
    std::unordered_map<TrainId, Train>          trains;

    std::unordered_map<NetworkId, std::string> networkNames;

    std::unordered_map<OrderId, Order>     orders;       // active deliveries
    std::unordered_map<TrainId, DrainTrip> drainTrips;   // at most one per train
    std::deque<Order>                      history;      // capped ring; pop_front over capacity

    std::unordered_map<ItemId, std::vector<BayRef>> providerIndex;
    std::unordered_map<ItemId, std::vector<BayRef>> requesterIndex;
    std::unordered_set<ItemId>                      noProviderFor;

    std::unordered_map<ProblemKey, Problem> problems;
    std::unordered_set<TrainId>             residueTrains;   // membership is the exclusion
};

}   // namespace dtl
