// Biological growth of plants, crops in particular.
//
// A crop is not a counter that only goes up. It sits on a piece of land, it grows
// while the season allows it, and it withers when the season turns. So the system
// holds a crop per sown cell and reads the clock, never a wall clock.
//
// Seasonal thresholds are handled the way OD-33 requires: a step that straddles a
// season boundary is split at the boundary, so the result does not depend on where
// the step edges happened to fall.
//
// Not here yet, and deliberately: maturity. A real crop stops growing at some point,
// and that cap needs the same split-at-the-crossing treatment seasons already get.
// Adding it without that would make growth depend on dt again.
#pragma once

#include <cstdint>
#include <vector>

#include "core/hash.h"
#include "core/season.h"
#include "core/system.h"

namespace core {

// One sown crop, at a map cell.
//
// The position is a plain cell index for now. When the first real object container
// arrives, guid and type-id take over, and the growth rate will come from the
// crop's type through static data rather than from the system.
struct Crop {
    std::int32_t x = 0;
    std::int32_t y = 0;

    // Growth is an exact integer sum of rate * dt over time, in units times
    // microseconds. Dividing is left to whoever needs a human-readable amount, so
    // no precision is lost as the sum grows. A whole-unit counter with a carry
    // would do for growth alone, but crops also shrink, and a signed sum handles
    // both directions without a sign convention for the carry.
    std::int64_t growth_sum = 0;

    // Growth in whole units.
    std::int64_t growth() const { return growth_sum / 1000000; }
};

class PlantSystem final : public System {
public:
    PlantSystem(std::int64_t growth_rate, std::int64_t wither_rate);

    void sow(std::int32_t x, std::int32_t y);

    void step(Clock& clock, std::int64_t dt_us) override;
    void hash(Hash64& h) const override;

    const std::vector<Crop>& crops() const { return m_crops; }
    std::int64_t growthRate() const { return m_growth_rate; }
    std::int64_t witherRate() const { return m_wither_rate; }

private:
    void applySeason(Season season, std::int64_t slice_us);

    std::vector<Crop> m_crops;
    std::int64_t m_growth_rate = 0;  // units per second
    std::int64_t m_wither_rate = 0;  // units per second, applied downwards
};

}  // namespace core
