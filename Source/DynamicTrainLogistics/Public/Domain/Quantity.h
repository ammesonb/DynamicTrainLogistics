#pragma once

#include <cstdint>

namespace dtl {

// Fixed-point at 1/100. Solids use whole units, fluids use hundredths.
// Divisions round toward zero, while wholeUnits/hundredths use floor semantics so
// negative values display cleanly (e.g. -1.50 = whole:-2 hund:50 -> "-1.50" formatting).
class Quantity {
public:
    static constexpr int64_t kScale = 100;

    constexpr Quantity() = default;

    static constexpr Quantity fromRaw(int64_t raw) {
        Quantity q;
        q.raw_ = raw;
        return q;
    }
    static constexpr Quantity fromUnits(int64_t units) { return fromRaw(units * kScale); }
    static constexpr Quantity fromHundredths(int64_t h) { return fromRaw(h); }
    static constexpr Quantity zero() { return {}; }

    constexpr int64_t raw() const { return raw_; }

    constexpr int64_t wholeUnits() const { return raw_ >= 0 ? raw_ / kScale : (raw_ - kScale + 1) / kScale; }
    constexpr int64_t hundredths() const { return raw_ - wholeUnits() * kScale; }

    constexpr Quantity  operator+(Quantity o) const { return fromRaw(raw_ + o.raw_); }
    constexpr Quantity  operator-(Quantity o) const { return fromRaw(raw_ - o.raw_); }
    constexpr Quantity  operator-() const { return fromRaw(-raw_); }
    constexpr Quantity  operator*(int64_t n) const { return fromRaw(raw_ * n); }
    constexpr Quantity  operator/(int64_t n) const { return fromRaw(raw_ / n); }
    constexpr Quantity& operator+=(Quantity o) {
        raw_ += o.raw_;
        return *this;
    }
    constexpr Quantity& operator-=(Quantity o) {
        raw_ -= o.raw_;
        return *this;
    }

    constexpr bool operator==(Quantity o) const { return raw_ == o.raw_; }
    constexpr bool operator!=(Quantity o) const { return raw_ != o.raw_; }
    constexpr bool operator<(Quantity o) const { return raw_ < o.raw_; }
    constexpr bool operator<=(Quantity o) const { return raw_ <= o.raw_; }
    constexpr bool operator>(Quantity o) const { return raw_ > o.raw_; }
    constexpr bool operator>=(Quantity o) const { return raw_ >= o.raw_; }

    constexpr Quantity abs() const { return raw_ >= 0 ? *this : Quantity::fromRaw(-raw_); }

private:
    int64_t raw_ = 0;
};

constexpr Quantity operator*(int64_t n, Quantity q) {
    return q * n;
}

}   // namespace dtl
