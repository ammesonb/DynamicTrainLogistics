#pragma once

#include "Ids.h"

namespace dtl {

// A simple 2-stop schedule (drain-station -> depot) that clears residue.
struct DrainTrip {
    enum class Stage : uint8_t { ToDrain, Draining, ToDepot, Done };

    TrainId   train;
    StationId drainStation;
    StationId depot;
    Stage     stage = Stage::ToDrain;
};

}   // namespace dtl
