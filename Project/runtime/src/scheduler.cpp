#include "runtime/scheduler.h"

namespace runtime {

Scheduler::Scheduler(std::int64_t dt_us) : m_dt_us(dt_us) {}

void Scheduler::addElapsed(std::int64_t real_us) {
    if (real_us > 0) {
        m_pending_us += real_us;
    }
}

std::int64_t Scheduler::takeDueSteps(std::int64_t max_steps) {
    if (m_dt_us <= 0 || max_steps <= 0) {
        return 0;
    }

    const std::int64_t due = m_pending_us / m_dt_us;
    if (due <= 0) {
        return 0;
    }

    // A long stall must not be paid back all at once: running a thousand steps in
    // one wake-up takes longer than the stall did and the queue grows instead of
    // shrinking. Steps beyond the cap are dropped, so the game falls behind real
    // time - which is the honest outcome, and visible, unlike a spiral.
    const std::int64_t steps = due < max_steps ? due : max_steps;
    m_pending_us -= steps * m_dt_us;

    // When the cap was hit, the remaining whole steps are dropped rather than
    // queued: paying them back later would take longer than the stall did, and
    // the backlog would grow instead of shrinking. The game falls behind real
    // time, which is honest and visible; a spiral is neither.
    if (due > max_steps) {
        m_pending_us %= m_dt_us;
    }

    return steps;
}

}  // namespace runtime
