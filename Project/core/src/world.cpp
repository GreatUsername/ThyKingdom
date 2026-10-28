#include "core/world.h"

#ifdef THYKINGDOM_PROFILE
#include <easy/profiler.h>
#endif

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
#ifdef THYKINGDOM_PROFILE
        // One named block per system, which is what makes a trace answer "which
        // system costs what" instead of "the step costs this much". The block ends
        // with the scope, so the timing covers exactly the step() call.
        const char* block_name = system->name();
        {
            EASY_BLOCK(block_name, profiler::colors::Blue);
            system->step(m_calendar, dt_us);
        }
#else
        system->step(m_calendar, dt_us);
#endif
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
