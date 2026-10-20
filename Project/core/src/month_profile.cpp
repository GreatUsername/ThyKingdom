#include "core/month_profile.h"

namespace core {
namespace {

std::int64_t clampIndex(int index) {
    const int wrapped = ((index % kMonthsPerYear) + kMonthsPerYear) % kMonthsPerYear;
    return wrapped;
}

}  // namespace

MonthProfile::MonthProfile() : m_year_total(0) {
    for (int i = 0; i < kMonthsPerYear; ++i) {
        m_knots[i] = 0;
        m_month_start[i] = 0;
    }
    recompute();
}

void MonthProfile::recompute() {
    std::int64_t acc = 0;
    for (int i = 0; i < kMonthsPerYear; ++i) {
        m_month_start[i] = acc;
        acc += wholeMonth(i);
    }
    m_year_total = acc;
}

void MonthProfile::setMonth(int month, std::int64_t coefficient) {
    if (month >= 1 && month <= kMonthsPerYear) {
        m_knots[month - 1] = coefficient;
        recompute();
    }
}

std::int64_t MonthProfile::coefficient(int month) const {
    if (month < 1 || month > kMonthsPerYear) {
        return 0;
    }
    return m_knots[month - 1];
}

std::int64_t MonthProfile::knot(int index) const {
    return m_knots[clampIndex(index)];
}

std::int64_t MonthProfile::at(std::int64_t us) const {
    const int month = monthOf(us) - 1;
    const std::int64_t offset = monthOffset(us);
    const std::int64_t from = knot(month);
    const std::int64_t to = knot(month + 1);
    return from + (to - from) * offset / kUsPerMonth;
}

std::int64_t MonthProfile::wholeMonth(int index) const {
    // Trapezoid over a whole month: (a + b) / 2 * length, in seconds.
    const std::int64_t from = knot(index);
    const std::int64_t to = knot(index + 1);
    return (from + to) * kSecondsPerMonth / 2;
}

std::int64_t MonthProfile::partMonth(int index, std::int64_t offset_us) const {
    // Integral of a straight line from the month start: a*u + (b-a)*u^2/(2L),
    // with u measured in whole seconds.
    const std::int64_t u = offset_us / kUsPerSecond;
    const std::int64_t from = knot(index);
    const std::int64_t slope = knot(index + 1) - from;
    return from * u + slope * u * u / (2 * kSecondsPerMonth);
}

std::int64_t MonthProfile::cumulative(std::int64_t us) const {
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

}  // namespace core
