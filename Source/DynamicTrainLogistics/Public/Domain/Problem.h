#pragma once

#include <optional>
#include "Ids.h"

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

struct Problem {
    Severity                 severity = Severity::Info;
    ProblemCode              code     = ProblemCode::Unbound;
    std::optional<ItemId>    item;
    std::optional<StationId> station;
    std::optional<TrainId>   train;
};

}  // namespace dtl
