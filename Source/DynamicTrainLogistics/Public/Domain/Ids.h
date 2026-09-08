#pragma once

#include <cstdint>
#include <functional>

namespace dtl {

// Opaque handle. Adapter maps <-> UE pointers/UClass*.
template <typename Tag>
struct Handle {
    uint64_t value = 0;

    constexpr bool valid() const { return value != 0; }
    constexpr bool operator==(Handle o) const { return value == o.value; }
    constexpr bool operator!=(Handle o) const { return value != o.value; }
    constexpr bool operator< (Handle o) const { return value <  o.value; }
};

struct StationTag {};
struct TrainTag   {};
struct OrderTag   {};
struct ItemTag    {};

using StationId = Handle<StationTag>;
using TrainId   = Handle<TrainTag>;
using OrderId   = Handle<OrderTag>;
using ItemId    = Handle<ItemTag>;

}  // namespace dtl

namespace std {
template <typename Tag>
struct hash<dtl::Handle<Tag>> {
    size_t operator()(const dtl::Handle<Tag>& h) const noexcept {
        return std::hash<uint64_t>{}(h.value);
    }
};
}
