#include "core/evaluator.h"

#include "core/crops.h"
#include "core/scenario.h"

namespace core {

// The generating mechanism, internal to this file.
//
// It is not declared in a header on purpose. A world exists inside the evaluator
// that owns it and nowhere else, so it cannot be created by one caller and then
// dropped by another; the only way to have one is to have an evaluator.
static World buildWorld(const Scenario& scenario) {
    World world;
    world.addSystem(std::unique_ptr<System>(new CropSystem(scenario.crop_rate)));
    world.rng().reseed(scenario.seed, 1);
    return world;
}

Evaluator::Evaluator(const Scenario& scenario) : m_world(buildWorld(scenario)) {}

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
    return std::unique_ptr<IEvaluator>(new Evaluator(scenario));
}

}  // namespace core
