#include "core/plants.h"

namespace core {

PlantSystem::PlantSystem(std::int64_t base_rate, MonthProfile profile)
    : m_base_rate(base_rate), m_profile(profile) {}

void PlantSystem::sow(std::int32_t x, std::int32_t y, std::int64_t now_us) {
    Crop crop;
    crop.x = x;
    crop.y = y;
    crop.sowed_integral = m_profile.cumulative(now_us);
    crop.growth = 0;
    m_crops.push_back(crop);
}

std::int64_t PlantSystem::growthAt(std::int64_t sowed_integral, std::int64_t now_us) const {
    // Coefficient-seconds since sowing, scaled by the base rate and by the
    // hundredths of the coefficient.
    const std::int64_t span = m_profile.cumulative(now_us) - sowed_integral;
    std::int64_t grown = m_base_rate * span / 100;
    if (grown < 0) {
        grown = 0;
    }
    return grown;
}

void PlantSystem::step(Clock& clock, std::int64_t dt_us) {
    (void)dt_us;
    const std::int64_t now = clock.now_us();
    for (Crop& crop : m_crops) {
        crop.growth = growthAt(crop.sowed_integral, now);
    }
}

void PlantSystem::hash(Hash64& h) const {
    h.mixI64(static_cast<std::int64_t>(m_crops.size()));
    for (const Crop& crop : m_crops) {
        h.mixI64(static_cast<std::int64_t>(crop.x));
        h.mixI64(static_cast<std::int64_t>(crop.y));
        h.mixI64(crop.growth);
    }
}

}  // namespace core
