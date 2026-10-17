// PCG32: small, fast, deterministic, seedable per entity.
//
// Mechanic 114: the seed composition decides the character of a unit. A seed
// taken from the unit's guid gives a stable personality; a seed taken from guid
// and time gives a changing mood. Both are reproducible, which is the point of
// OD-34.
//
// Deliberately NOT advanced once per step: see World::step.
#pragma once

#include <cstdint>

namespace core {

class Rng {
public:
    explicit Rng(std::uint64_t seed = 0, std::uint64_t stream = 1) { reseed(seed, stream); }

    void reseed(std::uint64_t seed, std::uint64_t stream = 1) {
        m_state = 0;
        m_inc = (stream << 1u) | 1u;
        next();
        m_state += seed;
        next();
    }

    std::uint32_t next() {
        const std::uint64_t old = m_state;
        m_state = old * 6364136223846793005ULL + m_inc;
        const std::uint32_t xorshifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
        const std::uint32_t rot = static_cast<std::uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
    }

    // Uniform integer in [0, bound).
    std::uint32_t below(std::uint32_t bound) {
        return bound == 0 ? 0 : next() % bound;
    }

    std::uint64_t state() const { return m_state; }

private:
    std::uint64_t m_state = 0;
    std::uint64_t m_inc = 1;
};

}  // namespace core
