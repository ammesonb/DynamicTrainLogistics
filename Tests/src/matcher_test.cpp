#include "catch.hpp"
#include "Domain/Matcher.h"

using namespace dtl;

namespace {

// Example capacities are deliberately small; these are not game defaults.
struct TestCatalog : IItemCatalog {
    std::string displayName(ItemId) const override { return "item"; }
    bool        isFluid(ItemId item) const override { return item == ItemId{20}; }
    Quantity    stackSize(ItemId item) const override { return Quantity::fromUnits(item == ItemId{11} ? 50 : 100); }
} catalog;

// Small builder helpers keep tests declarative.

Bay loadBay(int index, ItemId item, Quantity has, Quantity threshold, Quantity committed = Quantity::zero()) {
    Bay b;
    b.state.index          = index;
    b.state.mode           = BayMode::Load;
    b.state.contents[item] = has;
    b.item                 = item;
    b.threshold            = threshold;
    b.committed            = committed;
    return b;
}

Bay unloadBay(int index, ItemId item, Quantity has, Quantity threshold, Quantity inflight = Quantity::zero()) {
    Bay b;
    b.state.index          = index;
    b.state.mode           = BayMode::Unload;
    b.state.capacity.slots = static_cast<int>(500 / catalog.stackSize(item).wholeUnits());
    b.state.contents[item] = has;
    b.item                 = item;
    b.threshold            = threshold;
    b.inflight             = inflight;
    return b;
}

Bay emptyBay(int index) {
    Bay b;
    b.state.index = index;
    b.state.mode  = BayMode::None;
    return b;
}

Bay fluidLoadBay(int index, ItemId item, Quantity has, Quantity threshold) {
    Bay b                   = loadBay(index, item, has, threshold);
    b.state.isFluid         = true;
    b.state.capacity.volume = Quantity::fromUnits(500);
    return b;
}

Bay fluidUnloadBay(int index, ItemId item, Quantity has, Quantity threshold) {
    Bay b                   = unloadBay(index, item, has, threshold);
    b.state.isFluid         = true;
    b.state.capacity.volume = Quantity::fromUnits(500);
    return b;
}

Car freightCar(int slots) {
    Car c;
    c.isFluid        = false;
    c.capacity.slots = slots;
    return c;
}

Car fluidCar(Quantity capacity) {
    Car c;
    c.isFluid         = true;
    c.capacity.volume = capacity;
    return c;
}

Station makeStation(StationId id, NetworkId net, std::vector<Bay> bays) {
    Station s;
    s.id         = id;
    s.network    = net;
    s.bays       = std::move(bays);
    s.maxInbound = 1;
    return s;
}

Train makeTrain(TrainId id, NetworkId net, std::vector<TrainVehicle> behindEngine, StationId depot = StationId{99}) {
    Train t;
    t.id       = id;
    t.network  = net;
    t.vehicles = {Locomotive{}};
    t.vehicles.insert(t.vehicles.end(), behindEngine.begin(), behindEngine.end());
    t.atDepot = depot;
    return t;
}

void indexProvider(WorldState& w, ItemId item, StationId station, int bayIndex) {
    w.providerIndex[item].push_back({station, bayIndex});
}

void indexRequester(WorldState& w, ItemId item, StationId station, int bayIndex) {
    w.requesterIndex[item].push_back({station, bayIndex});
}

}   // namespace

TEST_CASE("Matcher: empty world -> no match") {
    WorldState w;
    REQUIRE_FALSE(findMatch(w, ItemId{1}, catalog).has_value());
}

TEST_CASE("Matcher: requester with no provider registered -> no match") {
    WorldState w;
    ItemId     plate{10};
    NetworkId  net{1};

    w.stations[StationId{2}] =
        makeStation(StationId{2}, net, {unloadBay(0, plate, Quantity::zero(), Quantity::fromUnits(500))});
    indexRequester(w, plate, StationId{2}, 0);

    REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
}

TEST_CASE("Matcher: single-bay delivery") {
    WorldState w;
    ItemId     plate{10};
    NetworkId  net{1};

    w.stations[StationId{1}] =
        makeStation(StationId{1}, net, {loadBay(0, plate, Quantity::fromUnits(200), Quantity::fromUnits(25))});
    w.stations[StationId{2}] =
        makeStation(StationId{2}, net, {unloadBay(0, plate, Quantity::zero(), Quantity::fromUnits(500))});
    indexProvider(w, plate, StationId{1}, 0);
    indexRequester(w, plate, StationId{2}, 0);

    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {freightCar(1)});

    auto m = findMatch(w, plate, catalog);
    REQUIRE(m.has_value());
    REQUIRE(m->provider == StationId{1});
    REQUIRE(m->requester == StationId{2});
    REQUIRE(m->train == TrainId{7});
    REQUIRE(m->indices == std::vector<int>{0});
    REQUIRE(m->wanted == Quantity::fromUnits(100));
}

TEST_CASE("Matcher: extra provider bay rejects a long train (Example 2)") {
    WorldState w;
    ItemId     plate{10};
    NetworkId  net{1};

    w.stations[StationId{1}] =
        makeStation(StationId{1}, net,
                    {loadBay(0, plate, Quantity::fromUnits(500), Quantity::fromUnits(25)), emptyBay(1),
                     loadBay(2, plate, Quantity::fromUnits(500), Quantity::fromUnits(25))});
    w.stations[StationId{2}] =
        makeStation(StationId{2}, net,
                    {unloadBay(0, plate, Quantity::zero(), Quantity::fromUnits(500)),
                     unloadBay(1, plate, Quantity::zero(), Quantity::fromUnits(500)), emptyBay(2)});
    indexProvider(w, plate, StationId{1}, 0);
    indexProvider(w, plate, StationId{1}, 2);
    indexRequester(w, plate, StationId{2}, 0);
    indexRequester(w, plate, StationId{2}, 1);

    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {freightCar(1), freightCar(1), freightCar(1)});

    REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
    w.trains.at(TrainId{7}).vehicles.resize(3);
    auto m = findMatch(w, plate, catalog);
    REQUIRE(m.has_value());
    REQUIRE(m->indices == std::vector<int>{0});
    REQUIRE(m->wanted == Quantity::fromUnits(100));
}

TEST_CASE("Matcher: index mismatch (Example 3) -> no match") {
    WorldState w;
    ItemId     plate{10};
    NetworkId  net{1};

    w.stations[StationId{1}] = makeStation(
        StationId{1}, net, {emptyBay(0), loadBay(1, plate, Quantity::fromUnits(500), Quantity::fromUnits(25))});
    w.stations[StationId{2}] =
        makeStation(StationId{2}, net, {unloadBay(0, plate, Quantity::zero(), Quantity::fromUnits(500)), emptyBay(1)});
    indexProvider(w, plate, StationId{1}, 1);
    indexRequester(w, plate, StationId{2}, 0);

    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {freightCar(1), freightCar(1)});

    REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
}

TEST_CASE("Matcher: provider threshold not met -> no match") {
    WorldState w;
    ItemId     plate{10};
    NetworkId  net{1};

    w.stations[StationId{1}] =
        makeStation(StationId{1}, net, {loadBay(0, plate, Quantity::fromUnits(10), Quantity::fromUnits(25))});
    w.stations[StationId{2}] =
        makeStation(StationId{2}, net, {unloadBay(0, plate, Quantity::zero(), Quantity::fromUnits(500))});
    indexProvider(w, plate, StationId{1}, 0);
    indexRequester(w, plate, StationId{2}, 0);
    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {freightCar(1)});

    REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
}

TEST_CASE("Matcher: committed reduces provider availability below threshold") {
    WorldState w;
    ItemId     plate{10};
    NetworkId  net{1};

    // 100 in stock, 90 committed, threshold 25 -> 10 available -> not ready
    w.stations[StationId{1}] =
        makeStation(StationId{1}, net,
                    {loadBay(0, plate, Quantity::fromUnits(100), Quantity::fromUnits(25), Quantity::fromUnits(90))});
    w.stations[StationId{2}] =
        makeStation(StationId{2}, net, {unloadBay(0, plate, Quantity::zero(), Quantity::fromUnits(500))});
    indexProvider(w, plate, StationId{1}, 0);
    indexRequester(w, plate, StationId{2}, 0);
    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {freightCar(1)});

    REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
}

TEST_CASE("Matcher: inflight satisfies requester -> no match") {
    WorldState w;
    ItemId     plate{10};
    NetworkId  net{1};

    // has 0, inflight 500, threshold 500 -> already covered
    w.stations[StationId{1}] =
        makeStation(StationId{1}, net, {loadBay(0, plate, Quantity::fromUnits(500), Quantity::fromUnits(25))});
    w.stations[StationId{2}] = makeStation(
        StationId{2}, net, {unloadBay(0, plate, Quantity::zero(), Quantity::fromUnits(500), Quantity::fromUnits(500))});
    indexProvider(w, plate, StationId{1}, 0);
    indexRequester(w, plate, StationId{2}, 0);
    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {freightCar(1)});

    REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
}

TEST_CASE("Matcher: station at maxInbound defers order") {
    WorldState w;
    ItemId     plate{10};
    NetworkId  net{1};

    w.stations[StationId{1}] =
        makeStation(StationId{1}, net, {loadBay(0, plate, Quantity::fromUnits(500), Quantity::fromUnits(25))});
    Station req = makeStation(StationId{2}, net, {unloadBay(0, plate, Quantity::zero(), Quantity::fromUnits(500))});
    req.inbound = 1;   // saturated
    w.stations[StationId{2}] = req;

    indexProvider(w, plate, StationId{1}, 0);
    indexRequester(w, plate, StationId{2}, 0);
    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {freightCar(1)});

    REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
}

TEST_CASE("Matcher: different networks do not match") {
    WorldState w;
    ItemId     plate{10};

    w.stations[StationId{1}] =
        makeStation(StationId{1}, NetworkId{1}, {loadBay(0, plate, Quantity::fromUnits(500), Quantity::fromUnits(25))});
    w.stations[StationId{2}] =
        makeStation(StationId{2}, NetworkId{2}, {unloadBay(0, plate, Quantity::zero(), Quantity::fromUnits(500))});
    indexProvider(w, plate, StationId{1}, 0);
    indexRequester(w, plate, StationId{2}, 0);
    w.trains[TrainId{7}] = makeTrain(TrainId{7}, NetworkId{1}, {freightCar(1)});

    REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
}

TEST_CASE("Matcher: fluid bay requires fluid car at that index") {
    WorldState w;
    ItemId     water{20};
    NetworkId  net{1};

    w.stations[StationId{1}] =
        makeStation(StationId{1}, net, {fluidLoadBay(0, water, Quantity::fromUnits(500), Quantity::fromUnits(25))});
    w.stations[StationId{2}] =
        makeStation(StationId{2}, net, {fluidUnloadBay(0, water, Quantity::zero(), Quantity::fromUnits(500))});
    indexProvider(w, water, StationId{1}, 0);
    indexRequester(w, water, StationId{2}, 0);

    SECTION("solid car at fluid index -> no match") {
        w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {freightCar(1)});
        REQUIRE_FALSE(findMatch(w, water, catalog).has_value());
    }
    SECTION("fluid car at fluid index -> match") {
        w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {fluidCar(Quantity::fromUnits(100))});
        REQUIRE(findMatch(w, water, catalog).has_value());
    }
}

TEST_CASE("Matcher: unavailable train (in flight) rejected") {
    WorldState w;
    ItemId     plate{10};
    NetworkId  net{1};

    w.stations[StationId{1}] =
        makeStation(StationId{1}, net, {loadBay(0, plate, Quantity::fromUnits(500), Quantity::fromUnits(25))});
    w.stations[StationId{2}] =
        makeStation(StationId{2}, net, {unloadBay(0, plate, Quantity::zero(), Quantity::fromUnits(500))});
    indexProvider(w, plate, StationId{1}, 0);
    indexRequester(w, plate, StationId{2}, 0);

    Train t              = makeTrain(TrainId{7}, net, {freightCar(1)});
    t.atDepot            = StationId{};   // in flight
    w.trains[TrainId{7}] = t;

    REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
}

TEST_CASE("Matcher: train carrying wrong item at needed index is ineligible") {
    WorldState w;
    ItemId     plate{10};
    ItemId     screw{11};
    NetworkId  net{1};

    w.stations[StationId{1}] =
        makeStation(StationId{1}, net, {loadBay(0, plate, Quantity::fromUnits(500), Quantity::fromUnits(25))});
    w.stations[StationId{2}] =
        makeStation(StationId{2}, net, {unloadBay(0, plate, Quantity::zero(), Quantity::fromUnits(500))});
    indexProvider(w, plate, StationId{1}, 0);
    indexRequester(w, plate, StationId{2}, 0);

    Car c                = freightCar(1);
    c.item               = screw;   // residue of wrong item at index 0
    c.amount             = Quantity::fromUnits(20);
    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {c});

    REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
}

TEST_CASE("Matcher: train carrying matching residue at needed index is eligible") {
    WorldState w;
    ItemId     plate{10};
    NetworkId  net{1};

    w.stations[StationId{1}] =
        makeStation(StationId{1}, net, {loadBay(0, plate, Quantity::fromUnits(500), Quantity::fromUnits(25))});
    w.stations[StationId{2}] =
        makeStation(StationId{2}, net, {unloadBay(0, plate, Quantity::zero(), Quantity::fromUnits(500))});
    indexProvider(w, plate, StationId{1}, 0);
    indexRequester(w, plate, StationId{2}, 0);

    Car c                = freightCar(1);
    c.item               = plate;
    c.amount             = Quantity::fromUnits(30);
    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {c});

    REQUIRE(findMatch(w, plate, catalog).has_value());
}

TEST_CASE("Matcher: same station cannot route to itself") {
    WorldState w;
    ItemId     plate{10};
    NetworkId  net{1};

    // Mixed station: load bay 0, unload bay 1, same item.
    w.stations[StationId{1}] = makeStation(StationId{1}, net,
                                           {loadBay(0, plate, Quantity::fromUnits(500), Quantity::fromUnits(25)),
                                            unloadBay(1, plate, Quantity::zero(), Quantity::fromUnits(500))});
    indexProvider(w, plate, StationId{1}, 0);
    indexRequester(w, plate, StationId{1}, 1);
    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {freightCar(1), freightCar(1)});

    REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
}

TEST_CASE("findProposals: one candidate per item with a requester") {
    WorldState w;
    ItemId     plate{10};
    ItemId     screw{11};
    NetworkId  net{1};

    w.stations[StationId{1}] =
        makeStation(StationId{1}, net, {loadBay(0, plate, Quantity::fromUnits(500), Quantity::fromUnits(25))});
    w.stations[StationId{2}] =
        makeStation(StationId{2}, net, {unloadBay(0, plate, Quantity::zero(), Quantity::fromUnits(500))});
    w.stations[StationId{3}] =
        makeStation(StationId{3}, net, {loadBay(0, screw, Quantity::fromUnits(500), Quantity::fromUnits(25))});
    w.stations[StationId{4}] =
        makeStation(StationId{4}, net, {unloadBay(0, screw, Quantity::zero(), Quantity::fromUnits(500))});
    indexProvider(w, plate, StationId{1}, 0);
    indexRequester(w, plate, StationId{2}, 0);
    indexProvider(w, screw, StationId{3}, 0);
    indexRequester(w, screw, StationId{4}, 0);

    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {freightCar(1)});
    w.trains[TrainId{8}] = makeTrain(TrainId{8}, net, {freightCar(1)});

    auto ms = findProposals(w, catalog);
    REQUIRE(ms.size() == 2);
    // ItemId ascending: plate(10) before screw(11).
    REQUIRE(ms[0].item == plate);
    REQUIRE(ms[1].item == screw);
    // Proposals compete for resources, they are not a dispatchable batch.
    REQUIRE(ms[0].train == TrainId{7});
    REQUIRE(ms[1].train == TrainId{7});   // matcher is stateless, same train picked twice
}

TEST_CASE("Matcher: missing stock and minimum worthwhile amount") {
    WorldState w;
    ItemId     plate{10};
    NetworkId  net{1};
    w.stations[StationId{1}] =
        makeStation(StationId{1}, net, {loadBay(0, plate, Quantity::fromUnits(200), Quantity::fromUnits(100))});
    w.stations[StationId{2}] =
        makeStation(StationId{2}, net, {unloadBay(0, plate, Quantity::fromUnits(300), Quantity::fromUnits(200))});
    indexProvider(w, plate, StationId{1}, 0);
    indexRequester(w, plate, StationId{2}, 0);
    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {freightCar(1)});
    auto& provider       = w.stations.at(StationId{1});
    auto& requester      = w.stations.at(StationId{2});

    SECTION("exactly 200 missing meets the minimum") {
        REQUIRE(findMatch(w, plate, catalog).has_value());
    }
    SECTION("incoming stock reduces missing quantity") {
        requester.bays[0].inflight = Quantity::fromUnits(1);
        REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
    }
    SECTION("provider exactly at its minimum is eligible") {
        provider.bays[0].committed = Quantity::fromUnits(100);
        REQUIRE(findMatch(w, plate, catalog).has_value());
    }
    SECTION("zero threshold still needs missing stock") {
        requester.bays[0].threshold             = Quantity::zero();
        requester.bays[0].state.contents[plate] = Quantity::fromUnits(500);
        REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
    }
    SECTION("zero threshold still needs available stock") {
        provider.bays[0].threshold             = Quantity::zero();
        provider.bays[0].state.contents[plate] = Quantity::zero();
        REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
    }
    SECTION("negative threshold is invalid") {
        requester.bays[0].threshold = Quantity::fromUnits(-1);
        REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
    }
    SECTION("changed provider direction overrides its old index entry") {
        provider.bays[0].state.mode = BayMode::Unload;
        REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
    }
    SECTION("changed requester direction overrides its old index entry") {
        requester.bays[0].state.mode = BayMode::Load;
        REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
    }
    SECTION("unbound requester cannot receive a train") {
        requester.bound = false;
        REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
    }
    SECTION("unbound provider cannot receive a train") {
        provider.bound = false;
        REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
    }
    SECTION("duplicate index entries do not duplicate a delivery") {
        indexProvider(w, plate, StationId{1}, 0);
        indexRequester(w, plate, StationId{2}, 0);
        auto match = findMatch(w, plate, catalog);
        REQUIRE(match.has_value());
        REQUIRE(match->indices == std::vector<int>{0});
        REQUIRE(match->wanted == Quantity::fromUnits(100));
    }
    SECTION("a drain assignment excludes a train that has not left its depot") {
        w.drainTrips[TrainId{7}] = DrainTrip{TrainId{7}, StationId{8}, StationId{99}};
        REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
    }
}

TEST_CASE("Matcher: whole-stop safety includes cars outside the order") {
    WorldState w;
    ItemId     plate{10}, screw{11};
    NetworkId  net{1};
    w.stations[StationId{1}] = makeStation(
        StationId{1}, net, {loadBay(0, plate, Quantity::fromUnits(200), Quantity::fromUnits(100)), emptyBay(1)});
    w.stations[StationId{2}] = makeStation(StationId{2}, net,
                                           {unloadBay(0, plate, Quantity::zero(), Quantity::fromUnits(100)),
                                            unloadBay(1, screw, Quantity::zero(), Quantity::fromUnits(100))});
    indexProvider(w, plate, StationId{1}, 0);
    indexRequester(w, plate, StationId{2}, 0);
    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {freightCar(1), freightCar(1)});
    auto& extra          = *w.trains.at(TrainId{7}).cargoAtBay(1);

    SECTION("plates over a screw unload bay would contaminate it") {
        extra.item   = plate;
        extra.amount = Quantity::fromUnits(30);
        REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
    }
    SECTION("unrelated residue is blocked by the plate unload filter") {
        extra.item   = screw;
        extra.amount = Quantity::fromUnits(30);
        REQUIRE(findMatch(w, plate, catalog).has_value());
    }
    SECTION("an extra load bay is unsafe even before it fills") {
        w.stations.at(StationId{1}).bays[1] = loadBay(1, plate, Quantity::zero(), Quantity::fromUnits(100));
        REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
    }
    SECTION("an extra load bay before the needed bay cannot be avoided by shortening") {
        auto& p   = w.stations.at(StationId{1});
        auto& r   = w.stations.at(StationId{2});
        p.bays[1] = loadBay(1, plate, Quantity::fromUnits(200), Quantity::fromUnits(100));
        r.bays[0] = emptyBay(0);
        r.bays[1] = unloadBay(1, plate, Quantity::zero(), Quantity::fromUnits(100));
        indexProvider(w, plate, StationId{1}, 1);
        indexRequester(w, plate, StationId{2}, 1);
        REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
        w.trains.at(TrainId{7}).vehicles.resize(2);
        REQUIRE_FALSE(findMatch(w, plate, catalog).has_value());
    }
}

TEST_CASE("Matcher: the same empty freight car has item-dependent capacity") {
    WorldState w;
    NetworkId  net{1};
    ItemId     plate{10}, screw{11};
    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {freightCar(1)});
    for (ItemId item : {plate, screw}) {
        w.stations[StationId{1}] =
            makeStation(StationId{1}, net, {loadBay(0, item, Quantity::fromUnits(200), Quantity::fromUnits(100))});
        w.stations[StationId{2}] =
            makeStation(StationId{2}, net, {unloadBay(0, item, Quantity::zero(), Quantity::fromUnits(100))});
        indexProvider(w, item, StationId{1}, 0);
        indexRequester(w, item, StationId{2}, 0);
        auto match = findMatch(w, item, catalog);
        REQUIRE(match.has_value());
        REQUIRE(match->wanted == catalog.stackSize(item));   // this car has one slot
    }
}

TEST_CASE("Matcher: a shorter train can serve a subset of matching bays") {
    WorldState w;
    ItemId     plate{10};
    NetworkId  net{1};
    w.stations[StationId{1}] = makeStation(StationId{1}, net,
                                           {loadBay(0, plate, Quantity::fromUnits(200), Quantity::fromUnits(100)),
                                            loadBay(1, plate, Quantity::fromUnits(200), Quantity::fromUnits(100))});
    w.stations[StationId{2}] = makeStation(StationId{2}, net,
                                           {unloadBay(0, plate, Quantity::zero(), Quantity::fromUnits(100)),
                                            unloadBay(1, plate, Quantity::zero(), Quantity::fromUnits(100))});
    for (int index : {0, 1}) {
        indexProvider(w, plate, StationId{1}, index);
        indexRequester(w, plate, StationId{2}, index);
    }
    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {freightCar(1)});
    auto match           = findMatch(w, plate, catalog);
    REQUIRE(match.has_value());
    REQUIRE(match->indices == std::vector<int>{0});
    REQUIRE(match->wanted == Quantity::fromUnits(100));
}

TEST_CASE("Matcher: fluid requests compare missing volume including fractional amounts") {
    WorldState w;
    ItemId     water{20};
    NetworkId  net{1};
    w.stations[StationId{1}] =
        makeStation(StationId{1}, net, {fluidLoadBay(0, water, Quantity::fromUnits(200), Quantity::fromUnits(100))});
    w.stations[StationId{2}] =
        makeStation(StationId{2}, net,
                    {fluidUnloadBay(0, water, Quantity::fromHundredths(39950), Quantity::fromHundredths(10050))});
    indexProvider(w, water, StationId{1}, 0);
    indexRequester(w, water, StationId{2}, 0);
    w.trains[TrainId{7}] = makeTrain(TrainId{7}, net, {fluidCar(Quantity::fromHundredths(10050))});
    auto match           = findMatch(w, water, catalog);
    REQUIRE(match.has_value());
    REQUIRE(match->wanted == Quantity::fromHundredths(10050));
    w.stations.at(StationId{2}).bays[0].inflight = Quantity::fromHundredths(1);
    REQUIRE_FALSE(findMatch(w, water, catalog).has_value());
}

namespace {

// Each request starts empty, with 500 units of room per bay. Each provider
// holds 200 units per bay. All cars have one slot, no game capacities implied.
void addRoute(WorldState& w, ItemId item, StationId provider, StationId requester, int bays) {
    std::vector<Bay> loads, unloads;
    for (int i = 0; i < bays; ++i) {
        loads.push_back(loadBay(i, item, Quantity::fromUnits(200), Quantity::fromUnits(100)));
        unloads.push_back(unloadBay(i, item, Quantity::zero(), Quantity::fromUnits(100)));
        indexProvider(w, item, provider, i);
        indexRequester(w, item, requester, i);
    }
    w.stations[provider]  = makeStation(provider, NetworkId{1}, std::move(loads));
    w.stations[requester] = makeStation(requester, NetworkId{1}, std::move(unloads));
}

}   // namespace

TEST_CASE("Priority: using residue outranks more bays and a shorter train") {
    WorldState w;
    ItemId     plate{10}, screw{11};
    addRoute(w, plate, StationId{1}, StationId{2}, 2);
    addRoute(w, screw, StationId{3}, StationId{4}, 1);
    w.trains[TrainId{1}] = makeTrain(TrainId{1}, NetworkId{1}, {freightCar(1), freightCar(1)});
    Car screws           = freightCar(1);
    screws.item          = screw;
    screws.amount        = Quantity::fromUnits(10);
    w.trains[TrainId{9}] = makeTrain(TrainId{9}, NetworkId{1}, {screws, freightCar(1), freightCar(1)});

    auto winner = findBestMatch(w, catalog);
    REQUIRE(winner.has_value());
    REQUIRE(winner->item == screw);
    REQUIRE(winner->train == TrainId{9});
    REQUIRE(winner->usesResidue);
    REQUIRE(winner->indices.size() == 1);
    REQUIRE(winner->trainLength == 4);
    auto proposals = findProposals(w, catalog);
    REQUIRE(proposals.size() == 2);
    REQUIRE(proposals.front().item == winner->item);

    // Simulate claiming the winner; the search itself never claims resources.
    REQUIRE_FALSE(w.trains.at(TrainId{9}).order.valid());
    w.trains.at(TrainId{9}).order = OrderId{1};
    auto next                     = findBestMatch(w, catalog);
    REQUIRE(next.has_value());
    REQUIRE(next->item == plate);
    REQUIRE(next->train == TrainId{1});
}

TEST_CASE("Priority: more bays outrank a shorter train and earlier station IDs") {
    WorldState w;
    ItemId     plate{10};
    addRoute(w, plate, StationId{1}, StationId{2}, 1);
    addRoute(w, plate, StationId{3}, StationId{4}, 2);
    w.trains[TrainId{1}] = makeTrain(TrainId{1}, NetworkId{1}, {freightCar(1)});
    w.trains[TrainId{2}] = makeTrain(TrainId{2}, NetworkId{1}, {freightCar(1), freightCar(1)});
    auto winner          = findMatch(w, plate, catalog);
    REQUIRE(winner.has_value());
    REQUIRE(winner->provider == StationId{3});
    REQUIRE(winner->requester == StationId{4});
    REQUIRE(winner->train == TrainId{2});
    REQUIRE(winner->indices == std::vector<int>{0, 1});
}

TEST_CASE("Priority: shorter train wins equal useful work, even with a later ID") {
    WorldState w;
    ItemId     plate{10};
    addRoute(w, plate, StationId{1}, StationId{2}, 1);
    w.trains[TrainId{1}] = makeTrain(TrainId{1}, NetworkId{1}, {freightCar(1), freightCar(1)});
    w.trains[TrainId{9}] = makeTrain(TrainId{9}, NetworkId{1}, {freightCar(1)});
    auto winner          = findBestMatch(w, catalog);
    REQUIRE(winner.has_value());
    REQUIRE(winner->train == TrainId{9});
    REQUIRE(winner->trainLength == 2);
}

TEST_CASE("Priority: residue merely carried along earns no preference") {
    WorldState w;
    ItemId     plate{10}, screw{11};
    addRoute(w, plate, StationId{1}, StationId{2}, 1);
    Car screws           = freightCar(1);
    screws.item          = screw;
    screws.amount        = Quantity::fromUnits(10);
    w.trains[TrainId{1}] = makeTrain(TrainId{1}, NetworkId{1}, {freightCar(1), screws});
    w.trains[TrainId{9}] = makeTrain(TrainId{9}, NetworkId{1}, {freightCar(1)});
    auto winner          = findBestMatch(w, catalog);
    REQUIRE(winner.has_value());
    REQUIRE(winner->train == TrainId{9});
    REQUIRE_FALSE(winner->usesResidue);
}

TEST_CASE("Priority: unsafe residue never enters the ranking") {
    WorldState w;
    ItemId     plate{10}, screw{11};
    addRoute(w, plate, StationId{1}, StationId{2}, 1);
    w.stations.at(StationId{2}).bays.push_back(unloadBay(1, screw, Quantity::zero(), Quantity::fromUnits(100)));
    Car plates           = freightCar(1);
    plates.item          = plate;
    plates.amount        = Quantity::fromUnits(10);
    w.trains[TrainId{1}] = makeTrain(TrainId{1}, NetworkId{1}, {plates, plates});
    w.trains[TrainId{9}] = makeTrain(TrainId{9}, NetworkId{1}, {freightCar(1)});
    auto winner          = findBestMatch(w, catalog);
    REQUIRE(winner.has_value());
    REQUIRE(winner->train == TrainId{9});
    REQUIRE_FALSE(winner->usesResidue);
}

TEST_CASE("Priority: exact ties use item, provider, requester and train IDs") {
    WorldState w;
    ItemId     plate{10}, screw{11};
    // Insert larger IDs first; ordering must not depend on hash table insertion.
    addRoute(w, screw, StationId{1}, StationId{2}, 1);
    addRoute(w, plate, StationId{7}, StationId{8}, 1);
    addRoute(w, plate, StationId{3}, StationId{4}, 1);
    w.trains[TrainId{9}] = makeTrain(TrainId{9}, NetworkId{1}, {freightCar(1)});
    w.trains[TrainId{1}] = makeTrain(TrainId{1}, NetworkId{1}, {freightCar(1)});
    auto winner          = findBestMatch(w, catalog);
    REQUIRE(winner.has_value());
    REQUIRE(winner->item == plate);
    REQUIRE(winner->provider == StationId{3});
    REQUIRE(winner->requester == StationId{4});
    REQUIRE(winner->train == TrainId{1});
    w.trains.rehash(100);
    w.stations.rehash(100);
    w.requesterIndex.rehash(100);
    auto reordered = findBestMatch(w, catalog);
    REQUIRE(reordered.has_value());
    REQUIRE(reordered->item == winner->item);
    REQUIRE(reordered->provider == winner->provider);
    REQUIRE(reordered->requester == winner->requester);
    REQUIRE(reordered->train == winner->train);
}

TEST_CASE("Priority: a global search without a safe candidate returns nothing") {
    WorldState w;
    REQUIRE_FALSE(findBestMatch(w, catalog).has_value());
    addRoute(w, ItemId{10}, StationId{1}, StationId{2}, 1);
    REQUIRE_FALSE(findBestMatch(w, catalog).has_value());
}

TEST_CASE("Matcher: engines cannot serve bays or create unwanted pickups") {
    WorldState w;
    ItemId     plate{10};
    // Provider and requester both have plate bays at positions 0 and 1.
    addRoute(w, plate, StationId{1}, StationId{2}, 2);
    // Station: engine. Bay 0: second engine. Bay 1: freight car with residue.
    Car plates           = freightCar(1);
    plates.item          = plate;
    plates.amount        = Quantity::fromUnits(30);
    w.trains[TrainId{7}] = makeTrain(TrainId{7}, NetworkId{1}, {Locomotive{}, plates});
    auto match           = findBestMatch(w, catalog);
    REQUIRE(match.has_value());
    REQUIRE(match->indices == std::vector<int>{1});
    REQUIRE(match->wanted == Quantity::fromUnits(100));
    REQUIRE(match->usesResidue);
    REQUIRE(match->trainLength == 3);

    // The first freight car cannot serve bay 0 just because it is cargo car 0.
    w.stations.at(StationId{2}).bays[1] = emptyBay(1);
    REQUIRE_FALSE(findBestMatch(w, catalog).has_value());
}

TEST_CASE("Matcher: an engine between freight cars preserves delivery and residue indices") {
    WorldState w;
    ItemId     plate{10}, screw{11};
    addRoute(w, plate, StationId{1}, StationId{2}, 3);
    Car plates           = freightCar(1);
    plates.item          = plate;
    plates.amount        = Quantity::fromUnits(30);
    w.trains[TrainId{7}] = makeTrain(TrainId{7}, NetworkId{1}, {freightCar(1), Locomotive{}, plates});
    auto match           = findBestMatch(w, catalog);
    REQUIRE(match.has_value());
    REQUIRE(match->indices == std::vector<int>{0, 2});
    REQUIRE(match->wanted == Quantity::fromUnits(200));
    REQUIRE(match->usesResidue);
    REQUIRE(match->trainLength == 4);

    SECTION("the last car would contaminate an off-order screw bay") {
        w.stations.at(StationId{1}).bays[2] = emptyBay(2);
        w.stations.at(StationId{2}).bays[2] = unloadBay(2, screw, Quantity::zero(), Quantity::fromUnits(100));
        REQUIRE_FALSE(findBestMatch(w, catalog).has_value());
    }
    SECTION("an empty last car would still pick up plates beyond the order") {
        w.trains.at(TrainId{7}).cargoAtBay(2)->item   = ItemId{};
        w.trains.at(TrainId{7}).cargoAtBay(2)->amount = Quantity::zero();
        w.stations.at(StationId{2}).bays[2]           = emptyBay(2);
        REQUIRE_FALSE(findBestMatch(w, catalog).has_value());
    }
}

TEST_CASE("Priority: extra engines count toward train length") {
    WorldState w;
    ItemId     plate{10};
    addRoute(w, plate, StationId{1}, StationId{2}, 1);
    // Same cargo capacity and bay served; the earlier ID has an extra rear engine.
    w.trains[TrainId{1}] = makeTrain(TrainId{1}, NetworkId{1}, {freightCar(1), Locomotive{}});
    w.trains[TrainId{9}] = makeTrain(TrainId{9}, NetworkId{1}, {freightCar(1)});
    auto winner          = findBestMatch(w, catalog);
    REQUIRE(winner.has_value());
    REQUIRE(winner->train == TrainId{9});
    REQUIRE(winner->trainLength == 2);
}

TEST_CASE("Matcher: a consist without a docking engine is not eligible") {
    WorldState w;
    ItemId     plate{10};
    addRoute(w, plate, StationId{1}, StationId{2}, 1);
    Train malformed;
    malformed.id           = TrainId{7};
    malformed.network      = NetworkId{1};
    malformed.atDepot      = StationId{99};
    malformed.vehicles     = {freightCar(1), freightCar(1)};
    w.trains[malformed.id] = malformed;
    REQUIRE_FALSE(findBestMatch(w, catalog).has_value());
}
