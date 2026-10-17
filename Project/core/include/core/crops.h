// Crops: the first real subsystem, and the first place M1 content will grow.
//
// A crop currently holds a constant rate. In the object model to come, that rate
// comes from the crop's type through its static data (type-id), not from a field
// beside the accumulator.
#pragma once

#include <cstdint>

#include "core/accumulator.h"
#include "core/hash.h"
#include "core/system.h"

namespace core {

class CropSystem final : public System {
public:
    explicit CropSystem(std::int64_t rate_micro_per_s) : m_rate(rate_micro_per_s) {}

    void step(Clock& clock, std::int64_t dt_us) override {
        (void)clock;  // constant rate for now; growth by season will read the clock
        m_growth.advance(m_rate, dt_us);
    }

    void hash(Hash64& h) const override {
        h.mixI64(m_growth.whole);
        h.mixI64(m_growth.carry);
    }

    std::int64_t grown() const { return m_growth.whole; }
    std::int64_t carry() const { return m_growth.carry; }

private:
    Accumulator m_growth;
    std::int64_t m_rate = 0;
};

}  // namespace core
