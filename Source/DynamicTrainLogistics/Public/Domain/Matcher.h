#pragma once

#include "Ports.h"
#include "WorldState.h"
#include <cstddef>
#include <optional>
#include <vector>

namespace dtl {

struct MatchCandidate {
    ItemId           item;
    StationId        provider;
    StationId        requester;
    std::vector<int> indices;   // sorted in ascending order, includes only positions reached by this train
    TrainId          train;
    Quantity         wanted;                // full desired capacity for this item across selected cars, including residue
    bool             usesResidue = false;   // already has matching cargo at a selected index, not cargo merely carried along
    size_t           trainLength = 0;       // count of all vehicles, including the docking engine (not size in meters)
};

// Best safe proposal for one item: uses residue, then more bays, then shorter
// train, then ascending (item, provider, requester, train) IDs.
// These return candidates but do NOT reserve anything.
std::optional<MatchCandidate> findMatch(const WorldState&, ItemId, const IItemCatalog&);

// Best safe proposal across all requested items, using the same priorities.
// To satisfy all outstanding requests, call repeatedly after committing previous match.
std::optional<MatchCandidate> findBestMatch(const WorldState&, const IItemCatalog&);

// Best proposal per item, sorted by priority. These compete for resources.
// The same train or station may appear more than once.
// After accepting one, rerun matching like findBestMatch.
std::vector<MatchCandidate> findProposals(const WorldState&, const IItemCatalog&);

}   // namespace dtl
