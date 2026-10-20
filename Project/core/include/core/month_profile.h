// How fast plants grow, as a coefficient per month.
//
// The curve has one knot at the start of each month and is interpolated linearly
// between knots, wrapping from month 12 back to month 1. A knot is in hundredths:
// 100 means full rate, -100 means the plants lose growth at the base rate, 0 means
// they stand still.
//
// Why this shape matters more than the numbers: because every month has the same
// length in this calendar (OD-31), the integral of the curve has a closed form. So
// growth is a pure function of an instant rather than a running total, and two runs
// that arrive at the same instant agree no matter how they cut time on the way. A
// system whose state is a function of the clock needs no stepping at all - which is
// what makes skipping an acceleration span possible without iterating.
//
// Precision note: the integral is computed on a whole-second grid, and dividing by
// two is integer division. Both bias the absolute value slightly, but neither makes
// the result depend on the step size, because the bias is a property of the instant
// alone.
#pragma once

#include <cstdint>

#include "core/clock.h"

namespace core {

class MonthProfile {
public:
    MonthProfile();

    // Month is 1..12. Coefficient in hundredths.
    void setMonth(int month, std::int64_t coefficient);
    std::int64_t coefficient(int month) const;

    // Coefficient at an instant, interpolated between knots. Hundredths.
    std::int64_t at(std::int64_t us) const;

    // Integral of the coefficient from the start of year 1 to 'us', in
    // coefficient-seconds. A pure function of 'us'.
    std::int64_t cumulative(std::int64_t us) const;

private:
    std::int64_t knot(int index) const;                 // index 0..11, wraps
    std::int64_t wholeMonth(int index) const;           // coefficient-seconds
    std::int64_t partMonth(int index, std::int64_t offset_us) const;
    void recompute();

    std::int64_t m_knots[kMonthsPerYear];
    // Cumulative coefficient-seconds at each month start, and for a whole year.
    // Precomputed so that cumulative() is a lookup plus one small formula: a
    // system that is evaluated every step must not pay for a loop.
    std::int64_t m_month_start[kMonthsPerYear];
    std::int64_t m_year_total;
};

}  // namespace core
