// Accumulates a rate over time without losing the fractional part.
//
// The carry is what makes the total independent of how the span was split into
// steps: the sum over any split equals the same total. Removing it would make the
// result depend on dt, which is exactly the failure OD-33 is about.
#pragma once

#include <cstdint>

namespace core {

struct Accumulator {
    static constexpr std::int64_t kMicro = 1'000'000;

    std::int64_t whole = 0;
    std::int64_t carry = 0;  // micro-units, 0..999999

    // Requires rate_micro_per_s * dt_us + carry to fit in int64. With dt in the
    // range OD-33 implies (tens of seconds) this holds with orders to spare.
    void advance(std::int64_t rate_micro_per_s, std::int64_t dt_us) {
        const std::int64_t num = rate_micro_per_s * dt_us + carry;
        whole += num / kMicro;
        carry = num % kMicro;
    }
};

}  // namespace core
