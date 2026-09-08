#pragma once

#include "Ids.h"
#include <cstdint>
#include <functional>

namespace dtl {

enum class Severity : uint8_t { Info, Warn, Error };

enum class ProblemCode : uint16_t {
    Unbound,
    NoProvider,
    NoIndexOverlap,
    NoEligibleTrain,
    AtInboundLimit,
    ResidueNoOutlet,
    DrainBayCoverage,
    Overshoot,
    ConflictingTimetableAuthority,   // dTT detected
};

// Dedup identity within a poll cycle. Handles use value==0 for none.
struct ProblemKey {
    ProblemCode code    = ProblemCode::Unbound;
    uint64_t    station = 0;
    uint64_t    item    = 0;
    uint64_t    train   = 0;

    bool operator==(const ProblemKey& o) const {
        return code == o.code && station == o.station && item == o.item && train == o.train;
    }
};

struct Problem {
    Severity    severity = Severity::Info;
    ProblemCode code     = ProblemCode::Unbound;
    ItemId      item;
    StationId   station;
    TrainId     train;

    ProblemKey key() const { return {code, station.value, item.value, train.value}; }
};

}   // namespace dtl

namespace std {
template <>
struct hash<dtl::ProblemKey> {
    size_t operator()(const dtl::ProblemKey& k) const noexcept {
        auto mix = [](size_t h, uint64_t v) {
            return h ^ (std::hash<uint64_t>{}(v) + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
        };
        size_t h = std::hash<uint16_t>{}(static_cast<uint16_t>(k.code));
        h        = mix(h, k.station);
        h        = mix(h, k.item);
        h        = mix(h, k.train);
        return h;
    }
};
}   // namespace std
