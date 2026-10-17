// Headless scenario runner.
//
// The pipeline is short on purpose: parse the scenario, build a world from it,
// step the world, print its hash. Everything the tests need, a bug report needs
// too.
#include <cstdio>
#include <cstring>
#include <string>

#include "core/clock.h"
#include "core/scenario.h"
#include "core/world.h"

static void printJson(const core::Scenario& sc, const core::World& w, std::uint64_t hash,
                      bool match) {
    const core::CalendarDate d = core::toCalendar(w.clock().now_us());
    std::printf(
        "{\"hash\":%llu,\"now_us\":%lld,\"steps\":%lld,\"dt_us\":%lld,"
        "\"year\":%lld,\"month\":%d,\"day\":%d,\"hour\":%d,"
        "\"systems\":%zu,\"expect\":%llu,\"match\":%s}\n",
        static_cast<unsigned long long>(hash),
        static_cast<long long>(w.clock().now_us()),
        static_cast<long long>(sc.steps),
        static_cast<long long>(sc.dt_us),
        static_cast<long long>(d.year),
        d.month,
        d.day,
        d.hour,
        w.systems().size(),
        static_cast<unsigned long long>(sc.expect_hash),
        match ? "true" : "false");
}

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

    core::Scenario scenario;
    std::string error;
    if (!core::loadScenario(argv[1], scenario, error)) {
        std::fprintf(stderr, "scenario error: %s\n", error.c_str());
        return 1;
    }

    core::World world = core::buildWorld(scenario);
    for (std::int64_t i = 0; i < scenario.steps; ++i) {
        world.step(scenario.dt_us);
    }

    const std::uint64_t hash = world.hash();
    const bool match = (scenario.expect_hash == 0) || (hash == scenario.expect_hash);

    if (json) {
        printJson(scenario, world, hash, match);
    } else {
        std::printf("%016llx\n", static_cast<unsigned long long>(hash));
    }

    if (!match) {
        std::fprintf(stderr, "hash mismatch: got %016llx, expected %016llx\n",
                     static_cast<unsigned long long>(hash),
                     static_cast<unsigned long long>(scenario.expect_hash));
        return 3;
    }
    return 0;
}
