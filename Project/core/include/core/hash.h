// FNV-1a, 64 bit.
//
// Small, dependency free and good enough to notice that state changed. It is not
// a cryptographic hash and must not be used as one.
//
// The hash covers the WORLD, not the run: step count and dt are parameters of a
// run, not properties of the world, and including them would make two runs of the
// same span incomparable. This is what lets the dt sweep in note 08 work.
#pragma once

#include <cstdint>

class Hash64 {
public:
    void mixU64(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            m_value ^= static_cast<std::uint8_t>(v >> (i * 8));
            m_value *= kPrime;
        }
    }

    void mixI64(std::int64_t v) { mixU64(static_cast<std::uint64_t>(v)); }

    std::uint64_t value() const { return m_value; }

private:
    static constexpr std::uint64_t kOffset = 1469598103934665603ULL;
    static constexpr std::uint64_t kPrime = 1099511628211ULL;

    std::uint64_t m_value = kOffset;
};
