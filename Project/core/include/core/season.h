// Seasons, as a label for the month that is passing.
//
// A season is not a block of days and it does not drive growth on its own: growth
// comes from the month coefficient curve in MonthProfile, which changes smoothly
// from month to month. Seasons remain useful as a human label - for a UI, for a
// test, for talking about the year - so they are derived from the month here and
// nowhere else.
#pragma once

#include <cstdint>

#include "core/clock.h"

namespace core {

enum class Season {
    Winter,
    Spring,
    Summer,
    Autumn,
};

// Meteorological seasons by month: spring 3-5, summer 6-8, autumn 9-11, winter
// 12, 1 and 2.
Season seasonOfMonth(int month);

inline Season seasonAt(std::int64_t us) { return seasonOfMonth(monthOf(us)); }

}  // namespace core
