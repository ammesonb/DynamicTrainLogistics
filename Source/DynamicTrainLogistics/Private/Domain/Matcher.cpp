#include "Domain/Matcher.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <tuple>

namespace dtl {
namespace {

// Compare priority fields directly - no count can outweigh a higher criterion.
bool higherPriority(const MatchCandidate& a, const MatchCandidate& b) {
    if (a.usesResidue != b.usesResidue)
        return a.usesResidue;
    if (a.indices.size() != b.indices.size())
        return a.indices.size() > b.indices.size();
    if (a.trainLength != b.trainLength)
        return a.trainLength < b.trainLength;
    return std::tie(a.item, a.provider, a.requester, a.train) < std::tie(b.item, b.provider, b.requester, b.train);
}

Quantity contentsOf(const Bay& b) {
    auto it = b.state.contents.find(b.item);
    return it == b.state.contents.end() ? Quantity::zero() : it->second;
}

using StationBays = std::map<StationId, std::vector<int>>;

StationBays groupEligible(const WorldState& w, const std::vector<BayRef>& refs, ItemId item, BayMode mode,
                          const IItemCatalog& catalog) {
    StationBays out;
    for (const BayRef& ref : refs) {
        auto it = w.stations.find(ref.station);
        if (it == w.stations.end() || !it->second.bound || ref.index < 0 ||
            static_cast<size_t>(ref.index) >= it->second.bays.size())
            continue;
        const Bay& b = it->second.bays[ref.index];
        if (b.item != item || b.state.mode != mode || b.state.isFluid != catalog.isFluid(item))
            continue;
        Quantity amount = mode == BayMode::Load ? contentsOf(b) - b.committed
                                                : b.state.capacity.forItem(b.state.isFluid, catalog.stackSize(item)) -
                                                      contentsOf(b) - b.inflight;
        // A zero threshold does not make zero stock or zero demand useful.
        if (b.threshold < Quantity::zero() || amount <= Quantity::zero() || amount < b.threshold)
            continue;
        out[ref.station].push_back(ref.index);
    }
    for (auto& [_, indices] : out) {
        std::sort(indices.begin(), indices.end());
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    }
    return out;
}

bool selected(const std::vector<int>& indices, size_t index) {
    return std::binary_search(indices.begin(), indices.end(), static_cast<int>(index));
}

// A stop filter applies to every reached platform, not just selected indices.
// Reject extra pickups even if the extra bay is currently empty, as it may fill
// before arrival. Unconfigured load bays are conservatively treated as unsafe.
bool stopTransfersFit(const Train& tr, ItemId item, const Station& provider, const Station& requester,
                      const std::vector<int>& indices) {
    for (size_t i = 0; i < tr.platformSpan(); ++i) {
        const Car* cargo = tr.cargoAtBay(i);
        if (!cargo)
            continue;   // an engine occupies this position but cannot transfer cargo
        const Car& c = *cargo;
        if (c.amount < Quantity::zero() || (c.amount > Quantity::zero() && !c.item.valid()))
            return false;
        if (selected(indices, i)) {
            if (c.isFluid != provider.bays[i].state.isFluid || c.isFluid != requester.bays[i].state.isFluid)
                return false;
            if (c.item.valid() && c.item != item)
                return false;
            continue;
        }
        if (i < provider.bays.size()) {
            const Bay& b         = provider.bays[i];
            auto       stock     = b.state.contents.find(item);
            const bool holdsItem = stock != b.state.contents.end() && stock->second > Quantity::zero();
            if (b.state.mode == BayMode::Load && b.state.isFluid == c.isFluid &&
                (!b.item.valid() || b.item == item || holdsItem))
                return false;
        }
        if (i < requester.bays.size()) {
            const Bay& b = requester.bays[i];
            if (b.state.mode == BayMode::Unload && b.state.isFluid == c.isFluid && c.item == item &&
                c.amount > Quantity::zero())
                return false;   // even a satisfied same-item bay is outside this order
        }
    }
    return true;
}

}   // namespace

std::optional<MatchCandidate> findMatch(const WorldState& w, ItemId item, const IItemCatalog& catalog) {
    if (!item.valid())
        return std::nullopt;
    auto pIt = w.providerIndex.find(item);
    auto rIt = w.requesterIndex.find(item);
    if (pIt == w.providerIndex.end() || rIt == w.requesterIndex.end())
        return std::nullopt;

    const auto                    providers  = groupEligible(w, pIt->second, item, BayMode::Load, catalog);
    const auto                    requesters = groupEligible(w, rIt->second, item, BayMode::Unload, catalog);
    std::optional<MatchCandidate> best;

    for (const auto& [pSid, pIndices] : providers) {
        const Station& p = w.stations.at(pSid);
        if (p.inbound >= p.maxInbound)
            continue;
        for (const auto& [rSid, rIndices] : requesters) {
            const Station& r = w.stations.at(rSid);
            if (pSid == rSid || r.network != p.network || r.inbound >= r.maxInbound)
                continue;
            std::vector<int> overlap;
            std::set_intersection(pIndices.begin(), pIndices.end(), rIndices.begin(), rIndices.end(),
                                  std::back_inserter(overlap));
            if (overlap.empty())
                continue;
            for (const auto& [id, tr] : w.trains) {
                if (tr.network != p.network || !isAvailable(w, id) || !tr.hasDockingLocomotive())
                    continue;
                // Only cargo positions can serve bays. Preserve engine gaps and
                // allow a shorter train makeup to serve a safe subset of the overlap.
                std::vector<int> indices;
                for (int index : overlap) {
                    if (tr.cargoAtBay(static_cast<size_t>(index)))
                        indices.push_back(index);
                }
                if (indices.empty() || !stopTransfersFit(tr, item, p, r, indices))
                    continue;
                Quantity wanted;
                bool     usesResidue   = false;
                bool     validCapacity = true;
                for (int index : indices) {
                    const Car&     c        = *tr.cargoAtBay(static_cast<size_t>(index));
                    const Quantity capacity = c.capacity.forItem(c.isFluid, catalog.stackSize(item));
                    if (capacity <= Quantity::zero() || c.amount > capacity) {
                        validCapacity = false;
                        break;
                    }
                    wanted += capacity;
                    usesResidue = usesResidue || (c.item == item && c.amount > Quantity::zero());
                }
                if (validCapacity) {
                    MatchCandidate candidate{item, pSid,   rSid,        std::move(indices),
                                             id,   wanted, usesResidue, tr.vehicles.size()};
                    if (!best || higherPriority(candidate, *best))
                        best = std::move(candidate);
                }
            }
        }
    }
    return best;
}

std::optional<MatchCandidate> findBestMatch(const WorldState& w, const IItemCatalog& catalog) {
    std::optional<MatchCandidate> best;
    // The best of each item's candidates is sufficient to find the global winner.
    for (const auto& [item, _] : w.requesterIndex) {
        auto candidate = findMatch(w, item, catalog);
        if (candidate && (!best || higherPriority(*candidate, *best)))
            best = std::move(candidate);
    }
    return best;
}

std::vector<MatchCandidate> findProposals(const WorldState& w, const IItemCatalog& catalog) {
    std::vector<ItemId> items;
    for (const auto& [id, _] : w.requesterIndex)
        items.push_back(id);
    std::sort(items.begin(), items.end());
    std::vector<MatchCandidate> out;
    for (ItemId item : items) {
        if (auto match = findMatch(w, item, catalog))
            out.push_back(std::move(*match));
    }
    std::sort(out.begin(), out.end(), higherPriority);
    return out;
}

}   // namespace dtl
