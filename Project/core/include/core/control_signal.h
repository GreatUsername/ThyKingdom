// A control signal: something that arrives from outside the simulation and asks
// the world to change.
//
// Signals are applied at a step boundary and never mid-step. A signal applied
// mid-step would make the result depend on when it happened to arrive, which is
// exactly the divergence OD-34 rules out.
//
// M1 has no commands yet, so this type carries no fields. It exists now so that
// the intake path is real and exercised before the first command exists: the
// boundary rule is easier to keep from the start than to retrofit.
#pragma once

namespace core {

struct ControlSignal {
    // Fields arrive with the first real command, before M2.
};

}  // namespace core
