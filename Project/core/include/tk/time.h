// Game time and calendar.
//
// Game time is an integer count of microseconds since the start of year 1.
// Integers rather than floating point: the world state has to be hashable and
// reproducible, and an integer clock is exactly that.
//
// The calendar is deliberately simple: 12 months of 30 days, 24 hours a day,
// 360 days a year (OD-31). Every conversion is exact integer arithmetic, with no
// month-length table and no leap years. If real month lengths are ever wanted,
// the month table becomes data; the clock stays integer.
#pragma once

#include <cstdint>

namespace tk {

constexpr std::int64_t kUsPerSecond = 1'000'000;
constexpr std::int64_t kUsPerMinute = 60 * kUsPerSecond;
constexpr std::int64_t kUsPerHour = 60 * kUsPerMinute;
constexpr std::int64_t kUsPerDay = 24 * kUsPerHour;
constexpr std::int64_t kUsPerMonth = 30 * kUsPerDay;
constexpr std::int64_t kUsPerYear = 12 * kUsPerMonth;

constexpr int kMonthsPerYear = 12;
constexpr int kDaysPerMonth = 30;
constexpr int kDaysPerYear = kDaysPerMonth * kMonthsPerYear;

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

}  // namespace tk
