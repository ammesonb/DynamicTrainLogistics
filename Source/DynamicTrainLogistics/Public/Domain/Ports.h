#pragma once

#include "BayState.h"
#include "Ids.h"
#include "Quantity.h"
#include "TrainVehicle.h"
#include <string>
#include <vector>

namespace dtl {

// Adapter contracts between the UE-facing layer and the pure domain.
// UE adapter implements these against FactoryGame/Circuitry, tests use fakes.

class IItemCatalog {
public:
    virtual ~IItemCatalog()                       = default;
    virtual std::string displayName(ItemId) const = 0;
    virtual bool        isFluid(ItemId) const     = 0;
    virtual Quantity    stackSize(ItemId) const   = 0;   // 1 for fluids
};

class IWorldSource {
public:
    virtual ~IWorldSource() = default;

    virtual std::vector<BayState> walkStation(StationId) = 0;
    // Full train makeup, ordered from the intended docking locomotive.
    // Never strip engines since they take up platform space too.
    virtual std::vector<TrainVehicle> readTrainMakeup(TrainId) = 0;
    virtual std::vector<TrainId>      allTrains()              = 0;

    // Reconcile counters from live world state (survives save mid-dock).
    virtual int inboundCountFor(StationId)  = 0;
    virtual int occupiedCountFor(StationId) = 0;
};

// Explicit filter mode. Dispatch only ever emits Block or Items.
// AllowAll exists to round-trip legacy imports where a stop had no filter or for drain trips.
enum class FilterMode : uint8_t { AllowAll, Block, Items };

struct DirectionFilter {
    FilterMode          mode = FilterMode::Block;
    std::vector<ItemId> items;
};

enum class DockingRule : uint8_t { Once, Fully };

struct Stop {
    StationId       station;
    DirectionFilter load;
    DirectionFilter unload;
    DockingRule     rule            = DockingRule::Once;
    float           durationSeconds = 0;
    bool            durationAndRule = false;
};

class IScheduleSink {
public:
    virtual ~IScheduleSink() = default;

    // Overwrites the timetable.
    // Adapter brackets against dTT re-entry and translates Block into UFGNoneDescriptor entries.
    virtual bool writeSchedule(TrainId, const std::vector<Stop>& stops) = 0;
};

// Timetable read side, for import only.
struct ImportedStopFilter {
    std::vector<ItemId> loadItems;
    std::vector<ItemId> unloadItems;
    bool                loadIsNone   = false;
    bool                unloadIsNone = false;
};

struct ImportedStop {
    StationId          station;
    ImportedStopFilter filter;
};

class IImportSource {
public:
    virtual ~IImportSource()                                = default;
    virtual std::vector<ImportedStop> readSchedule(TrainId) = 0;
};

// Output side: domain -> adapter -> Circuitry wires.
struct DesiredBayAmount {
    int      bayIndex = 0;
    ItemId   item;
    Quantity amount;
};

struct StationOutputs {
    std::vector<DesiredBayAmount> desired;   // load bays for the next outbound order
    bool                          ready = false;
    std::string                   status;
};

struct DepotOutputs {
    bool        hasStuckTrain = false;
    ItemId      stuckItem;   // valid if hasStuckTrain
    TrainId     stuckTrain;
    std::string status;
};

class IOutputSink {
public:
    virtual ~IOutputSink()                                             = default;
    virtual void writeStationOutputs(StationId, const StationOutputs&) = 0;
    virtual void writeDepotOutputs(StationId, const DepotOutputs&)     = 0;
};

}   // namespace dtl
