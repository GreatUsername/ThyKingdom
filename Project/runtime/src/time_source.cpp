#include "runtime/time_source.h"

#include <chrono>
#include <thread>

namespace runtime {
namespace {

using Clock = std::chrono::steady_clock;

Clock::time_point g_start = Clock::now();

}  // namespace

std::int64_t SteadyTimeSource::now_us() const {
    const auto delta = Clock::now() - g_start;
    return std::chrono::duration_cast<std::chrono::microseconds>(delta).count();
}

void SteadyTimeSource::sleep_us(std::int64_t us) {
    if (us > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(us));
    }
}

}  // namespace runtime
