#include "catch.hpp"

// Include every domain header once. Catches include-order and self-sufficiency bugs.
#include "Domain/Bay.h"
#include "Domain/BayState.h"
#include "Domain/Car.h"
#include "Domain/Depot.h"
#include "Domain/DrainStation.h"
#include "Domain/DrainTrip.h"
#include "Domain/Ids.h"
#include "Domain/Order.h"
#include "Domain/Ports.h"
#include "Domain/Problem.h"
#include "Domain/Quantity.h"
#include "Domain/Station.h"
#include "Domain/Train.h"
#include "Domain/WorldState.h"

using namespace dtl;

TEST_CASE("Handles: null sentinel and equality") {
    StationId a{};
    StationId b{42};
    REQUIRE_FALSE(a.valid());
    REQUIRE(b.valid());
    REQUIRE(a != b);
    REQUIRE(a == StationId{});
}

TEST_CASE("BayRef equality and hashing") {
    BayRef r1{StationId{1}, 3};
    BayRef r2{StationId{1}, 3};
    BayRef r3{StationId{1}, 4};
    REQUIRE(r1 == r2);
    REQUIRE_FALSE(r1 == r3);

    std::hash<BayRef> h;
    REQUIRE(h(r1) == h(r2));
}

TEST_CASE("Problem::key derives from optional-shaped handles") {
    Problem p{Severity::Error, ProblemCode::NoProvider};
    p.item    = ItemId{100};
    p.station = StationId{200};
    // train left null
    auto k = p.key();
    REQUIRE(k.code == ProblemCode::NoProvider);
    REQUIRE(k.item == 100);
    REQUIRE(k.station == 200);
    REQUIRE(k.train == 0);
}

TEST_CASE("WorldState default-constructs and accepts inserts") {
    WorldState w;
    w.stations[StationId{1}]     = Station{StationId{1}, "A", NetworkId{1}, {}, 1, 0, false, true};
    w.depots[StationId{2}]       = Depot{StationId{2}, NetworkId{1}, 4, 0, 0};
    w.networkNames[NetworkId{1}] = "default";

    Problem p{Severity::Warn, ProblemCode::AtInboundLimit};
    p.station           = StationId{1};
    w.problems[p.key()] = p;

    DrainTrip dt{TrainId{9}, StationId{3}, StationId{2}, DrainTrip::Stage::ToDrain};
    w.drainTrips[TrainId{9}] = dt;

    w.history.push_back(Order{OrderId{1}});
    REQUIRE(w.history.size() == 1);
    REQUIRE(w.stations.size() == 1);
    REQUIRE(w.problems.size() == 1);
    REQUIRE(w.drainTrips.size() == 1);
    REQUIRE(w.networkNames.at(NetworkId{1}) == "default");
}
