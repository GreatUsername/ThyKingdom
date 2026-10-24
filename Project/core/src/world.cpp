#include "core/world.h"

namespace core {

void World::addSystem(std::unique_ptr<System> system) {
    if (system) {
        m_systems.push_back(std::move(system));
    }
}

void World::step(std::int64_t dt_us) {
    // The month boundaries crossed are reported by the calendar and ignored here: no
    // system consumes them yet, and inventing a consumer would be a design decision.
    static_cast<void>(m_calendar.advance(dt_us));

    for (const auto& system : m_systems) {
        system->step(m_calendar, dt_us);
    }
}

void World::hash(Hash64& h) const {
    h.mixI64(m_calendar.now_us());
    for (const auto& system : m_systems) {
        system->hash(h);
    }
    h.mixU64(m_rng.state());
}

}  // namespace core
