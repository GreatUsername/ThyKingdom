// The machine clock, behind a seam.
//
// Everything that reads real time goes through here, and nothing in core may
// include this header. That is what keeps two properties true at once: the
// simulation never sees a wall clock, and the pacing can be tested with synthetic
// time and no threads at all.
#pragma once

#include <cstdint>

namespace runtime {

class TimeSource {
public:
    virtual ~TimeSource() = default;

    virtual std::int64_t now_us() const = 0;
    virtual void sleep_us(std::int64_t us) = 0;
};

// Monotonic, so that a change of the system clock cannot jump the simulation
// forward or backwards.
class SteadyTimeSource final : public TimeSource {
public:
    std::int64_t now_us() const override;
    void sleep_us(std::int64_t us) override;
};

}  // namespace runtime
