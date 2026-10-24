#include "core/weather.h"

namespace core {

WeatherSystem::WeatherSystem(TemperatureCurve profile) : m_profile(profile) {}

void WeatherSystem::step(Calendar& calendar, std::int64_t dt_us) {
    // dt is deliberately unused. Temperature is a function of the instant, not a
    // quantity accumulated across steps, so the step only decides when the value is
    // refreshed - the same reasoning as PlantSystem, and the reason a whole span can
    // later be skipped instead of iterated.
    (void)dt_us;
    m_temperature = m_profile.at(calendar.now_us());
}

void WeatherSystem::hash(Hash64& h) const {
    h.mixI64(m_temperature);
}

}  // namespace core
