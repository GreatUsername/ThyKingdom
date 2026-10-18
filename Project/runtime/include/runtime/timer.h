// The timer: decides when the evaluator makes a step.
//
// Created after the evaluator, because it signals one that already exists, so the
// link is set by attach() rather than by the constructor. Until attached, poll()
// does nothing.
//
// Two things it deliberately does not do:
//   * it does not choose dt - that comes from the run data (OD-18), and the
//     scheduler only decides how many steps are due;
//   * it does not pace acceleration - a skipped span is not paced by real time, so
//     while the evaluator is accelerating the timer does not sleep at all.
//
// No thread lives here yet. The loop is poll(), and something must call it: a
// thread in the real game, a plain loop in a test. Keeping the thread out means the
// pacing is testable with synthetic time instead of sleeps and guesswork.
#pragma once

#include <cstdint>

#include "core/evaluator.h"
#include "runtime/scheduler.h"
#include "runtime/time_source.h"

namespace runtime {

class Timer {
public:
    Timer(Scheduler scheduler, TimeSource& time);

    // Late binding, in the order the pieces are created.
    void attach(core::IEvaluator* evaluator);
    bool attached() const { return m_evaluator != nullptr; }

    // Accounts for real time passed, runs the steps that are due, and returns how
    // many ran. max_steps bounds the catch-up after a stall.
    std::int64_t poll(std::int64_t max_steps);

    // Sleeps until the next step is due. Returns immediately while accelerating,
    // because acceleration is not paced by real time.
    void waitForNextStep();

    const Scheduler& scheduler() const { return m_scheduler; }

private:
    void collectElapsed();

    Scheduler m_scheduler;
    TimeSource& m_time;
    core::IEvaluator* m_evaluator = nullptr;
    std::int64_t m_last_us = 0;
};

}  // namespace runtime
