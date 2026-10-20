#include "core/season.h"

namespace core {

Season seasonAt(std::int64_t us) {
    // Time starts at zero, so the index is never negative and a plain modulo is
    // enough. The cast keeps the arithmetic in the calendar's own units.
    const std::int64_t index = (us / kUsPerSeason) % kSeasonsPerYear;
    switch (index) {
    case 0:
        return Season::Spring;
    case 1:
        return Season::Summer;
    case 2:
        return Season::Autumn;
    default:
        return Season::Winter;
    }
}

std::int64_t usUntilNextSeason(std::int64_t us) {
    return kUsPerSeason - (us % kUsPerSeason);
}

}  // namespace core
