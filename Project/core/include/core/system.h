// A simulation subsystem.
//
// The world is a composition of subsystems behind this interface (06 Simulation loop).
// Two things matter for reproducibility:
//
//   * step() receives the calendar, not just dt. A closed-form system needs the
//     absolute time to be dt-exact, and dt alone cannot reconstruct it.
//   * hash() takes the hash by reference and mixes in its own state, so a subsystem
//     that grows a field cannot forget to cover it in one central list.
#pragma once

#include <cstdint>

namespace core {

class Calendar;
class Hash64;

class System {
public:
    virtual ~System() = default;

    // A stable label for this system. It names profiling blocks and says which
    // system is meant in a diagnostic. It is not world state and does not enter the
    // hash: renaming a system must not change a run.
    virtual const char* name() const = 0;

    virtual void step(Calendar& calendar, std::int64_t dt_us) = 0;
    virtual void hash(Hash64& h) const = 0;
};

}  // namespace core
