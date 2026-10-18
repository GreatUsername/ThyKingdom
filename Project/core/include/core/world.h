// The World: the simulated entities and nothing else.
//
// No clock, no time, no control. The world does not know that time is passing; it
// is told to step, and it steps its systems in the order they were added. Whoever
// owns time - the Evaluator - owns the run.
//
// A world is built by the evaluator that owns it, and there is no public factory:
// see buildWorld in evaluator.cpp.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "core/hash.h"
#include "core/rng.h"
#include "core/system.h"

namespace core {

class Clock;

class World {
public:
    World() = default;

    // Construction time only. Systems are added while the world is being built,
    // before the first step; adding one mid-run would change the result of a run
    // in flight and break replay, so it is deliberately not part of the step loop.
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

}  // namespace core
