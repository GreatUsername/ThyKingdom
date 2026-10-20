#include "core/plants.h"

namespace core {

PlantSystem::PlantSystem(std::int64_t growth_rate, std::int64_t wither_rate)
    : m_growth_rate(growth_rate), m_wither_rate(wither_rate) {}

void PlantSystem::sow(std::int32_t x, std::int32_t y) {
    Crop crop;
    crop.x = x;
    crop.y = y;
    m_crops.push_back(crop);
}

void PlantSystem::step(Clock& clock, std::int64_t dt_us) {
    if (dt_us <= 0 || m_crops.empty()) {
        return;
    }

    // The clock has already advanced, so this step covers [end - dt, end). Walking
    // that interval in season-sized slices is what keeps a step from straddling a
    // boundary: growth is applied with the rate of the season it actually happened
    // in, whichever way the caller chose to cut time.
    std::int64_t cursor = clock.now_us() - dt_us;
    std::int64_t remaining = dt_us;
    while (remaining > 0) {
        const Season season = seasonAt(cursor);
        const std::int64_t until_boundary = usUntilNextSeason(cursor);
        const std::int64_t slice = remaining < until_boundary ? remaining : until_boundary;
        applySeason(season, slice);
        cursor += slice;
        remaining -= slice;
    }
}

void PlantSystem::applySeason(Season season, std::int64_t slice_us) {
    if (seasonGrows(season)) {
        for (Crop& crop : m_crops) {
            crop.growth_sum += m_growth_rate * slice_us;
        }
        return;
    }

    if (seasonWithers(season)) {
        for (Crop& crop : m_crops) {
            crop.growth_sum -= m_wither_rate * slice_us;
            // Withering stops at zero. Clamping needs no boundary split: a coarse
            // step that overshoots the ground and a fine one that lands exactly on
            // it both leave the crop at zero, so the state agrees either way.
            if (crop.growth_sum < 0) {
                crop.growth_sum = 0;
            }
        }
        return;
    }

    // Winter: dormant. Nothing grows and nothing dies.
}

void PlantSystem::hash(Hash64& h) const {
    h.mixI64(static_cast<std::int64_t>(m_crops.size()));
    for (const Crop& crop : m_crops) {
        h.mixI64(static_cast<std::int64_t>(crop.x));
        h.mixI64(static_cast<std::int64_t>(crop.y));
        h.mixI64(crop.growth_sum);
    }
}

}  // namespace core
