#include "runtime/timer.h"

namespace runtime {

Timer::Timer(Scheduler scheduler, TimeSource& time)
    : m_scheduler(scheduler), m_time(time), m_last_us(time.now_us()) {}

void Timer::attach(core::IEvaluator* evaluator) {
    m_evaluator = evaluator;
    // Time passed before the evaluator existed is not owed to it.
    m_last_us = m_time.now_us();
}

void Timer::collectElapsed() {
    const std::int64_t now = m_time.now_us();
    const std::int64_t elapsed = now - m_last_us;
    m_last_us = now;
    m_scheduler.addElapsed(elapsed);
}

std::int64_t Timer::poll(std::int64_t max_steps) {
    if (m_evaluator == nullptr) {
        return 0;
    }

    collectElapsed();
    const std::int64_t steps = m_scheduler.takeDueSteps(max_steps);
    for (std::int64_t i = 0; i < steps; ++i) {
        m_evaluator->makeStep(m_scheduler.dt_us());
    }
    return steps;
}

void Timer::waitForNextStep() {
    if (m_evaluator == nullptr) {
        return;
    }
    if (m_evaluator->mode() == core::Mode::Accelerating) {
        return;
    }

    const std::int64_t need = m_scheduler.dt_us() - m_scheduler.pending_us();
    if (need > 0) {
        m_time.sleep_us(need);
        collectElapsed();
    }
}

}  // namespace runtime
