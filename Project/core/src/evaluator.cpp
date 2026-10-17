#include "core/evaluator.h"

#include <utility>

#include "core/scenario.h"

namespace core {

Evaluator::Evaluator(World world, Clock clock)
    : m_clock(clock), m_world(std::move(world)) {}

void Evaluator::pushSignal(const ControlSignal& signal) {
    m_pending.push_back(signal);
}

void Evaluator::applyPendingSignals() {
    // Nothing to apply yet: ControlSignal has no fields. The path exists so the
    // boundary rule is established before the first command does.
    m_pending.clear();
}

void Evaluator::advance(std::int64_t dt_us) {
    applyPendingSignals();
    m_clock.advance(dt_us);
    m_world.step(m_clock, dt_us);
}

std::uint64_t Evaluator::hash() const {
    Hash64 h;
    h.mixI64(m_clock.now_us());
    m_world.hash(h);
    return h.value();
}

Evaluator buildEvaluator(const Scenario& scenario) {
    return Evaluator(buildWorld(scenario), Clock());
}

}  // namespace core
