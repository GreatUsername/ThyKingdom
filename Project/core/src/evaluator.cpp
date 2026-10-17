#include "core/evaluator.h"

#include <utility>

#include "core/scenario.h"

namespace core {

Evaluator::Evaluator(World world, Clock clock) : m_clock(clock), m_world(std::move(world)) {}

void Evaluator::pushSignal(const ControlSignal& signal) {
    m_pending.push_back(signal);
}

void Evaluator::applyPendingSignals() {
    for (const auto& signal : m_pending) {
        switch (signal.kind) {
        case SignalKind::BeginAcceleration:
            m_mode = Mode::Accelerating;
            m_acceleration_remaining_us = signal.span_us;
            break;
        case SignalKind::InterruptAcceleration:
            // An interrupt ends acceleration and starts a new turn just as
            // completing the span does (OD-14).
            m_mode = Mode::Normal;
            m_acceleration_remaining_us = 0;
            break;
        }
    }
    m_pending.clear();
}

void Evaluator::advance(std::int64_t dt_us) {
    applyPendingSignals();
    m_clock.advance(dt_us);
    m_world.step(m_clock, dt_us);

    if (m_mode == Mode::Accelerating) {
        m_acceleration_remaining_us -= dt_us;
        if (m_acceleration_remaining_us <= 0) {
            m_acceleration_remaining_us = 0;
            m_mode = Mode::Normal;
        }
    }
}

std::uint64_t Evaluator::hash() const {
    Hash64 h;
    h.mixI64(m_clock.now_us());
    m_world.hash(h);
    return h.value();
}

std::unique_ptr<IEvaluator> buildEvaluator(const Scenario& scenario) {
    return std::unique_ptr<IEvaluator>(new Evaluator(buildWorld(scenario), Clock()));
}

}  // namespace core
