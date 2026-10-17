// The World: the simulated entities and nothing else.
//
// No clock, no time, no control. The world does not know that time is passing; it
// is told to step, and it steps its systems in the order they were added. Whoever
// owns time - the Evaluator - owns the run.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "core/hash.h"
#include "core/rng.h"
#include "core/system.h"

namespace core {

class Clock;
struct Scenario;

class World {
public:
    World() = default;

    // Construction time only. Systems are added by buildWorld before the first
    // step; adding one mid-run would change the result of a run in flight and
    // break replay, so it is deliberately not part of the step loop.
    void addSystem(std::unique_ptr<System> system);

    // The clock arrives already advanced: systems compute the state of the new
    // instant, and read absolute time from it when their formulas need it.
    void step(Clock& clock, std::int64_t dt_us);

    // Mixes in this world's state: every system in insertion order, then the
    // world-level RNG. The caller owns the outer hash and decides what else goes
    // in, so the world never needs to know that a clock exists.
    void hash(Hash64& h) const;

    const std::vector<std::unique_ptr<System>>& systems() const { return m_systems; }
    Rng& rng() { return m_rng; }
    const Rng& rng() const { return m_rng; }

private:
    Rng m_rng;
    std::vector<std::unique_ptr<System>> m_systems;
};

// Creates the world and its systems from the scenario, in a declared order. The
// order of addSystem calls IS the order systems run in, and it is stated once,
// here, rather than emerging from however the systems were written.
World buildWorld(const Scenario& scenario);

}  // namespace core
