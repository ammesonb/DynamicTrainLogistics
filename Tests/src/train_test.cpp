#include "catch.hpp"
#include "Domain/Train.h"

#include <limits>

using namespace dtl;

TEST_CASE("Train: a second front engine occupies platform zero") {
    Train train;
    Car   plates;
    plates.item    = ItemId{10};
    plates.amount  = Quantity::fromUnits(30);
    train.vehicles = {Locomotive{}, Locomotive{}, plates};

    REQUIRE(train.hasDockingLocomotive());
    REQUIRE(train.platformSpan() == 2);
    REQUIRE(train.cargoAtBay(0) == nullptr);
    REQUIRE(train.cargoAtBay(1) != nullptr);
    REQUIRE(train.cargoAtBay(1)->item == ItemId{10});
    REQUIRE(train.cargoAtBay(2) == nullptr);
    REQUIRE(train.cargoAtBay(std::numeric_limits<size_t>::max()) == nullptr);

    train.cargoAtBay(1)->amount = Quantity::fromUnits(40);
    const Train& snapshot       = train;
    REQUIRE(snapshot.cargoAtBay(1)->amount == Quantity::fromUnits(40));
    REQUIRE(snapshot.cargoAtBay(0) == nullptr);
}

TEST_CASE("Train: engines between and after cargo preserve physical positions") {
    Train train;
    train.vehicles = {Locomotive{}, Car{}, Locomotive{}, Car{}, Locomotive{}};
    REQUIRE(train.platformSpan() == 4);
    REQUIRE(train.cargoAtBay(0) != nullptr);
    REQUIRE(train.cargoAtBay(1) == nullptr);
    REQUIRE(train.cargoAtBay(2) != nullptr);
    REQUIRE(train.cargoAtBay(3) == nullptr);
}

TEST_CASE("Train: cargo mapping requires a leading docking engine") {
    Train train;
    SECTION("empty snapshot") {
        REQUIRE_FALSE(train.hasDockingLocomotive());
        REQUIRE(train.platformSpan() == 0);
        REQUIRE(train.cargoAtBay(0) == nullptr);
    }
    SECTION("engine only") {
        train.vehicles = {Locomotive{}};
        REQUIRE(train.hasDockingLocomotive());
        REQUIRE(train.platformSpan() == 0);
        REQUIRE(train.cargoAtBay(0) == nullptr);
    }
    SECTION("cargo leading a snapshot is not silently shifted") {
        train.vehicles = {Car{}, Car{}, Locomotive{}};
        REQUIRE_FALSE(train.hasDockingLocomotive());
        REQUIRE(train.cargoAtBay(0) == nullptr);
    }
}
