#pragma once

#include <cstdint>
#include <functional>

namespace dtl {

// Opaque handle. value==0 is the null sentinel, adapter maps <-> UE pointers/UClass*.
template <typename Tag>
struct Handle {
    uint64_t value = 0;

    constexpr bool valid() const { return value != 0; }
    constexpr bool operator==(Handle o) const { return value == o.value; }
    constexpr bool operator!=(Handle o) const { return value != o.value; }
    constexpr bool operator<(Handle o) const { return value < o.value; }
};

struct StationTag {};
struct TrainTag {};
struct OrderTag {};
struct ItemTag {};
struct NetworkTag {};

using StationId = Handle<StationTag>;
using TrainId   = Handle<TrainTag>;
using OrderId   = Handle<OrderTag>;
using ItemId    = Handle<ItemTag>;
using NetworkId = Handle<NetworkTag>;

struct BayRef {
    StationId station;
    int       index = 0;

    constexpr bool operator==(const BayRef& o) const { return station == o.station && index == o.index; }
};

}   // namespace dtl

namespace std {
template <typename Tag>
struct hash<dtl::Handle<Tag>> {
    size_t operator()(const dtl::Handle<Tag>& h) const noexcept { return std::hash<uint64_t>{}(h.value); }
};
template <>
struct hash<dtl::BayRef> {
    size_t operator()(const dtl::BayRef& r) const noexcept {
        return std::hash<uint64_t>{}(r.station.value) ^ (std::hash<int>{}(r.index) << 1);
    }
};
}   // namespace std
