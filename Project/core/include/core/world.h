// The World: the simulated entities and the time they live in.
//
// The calendar lives here, because time is world state: two worlds at different
// instants are different worlds. Deciding *when* time moves is a matter of control, so
// that stays with the Evaluator - the world is told to step, and stepping is what moves
// time.
//
// A world is built by the evaluator that owns it, and there is no public factory:
// see buildWorld in evaluator.cpp.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "core/calendar.h"
#include "core/hash.h"
#include "core/rng.h"
#include "core/system.h"

namespace core {

class World {
public:
    // No default constructor on purpose: a world always begins at some instant, and
    // one that silently began at zero would be a world nobody asked for.
    explicit World(std::int64_t start_us) : m_calendar(start_us) {}

    // Construction time only. Systems are added while the world is being built, before
    // the first step; adding one mid-run would change the result of a run in flight and
    // break replay, so it is deliberately not part of the step loop.
    void addSystem(std::unique_ptr<System> system);

    // Advances the calendar, then steps every system in the order they were added.
    // Time moves first: a system computes the state of the new instant and reads
    // absolute time from the calendar when its formula needs it.
    void step(std::int64_t dt_us);

    // Mixes in this world's state: the instant first, then every system in insertion
    // order, then the world-level RNG. The order predates the calendar moving in here,
    // which is why the golden hash did not move with it.
    void hash(Hash64& h) const;

    const std::vector<std::unique_ptr<System>>& systems() const { return m_systems; }
    const Calendar& calendar() const { return m_calendar; }
    Rng& rng() { return m_rng; }
    const Rng& rng() const { return m_rng; }

private:
    Calendar m_calendar;
    Rng m_rng;
    std::vector<std::unique_ptr<System>> m_systems;
};

}  // namespace core
