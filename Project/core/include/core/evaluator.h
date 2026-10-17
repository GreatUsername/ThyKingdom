// The Evaluator: the class that controls a run.
//
// It sits above the world. It owns the simulation clock, accumulates incoming
// control signals, and advances time. The world below it holds only simulated
// entities and knows nothing about time or control - which is the point of the
// split: the clock is state-control duty, so it belongs to whoever controls state.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/clock.h"
#include "core/control_signal.h"
#include "core/hash.h"
#include "core/world.h"

namespace core {

struct Scenario;

class Evaluator {
public:
    Evaluator() = default;
    Evaluator(World world, Clock clock);

    // One step: apply what arrived, advance time, then let the world compute the
    // new state. This is the only place simulation time moves.
    void advance(std::int64_t dt_us);

    // Accepted at any time, applied at the next boundary.
    void pushSignal(const ControlSignal& signal);
    std::size_t pendingSignals() const { return m_pending.size(); }

    // Covers the world, not the run: the clock first, then the world in its own
    // order. Step count and dt are absent on purpose.
    std::uint64_t hash() const;

    const Clock& clock() const { return m_clock; }
    const World& world() const { return m_world; }
    World& world() { return m_world; }

private:
    void applyPendingSignals();

    Clock m_clock;
    World m_world;
    std::vector<ControlSignal> m_pending;
};

// The generating mechanism: creates the clock, builds the world and its systems,
// and returns a run ready to advance.
Evaluator buildEvaluator(const Scenario& scenario);

}  // namespace core
