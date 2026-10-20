// Biological growth of plants, crops in particular.
//
// A crop sits on a piece of land and its growth is a function of the clock: the
// integral of the month coefficient curve since it was sown, scaled by the base
// rate, and never below zero. Nothing accumulates step by step.
//
// That is the whole point of the shape. Two runs that reach the same instant hold
// the same crops whether they walked there in one step or a million, because the
// state is evaluated, not accumulated. And a system whose state is a function of an
// instant can in principle be skipped entirely during acceleration: only the
// evaluation times matter.
//
// Not here yet: death. A withered crop in this model is a crop whose net integral
// has fallen to zero or below; it does not remember that and cannot stay dead while
// the season oscillates. Real death needs a state of its own and therefore an event
// to flip it, which is the non-linear part of the picture.
#pragma once

#include <cstdint>
#include <vector>

#include "core/hash.h"
#include "core/month_profile.h"
#include "core/system.h"

namespace core {

struct Crop {
    std::int32_t x = 0;
    std::int32_t y = 0;
    // The profile integral at sowing, captured once. Growth is then the integral
    // at the current instant minus this, so a step evaluates one curve instead
    // of two.
    std::int64_t sowed_integral = 0;
    std::int64_t growth = 0;  // units, evaluated from the clock
};

class PlantSystem final : public System {
public:
    PlantSystem(std::int64_t base_rate, MonthProfile profile);

    void sow(std::int32_t x, std::int32_t y, std::int64_t now_us);

    // The signature has to accept dt, but growth does not use it: the state is a
    // function of the clock, so the step only decides *when* the value is refreshed.
    void step(Clock& clock, std::int64_t dt_us) override;
    void hash(Hash64& h) const override;

    const std::vector<Crop>& crops() const { return m_crops; }
    std::int64_t baseRate() const { return m_base_rate; }
    const MonthProfile& profile() const { return m_profile; }

private:
    std::int64_t growthAt(std::int64_t sowed_integral, std::int64_t now_us) const;

    std::vector<Crop> m_crops;
    std::int64_t m_base_rate = 0;  // units per second at coefficient 100
    MonthProfile m_profile;
};

}  // namespace core
