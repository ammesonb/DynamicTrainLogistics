#pragma once

#include <cstdint>

namespace dtl {

// Fixed-point at 1/100. Solids use whole units, fluids use hundredths.
class Quantity {
public:
    static constexpr int64_t kScale = 100;

    constexpr Quantity() = default;

    static constexpr Quantity fromRaw(int64_t raw)          { Quantity q; q.raw_ = raw; return q; }
    static constexpr Quantity fromUnits(int64_t units)      { return fromRaw(units * kScale); }
    static constexpr Quantity fromHundredths(int64_t h)     { return fromRaw(h); }
    static constexpr Quantity zero()                        { return {}; }

    constexpr int64_t raw()        const { return raw_; }
    constexpr int64_t wholeUnits() const { return raw_ / kScale; }
    constexpr int64_t hundredths() const { return raw_ % kScale; }

    constexpr Quantity  operator+(Quantity o) const  { return fromRaw(raw_ + o.raw_); }
    constexpr Quantity  operator-(Quantity o) const  { return fromRaw(raw_ - o.raw_); }
    constexpr Quantity& operator+=(Quantity o)       { raw_ += o.raw_; return *this; }
    constexpr Quantity& operator-=(Quantity o)       { raw_ -= o.raw_; return *this; }

    constexpr bool operator==(Quantity o) const { return raw_ == o.raw_; }
    constexpr bool operator!=(Quantity o) const { return raw_ != o.raw_; }
    constexpr bool operator< (Quantity o) const { return raw_ <  o.raw_; }
    constexpr bool operator<=(Quantity o) const { return raw_ <= o.raw_; }
    constexpr bool operator> (Quantity o) const { return raw_ >  o.raw_; }
    constexpr bool operator>=(Quantity o) const { return raw_ >= o.raw_; }

private:
    int64_t raw_ = 0;
};

}  // namespace dtl
