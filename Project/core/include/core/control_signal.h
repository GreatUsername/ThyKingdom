// A control signal: something that arrives from outside the simulation and asks
// the evaluator to change something.
//
// Signals are accepted at any time and applied at a step boundary, never mid-step.
// A signal applied mid-step would make the result depend on when it happened to
// arrive, which is exactly the divergence OD-34 rules out.
//
// The first signals are the acceleration controls, because the evaluator is what
// switches the run between normal and accelerating (OD-14, OD-15). Player orders
// join this type when they exist, before M2 - and they will be refused while
// accelerating, which is a rule OD-14 states and the evaluator enforces.
#pragma once

#include <cstdint>

namespace core {

enum class SignalKind {
    BeginAcceleration,
    InterruptAcceleration,
};

struct ControlSignal {
    SignalKind kind = SignalKind::InterruptAcceleration;
    std::int64_t span_us = 0;  // how much game time to skip; used by BeginAcceleration
};

}  // namespace core
