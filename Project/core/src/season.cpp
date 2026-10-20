#include "core/season.h"

namespace core {

Season seasonOfMonth(int month) {
    if (month >= 3 && month <= 5) {
        return Season::Spring;
    }
    if (month >= 6 && month <= 8) {
        return Season::Summer;
    }
    if (month >= 9 && month <= 11) {
        return Season::Autumn;
    }
    return Season::Winter;
}

}  // namespace core
