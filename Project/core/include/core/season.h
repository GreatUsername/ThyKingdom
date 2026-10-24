// Seasons, as a label for the month that is passing.
//
// A season is not a block of days and it does not drive growth on its own: growth comes
// from the month curve in month_curve.h, which changes smoothly from month to month.
// Seasons remain useful as a human label - for a UI, for a test, for talking about the
// year - so they are derived from the month and nowhere else.
//
// This header is deliberately a leaf: it does not know about the calendar. Reading a
// season from an instant needs the calendar, so seasonAt() lives there.
#pragma once

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

}  // namespace core
