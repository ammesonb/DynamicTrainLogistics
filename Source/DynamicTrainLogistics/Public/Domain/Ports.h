#pragma once

#include <string>
#include <vector>
#include "BayState.h"
#include "Car.h"
#include "Ids.h"
#include "Quantity.h"

namespace dtl {

// Adapter contracts between the UE-facing layer and the pure domain.
// UE adapter implements these against FactoryGame/Circuitry APIs.
// Tests implement them with fakes.

class IItemCatalog {
public:
    virtual ~IItemCatalog() = default;
    virtual std::string displayName(ItemId) const = 0;
    virtual bool        isFluid(ItemId)     const = 0;
    virtual Quantity    stackSize(ItemId)   const = 0;   // 1 for fluids
};

class IWorldSource {
public:
    virtual ~IWorldSource() = default;

    // Read the current bay layout for a bound station via the platform chain.
    virtual std::vector<BayState> walkStation(StationId) = 0;

    // Read car layout + current contents for one train.
    virtual std::vector<Car> readTrainCars(TrainId) = 0;

    // Every train on the world; used for load-time reconciliation and residue scan.
    virtual std::vector<TrainId> allTrains() = 0;

    // Reconcile inbound-train and depot-occupancy counters from live world state
    // (survives save mid-dock).
    virtual int inboundCountFor(StationId)  = 0;
    virtual int occupiedCountFor(StationId) = 0;
};

// Filter direction on a single stop. Empty allow-lists don't collapse into
// "block" — the adapter must translate Block to UFGNoneDescriptor explicitly.
enum class FilterMode : uint8_t { AllowAll, Block, Items };

struct DirectionFilter {
    FilterMode           mode = FilterMode::Block;
    std::vector<ItemId>  items;
};

struct Stop {
    StationId       station;
    DirectionFilter load;
    DirectionFilter unload;
    bool            fullyLoad   = true;
    bool            fullyUnload = true;
};

class IScheduleSink {
public:
    virtual ~IScheduleSink() = default;

    // Overwrites the timetable. Adapter must bracket its own SetStops call
    // against dTT re-entry.
    virtual bool writeSchedule(TrainId, const std::vector<Stop>& stops) = 0;
};

// Timetable parsing feed. One entry per stop in original order, per train.
// Used by import; dispatch is the only writer of stops in normal operation.
struct ImportedStopFilter {
    std::vector<ItemId> loadItems;
    std::vector<ItemId> unloadItems;
    bool loadIsNone   = false;   // true when the entry is UFGNoneDescriptor
    bool unloadIsNone = false;
};

struct ImportedStop {
    StationId          station;
    ImportedStopFilter filter;
};

class IImportSource {
public:
    virtual ~IImportSource() = default;
    virtual std::vector<ImportedStop> readSchedule(TrainId) = 0;
};

}  // namespace dtl
