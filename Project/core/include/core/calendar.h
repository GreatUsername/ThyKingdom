// The calendar: the single time type of the simulation.
//
// Game time is an integer count of microseconds since the start of year 1. Integers
// rather than floating point: world state has to be hashable and reproducible, and an
// integer calendar is exactly that.
//
// The calendar is world state, so the world owns it: two worlds at different instants
// are different worlds. Deciding *when* time moves is a matter of control, so that
// stays with the Evaluator, which moves time by telling the world to step.
//
// Systems never read machine time. They read this, which is why a run can be replayed:
// the calendar is the only source of "now" in the simulation.
#pragma once

#include <cstdint>

#include "core/season.h"

namespace core {

// 12 months of 30 days, 24 hours a day, 360 days a year (OD-31). Exact integer
// arithmetic, no month-length table, no leap years. If real month lengths are ever
// wanted, the month table becomes data; the calendar stays integer.
constexpr std::int64_t kUsPerSecond = 1'000'000;
constexpr std::int64_t kUsPerMinute = 60 * kUsPerSecond;
constexpr std::int64_t kUsPerHour = 60 * kUsPerMinute;
constexpr std::int64_t kUsPerDay = 24 * kUsPerHour;
constexpr std::int64_t kUsPerMonth = 30 * kUsPerDay;
constexpr std::int64_t kUsPerYear = 12 * kUsPerMonth;

constexpr std::int64_t kSecondsPerDay = 86'400;
constexpr std::int64_t kSecondsPerMonth = 30 * kSecondsPerDay;
constexpr std::int64_t kSecondsPerYear = 12 * kSecondsPerMonth;

constexpr int kMonthsPerYear = 12;
constexpr int kDaysPerMonth = 30;
constexpr int kDaysPerYear = kDaysPerMonth * kMonthsPerYear;

// Month 1..12 of the year the instant falls in.
inline int monthOf(std::int64_t us) {
    return static_cast<int>((us % kUsPerYear) / kUsPerMonth) + 1;
}

// Offset into the month the instant falls in.
inline std::int64_t monthOffset(std::int64_t us) { return us % kUsPerMonth; }

struct CalendarDate {
    std::int64_t year = 1;
    int month = 1;
    int day = 1;
    int hour = 0;
    int minute = 0;
    int second = 0;
    std::int64_t microsecond = 0;
};

inline CalendarDate toCalendar(std::int64_t us) {
    CalendarDate d;
    d.year = us / kUsPerYear + 1;
    std::int64_t rest = us % kUsPerYear;
    d.month = static_cast<int>(rest / kUsPerMonth) + 1;
    rest %= kUsPerMonth;
    d.day = static_cast<int>(rest / kUsPerDay) + 1;
    rest %= kUsPerDay;
    d.hour = static_cast<int>(rest / kUsPerHour);
    rest %= kUsPerHour;
    d.minute = static_cast<int>(rest / kUsPerMinute);
    rest %= kUsPerMinute;
    d.second = static_cast<int>(rest / kUsPerSecond);
    d.microsecond = rest % kUsPerSecond;
    return d;
}

inline std::int64_t fromCalendar(const CalendarDate& d) {
    return (d.year - 1) * kUsPerYear
         + static_cast<std::int64_t>(d.month - 1) * kUsPerMonth
         + static_cast<std::int64_t>(d.day - 1) * kUsPerDay
         + static_cast<std::int64_t>(d.hour) * kUsPerHour
         + static_cast<std::int64_t>(d.minute) * kUsPerMinute
         + static_cast<std::int64_t>(d.second) * kUsPerSecond
         + d.microsecond;
}

// Season of an instant, derived from the month it falls in. It lives here rather than
// in season.h because reading a season means reading the calendar; season.h stays a
// leaf so that this header can include it.
inline Season seasonAt(std::int64_t us) { return seasonOfMonth(monthOf(us)); }

class Calendar {
public:
    Calendar() = default;
    explicit Calendar(std::int64_t now_us) : m_now_us(now_us) {}

    std::int64_t now_us() const { return m_now_us; }
    int month() const { return monthOf(m_now_us); }
    Season season() const { return seasonOfMonth(monthOf(m_now_us)); }
    CalendarDate date() const { return toCalendar(m_now_us); }

    // Advances time and returns how many month boundaries were crossed. Zero on almost
    // every step; the caller decides what to do with it. A count rather than a callback:
    // it allocates nothing on the hot path, has no hidden invocation order, and is
    // trivial to test.
    std::int64_t advance(std::int64_t dt_us) {
        const std::int64_t months_before = m_now_us / kUsPerMonth;
        m_now_us += dt_us;
        return m_now_us / kUsPerMonth - months_before;
    }

private:
    std::int64_t m_now_us = 0;
};

}  // namespace core
