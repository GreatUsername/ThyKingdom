#include "core/world.h"

namespace core {

void World::addSystem(std::unique_ptr<System> system) {
    if (system) {
        m_systems.push_back(std::move(system));
    }
}

void World::step(Clock& clock, std::int64_t dt_us) {
    for (const auto& system : m_systems) {
        system->step(clock, dt_us);
    }
}

void World::hash(Hash64& h) const {
    for (const auto& system : m_systems) {
        system->hash(h);
    }
    h.mixU64(m_rng.state());
}

}  // namespace core
