// The Evaluator: the class that controls a run.
//
// It sits above the world. It owns the simulation clock and the acceleration mode,
// accumulates incoming control signals, and advances time. The world below it holds
// only simulated entities and knows nothing about time or control - the clock is
// state-control duty, so it belongs to whoever controls state.
//
// The abstract base exists so that the runner and, later, the UI talk to the run
// through a narrow interface rather than a concrete class. It is deliberately thin:
// what would vary between implementations is when advance() is called (a timer, a
// batch sweep, a replay), not what a step does.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "core/clock.h"
#include "core/control_signal.h"
#include "core/hash.h"
#include "core/settings.h"
#include "core/world.h"

namespace core {

struct Scenario;

// Normal time flows as it flows; acceleration skips a span the player chose
// (OD-15) and ends either when the span is exhausted or on an interrupt (OD-14).
enum class Mode {
    Normal,
    Accelerating,
};

class IEvaluator {
public:
    virtual ~IEvaluator() = default;

    // One step. This is the only place simulation time moves.
    virtual void advance(std::int64_t dt_us) = 0;
    virtual void pushSignal(const ControlSignal& signal) = 0;

    // Signals are queued until the next boundary. How many are waiting is part of
    // the interface because a UI wants to show it and a test wants to assert it.
    virtual std::size_t pendingSignals() const = 0;

    virtual std::uint64_t hash() const = 0;
    virtual const Clock& clock() const = 0;
    virtual const World& world() const = 0;

    virtual Mode mode() const = 0;
    virtual std::int64_t accelerationRemaining_us() const = 0;
    virtual const Settings& settings() const = 0;
};

class Evaluator final : public IEvaluator {
public:
    Evaluator(World world, Clock clock, Settings settings);

    void advance(std::int64_t dt_us) override;
    void pushSignal(const ControlSignal& signal) override;
    std::size_t pendingSignals() const override { return m_pending.size(); }

    std::uint64_t hash() const override;
    const Clock& clock() const override { return m_clock; }
    const World& world() const override { return m_world; }

    Mode mode() const override { return m_mode; }
    std::int64_t accelerationRemaining_us() const override { return m_acceleration_remaining_us; }
    const Settings& settings() const override { return m_settings; }

private:
    void applyPendingSignals();

    Clock m_clock;
    World m_world;
    Settings m_settings;
    std::vector<ControlSignal> m_pending;

    Mode m_mode = Mode::Normal;
    std::int64_t m_acceleration_remaining_us = 0;
};

// The generating mechanism: creates the clock, builds the world and its systems,
// and returns a run ready to advance.
std::unique_ptr<IEvaluator> buildEvaluator(const Scenario& scenario, const Settings& settings);

}  // namespace core
