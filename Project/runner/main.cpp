// Headless scenario runner.
//
// This is the only way the simulation is driven for now, and that is the point:
// everything the tests need, a bug report needs too. Output is either a bare hash
// (for scripts) or one JSON line (for Python).
#include <cstdio>
#include <cstring>
#include <string>

#include "tk/scenario.h"
#include "tk/time.h"

namespace {

void printJson(const tk::Scenario& sc, const tk::World& w, std::uint64_t hash, bool match) {
    const tk::CalendarDate d = tk::toCalendar(w.now_us);
    std::printf(
        "{\"hash\":%llu,\"now_us\":%lld,\"steps\":%lld,\"dt_us\":%lld,"
        "\"year\":%lld,\"month\":%d,\"day\":%d,\"hour\":%d,"
        "\"crop\":%lld,\"crop_carry\":%lld,\"expect\":%llu,\"match\":%s}\n",
        static_cast<unsigned long long>(hash),
        static_cast<long long>(w.now_us),
        static_cast<long long>(sc.steps),
        static_cast<long long>(sc.dt_us),
        static_cast<long long>(d.year),
        d.month,
        d.day,
        d.hour,
        static_cast<long long>(w.crop.whole),
        static_cast<long long>(w.crop.carry),
        static_cast<unsigned long long>(sc.expect_hash),
        match ? "true" : "false");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: %s <scenario> [--json]\n", argv[0]);
        return 2;
    }

    bool json = false;
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "--json") == 0) {
            json = true;
        }
    }

    tk::Scenario sc;
    std::string error;
    if (!tk::loadScenario(argv[1], sc, error)) {
        std::fprintf(stderr, "scenario error: %s\n", error.c_str());
        return 1;
    }

    tk::World w = sc.makeWorld();
    for (std::int64_t i = 0; i < sc.steps; ++i) {
        w.step(sc.dt_us);
    }

    const std::uint64_t hash = w.hash();
    const bool match = (sc.expect_hash == 0) || (hash == sc.expect_hash);

    if (json) {
        printJson(sc, w, hash, match);
    } else {
        std::printf("%016llx\n", static_cast<unsigned long long>(hash));
    }

    if (!match) {
        std::fprintf(stderr, "hash mismatch: got %016llx, expected %016llx\n",
                     static_cast<unsigned long long>(hash),
                     static_cast<unsigned long long>(sc.expect_hash));
        return 3;
    }
    return 0;
}
