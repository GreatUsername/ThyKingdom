// Temperature, as a function of the clock.
//
// The same shape as PlantSystem, and for the same reason: the temperature at an
// instant is a property of that instant, not of the history that led to it. A curve
// with one knot per month, interpolated linearly between knots, gives the value
// directly, so there is nothing to accumulate and nothing that depends on the step
// size.
//
// The curve is kept in hundredths of a degree so that the arithmetic stays exact in
// integers and the hash can cover the value without rounding.
//
// Only temperature is modelled here. Precipitation, wind, and any effect weather has
// on the world are separate concerns; in particular crops do not read this system
// yet, and wiring them together is a decision, not a detail.
#pragma once

#include <cstdint>

#include "core/hash.h"
#include "core/month_profile.h"
#include "core/system.h"

namespace core {

class WeatherSystem final : public System {
public:
    explicit WeatherSystem(MonthProfile profile);

    // The signature has to accept dt, but the temperature does not use it: the value
    // is a function of the instant, so the step only decides *when* it is refreshed.
    void step(Clock& clock, std::int64_t dt_us) override;
    void hash(Hash64& h) const override;

    // Hundredths of a degree.
    std::int64_t temperature() const { return m_temperature; }

private:
    MonthProfile m_profile;
    std::int64_t m_temperature = 0;
};

}  // namespace core
