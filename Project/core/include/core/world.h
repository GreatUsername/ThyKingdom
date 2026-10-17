// The world: a functional class that computes the in-game systems.
//
// It owns the clock and the subsystems, and it is the only thing that decides the
// order in which systems run. It holds no scenario data: parameters arrive in a
// Scenario and are consumed by buildWorld, so a world does not remember how it was
// born.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "core/clock.h"
#include "core/rng.h"
#include "core/system.h"

namespace core {

struct Scenario;

class World {
public:
    World() = default;

    // Construction time only. Systems are added by buildWorld before the first
    // step; adding one mid-run would change the result of a run in flight and
    // break replay, so it is deliberately not part of the step loop.
    void addSystem(std::unique_ptr<System> system);

    void step(std::int64_t dt_us);

    // Covers the world, not the run: the clock first, then every system in
    // insertion order, then the world-level RNG. Step count and dt are absent on
    // purpose, so two runs of the same span are comparable.
    std::uint64_t hash() const;

    const Clock& clock() const { return m_clock; }
    const std::vector<std::unique_ptr<System>>& systems() const { return m_systems; }
    Rng& rng() { return m_rng; }
    const Rng& rng() const { return m_rng; }

private:
    Clock m_clock;
    Rng m_rng;
    std::vector<std::unique_ptr<System>> m_systems;
};

// The generating mechanism: reads the scenario, creates the clock and the
// systems, initialises them from the given data, and returns a world ready to
// step. Everything that makes one run differ from another lives here.
World buildWorld(const Scenario& scenario);

}  // namespace core
