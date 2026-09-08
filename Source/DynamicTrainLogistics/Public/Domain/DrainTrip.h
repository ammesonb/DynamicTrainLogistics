#pragma once

#include "Ids.h"

namespace dtl {

// A 2-stop schedule (drain-station -> depot) that clears residue.
// One in flight per train; keyed by TrainId in WorldState.
struct DrainTrip {
    enum class Stage : uint8_t { ToDrain, Draining, ToDepot, Done };

    TrainId   train;
    StationId drainStation;
    StationId depot;
    Stage     stage = Stage::ToDrain;
};

}   // namespace dtl
