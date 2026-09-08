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

    // Trains with residue AND no drain outlet found. A train that has residue but
    // can be drained gets a DrainTrip instead; "has residue" itself is derived
    // by walking Train.cars, not stored.
    std::unordered_set<TrainId> stuckTrains;
};

// A train is dispatchable when it's parked, unassigned, and not stuck.
// Assumes DrainTrip entries are torn down before the train re-enters matching
// (a live drain trip means atDepot is invalid, so the depot check catches it).
inline bool isAvailable(const WorldState& w, TrainId t) {
    auto it = w.trains.find(t);
    if (it == w.trains.end())
        return false;
    const Train& tr = it->second;
    return tr.atDepot.valid() && !tr.order.valid() && w.stuckTrains.count(t) == 0;
}

}   // namespace dtl
