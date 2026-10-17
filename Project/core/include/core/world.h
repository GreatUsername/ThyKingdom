// The world: time, one growing crop, and the RNG.
//
// This is a skeleton, not the game. It exists so that the harness (scenario,
// hash, runner, tests) can be built and verified before there is any content to
// verify. M1 content - weather, seasons, withering, movement, collisions - is
// added on top of this shape.
#pragma once

#include <cstdint>

#include "core/hash.h"
#include "core/rng.h"

namespace core {

// Accumulates a rate over time without losing the fractional part.
//
// The carry is what makes the total independent of how the span was split into
// steps: the sum over any split equals the same total. Removing it would make the
// result depend on dt, which is exactly the failure OD-33 is about.
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

struct World {
    std::int64_t now_us = 0;
    Accumulator crop;
    std::int64_t crop_rate = 0;  // micro-units per second
    Rng rng;
    std::uint64_t seed = 0;

    void step(std::int64_t dt_us) {
        now_us += dt_us;
        crop.advance(crop_rate, dt_us);

        // The RNG is deliberately not advanced here. Drawing once per step would
        // make the outcome depend on the number of steps and break dt invariance.
        // Randomness belongs to events, not to every tick.
    }

    std::uint64_t hash() const {
        Hash64 h;
        h.mixI64(now_us);
        h.mixI64(crop.whole);
        h.mixI64(crop.carry);
        h.mixU64(rng.state());
        return h.value();
    }
};

}  // namespace core
