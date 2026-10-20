// Seasons, derived from the calendar.
//
// Four seasons of 90 days each: spring covers days 1-90, summer 91-180, autumn
// 181-270, winter 271-360. Four equal seasons are one of the reasons the calendar
// has 360 days (OD-31): every boundary is exact integer arithmetic, so a step can
// be split exactly on it instead of straddling it.
#pragma once

#include <cstdint>

#include "core/clock.h"

namespace core {

enum class Season {
    Spring,
    Summer,
    Autumn,
    Winter,
};

constexpr std::int64_t kDaysPerSeason = 90;
constexpr std::int64_t kSeasonsPerYear = 4;
constexpr std::int64_t kUsPerSeason = kDaysPerSeason * kUsPerDay;

Season seasonAt(std::int64_t us);

// Game time until the next season boundary. Always in (0, kUsPerSeason].
std::int64_t usUntilNextSeason(std::int64_t us);

// What the season does to plants. Spring and summer grow them; autumn takes them
// back down; winter leaves them alone.
inline bool seasonGrows(Season s) { return s == Season::Spring || s == Season::Summer; }
inline bool seasonWithers(Season s) { return s == Season::Autumn; }

}  // namespace core
