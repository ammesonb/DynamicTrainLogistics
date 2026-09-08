#include "catch.hpp"
#include "Domain/Quantity.h"

#include <algorithm>

using dtl::Quantity;

TEST_CASE("Quantity: construction") {
    REQUIRE(Quantity::fromUnits(10).raw() == 1000);
    REQUIRE(Quantity::fromHundredths(250).raw() == 250);
    REQUIRE(Quantity::zero().raw() == 0);
    REQUIRE(Quantity{}.raw() == 0);
}

TEST_CASE("Quantity: whole/hundredths decomposition (positive)") {
    auto q = Quantity::fromHundredths(1234);
    REQUIRE(q.wholeUnits() == 12);
    REQUIRE(q.hundredths() == 34);
}

TEST_CASE("Quantity: whole/hundredths decomposition uses floor semantics for negatives") {
    // -1.50
    auto q = Quantity::fromHundredths(-150);
    REQUIRE(q.wholeUnits() == -2);
    REQUIRE(q.hundredths() == 50);

    // -1.00 exactly
    auto e = Quantity::fromHundredths(-100);
    REQUIRE(e.wholeUnits() == -1);
    REQUIRE(e.hundredths() == 0);

    // -0.50
    auto h = Quantity::fromHundredths(-50);
    REQUIRE(h.wholeUnits() == -1);
    REQUIRE(h.hundredths() == 50);
}

TEST_CASE("Quantity: arithmetic") {
    auto a = Quantity::fromUnits(5);
    auto b = Quantity::fromHundredths(75);
    REQUIRE((a + b).raw() == 575);
    REQUIRE((a - b).raw() == 425);

    a += b;
    REQUIRE(a.raw() == 575);
    a -= b;
    REQUIRE(a.raw() == 500);
}

TEST_CASE("Quantity: unary minus and abs") {
    auto a = Quantity::fromHundredths(300);
    REQUIRE((-a).raw() == -300);
    REQUIRE((-a).abs().raw() == 300);
    REQUIRE(Quantity::zero().abs().raw() == 0);
}

TEST_CASE("Quantity: scalar multiply and divide") {
    auto cap = Quantity::fromUnits(50);
    REQUIRE((cap * 3).raw() == 15000);   // intersection.size * capacity
    REQUIRE((3 * cap).raw() == 15000);   // symmetric
    REQUIRE((cap / 4).raw() == 1250);    // quarter-of-a-car default threshold

    auto small = Quantity::fromHundredths(1);
    REQUIRE((small / 2).raw() == 0);   // truncation toward zero
}

TEST_CASE("Quantity: comparisons") {
    auto a = Quantity::fromHundredths(100);
    auto b = Quantity::fromHundredths(200);
    REQUIRE(a < b);
    REQUIRE(a <= b);
    REQUIRE(b > a);
    REQUIRE(b >= a);
    REQUIRE(a != b);
    REQUIRE(a == Quantity::fromUnits(1));
}

TEST_CASE("Quantity: std::min / std::max work via operator<") {
    auto a = Quantity::fromUnits(3);
    auto b = Quantity::fromUnits(7);
    REQUIRE(std::min(a, b).raw() == 300);
    REQUIRE(std::max(a, b).raw() == 700);
}
