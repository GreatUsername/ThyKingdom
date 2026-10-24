// A curve of monthly knots: one knot at the start of each month, interpolated
// linearly between knots, wrapping from month 12 back to month 1.
//
// The Tag parameter exists so that two curves of the same shape cannot be handed to
// the wrong system. Growth and temperature are the same machine with different
// meanings, and a tagged template makes a mix-up a compile error instead of a silent
// swap. A template has to live in a header, which is why this file is header-only.
//
// Why the shape matters more than the numbers: because every month has the same
// length in this calendar (OD-31), the integral of the curve has a closed form. A
// value derived from it is therefore a function of an instant rather than a running
// total, and two runs that arrive at the same instant agree no matter how they cut
// time on the way.
//
// Precision note: the integral is computed on a whole-second grid, and dividing by
// two is integer division. Both bias the absolute value slightly, but neither makes
// the result depend on the step size, because the bias is a property of the instant
// alone.
#pragma once

#include <cstdint>

#include "core/calendar.h"

namespace core {

struct GrowthTag {};
struct TemperatureTag {};

template <class Tag>
class MonthCurve {
public:
    MonthCurve() { recompute(); }

    // Building straight from the scenario array: the twelve-element loop that used to
    // be repeated at every call site lives here now.
    explicit MonthCurve(const std::int64_t (&knots)[kMonthsPerYear]) {
        for (int i = 0; i < kMonthsPerYear; ++i) {
            m_knots[i] = knots[i];
        }
        recompute();
    }

    // Month is 1..12. Coefficient in hundredths.
    void setMonth(int month, std::int64_t coefficient) {
        if (month >= 1 && month <= kMonthsPerYear) {
            m_knots[month - 1] = coefficient;
            recompute();
        }
    }

    std::int64_t coefficient(int month) const {
        if (month < 1 || month > kMonthsPerYear) {
            return 0;
        }
        return m_knots[month - 1];
    }

    // Coefficient at an instant, interpolated between knots. Hundredths.
    std::int64_t at(std::int64_t us) const {
        const int month = monthOf(us) - 1;
        const std::int64_t offset = monthOffset(us);
        const std::int64_t from = knot(month);
        const std::int64_t to = knot(month + 1);
        return from + (to - from) * offset / kUsPerMonth;
    }

    // Integral of the curve from the start of year 1 to 'us', in
    // coefficient-seconds. A pure function of 'us'.
    std::int64_t cumulative(std::int64_t us) const {
        if (us <= 0) {
            return 0;
        }

        const std::int64_t years = us / kUsPerYear;
        const std::int64_t rest = us % kUsPerYear;
        const int month = static_cast<int>(rest / kUsPerMonth);

        return years * m_year_total
             + m_month_start[month]
             + partMonth(month, rest % kUsPerMonth);
    }

private:
    static int clampIndex(int index) {
        return ((index % kMonthsPerYear) + kMonthsPerYear) % kMonthsPerYear;
    }

    std::int64_t knot(int index) const {
        return m_knots[clampIndex(index)];
    }

    // Trapezoid over a whole month: (a + b) / 2 * length, in seconds.
    std::int64_t wholeMonth(int index) const {
        const std::int64_t from = knot(index);
        const std::int64_t to = knot(index + 1);
        return (from + to) * kSecondsPerMonth / 2;
    }

    // Integral of a straight line from the month start: a*u + (b-a)*u^2/(2L), with u
    // measured in whole seconds.
    std::int64_t partMonth(int index, std::int64_t offset_us) const {
        const std::int64_t u = offset_us / kUsPerSecond;
        const std::int64_t from = knot(index);
        const std::int64_t slope = knot(index + 1) - from;
        return from * u + slope * u * u / (2 * kSecondsPerMonth);
    }

    void recompute() {
        std::int64_t acc = 0;
        for (int i = 0; i < kMonthsPerYear; ++i) {
            m_month_start[i] = acc;
            acc += wholeMonth(i);
        }
        m_year_total = acc;
    }

    std::int64_t m_knots[kMonthsPerYear] = {};
    // Cumulative coefficient-seconds at each month start, and for a whole year.
    // Precomputed so that cumulative() is a lookup plus one small formula: a system
    // that is evaluated every step must not pay for a loop.
    std::int64_t m_month_start[kMonthsPerYear] = {};
    std::int64_t m_year_total = 0;
};

using GrowthCurve = MonthCurve<GrowthTag>;
using TemperatureCurve = MonthCurve<TemperatureTag>;

}  // namespace core
