#include "core/world.h"

#include "core/crops.h"
#include "core/scenario.h"

namespace core {

void World::addSystem(std::unique_ptr<System> system) {
    if (system) {
        m_systems.push_back(std::move(system));
    }
}

void World::step(std::int64_t dt_us) {
    // Time advances first, then systems compute the state at the new instant.
    // Systems that need absolute time read it from the clock they are handed.
    m_clock.advance(dt_us);
    for (const auto& system : m_systems) {
        system->step(m_clock, dt_us);
    }
}

std::uint64_t World::hash() const {
    Hash64 h;
    h.mixI64(m_clock.now_us());
    for (const auto& system : m_systems) {
        system->hash(h);
    }
    h.mixU64(m_rng.state());
    return h.value();
}

World buildWorld(const Scenario& scenario) {
    World world;

    // The order of addSystem calls is the order systems run in, and it has to be
    // stated once, here, rather than emerge from however the systems were
    // written. Concrete order is still open in 06; with one system it is trivial.
    world.addSystem(std::unique_ptr<System>(new CropSystem(scenario.crop_rate)));

    world.rng().reseed(scenario.seed, 1);
    return world;
}

}  // namespace core
