// Turns real elapsed time into a number of simulation steps.
//
// The step rate is a property of the simulation, not of the display. The refresh
// rate decides how often a picture is drawn, never how much game time passes: if it
// decided that, two players at 60 Hz and 144 Hz would get different simulations from
// the same seed, and a replay would only be valid on the machine that recorded it.
//
// So the scheduler accumulates real time and hands out whole steps of a fixed dt
// (OD-18). Its own wake-up rate is an implementation detail: it can be called as
// often as anything likes, and the totals come out the same because only the sum of
// elapsed time matters.
//
// This class reads no clock and starts no thread. It is given elapsed time and asked
// for steps, which is what makes it testable without either.
#pragma once

#include <cstdint>

namespace runtime {

class Scheduler {
public:
    explicit Scheduler(std::int64_t dt_us);

    std::int64_t dt_us() const { return m_dt_us; }

    // Real time that has passed since the last call.
    void addElapsed(std::int64_t real_us);

    // How many whole steps are due, consuming them. The fractional remainder is
    // kept, so a wake-up rate that does not divide dt loses nothing over time.
    std::int64_t takeDueSteps(std::int64_t max_steps);

    // Time accumulated but not yet turned into a step.
    std::int64_t pending_us() const { return m_pending_us; }

private:
    std::int64_t m_dt_us = 0;
    std::int64_t m_pending_us = 0;
};

}  // namespace runtime
