// Headless scenario runner.
//
// This is a development tool, not the game: it drives the simulation with no
// picture and no settings. Everything the tests need, a bug report needs too.
#include <cstdio>
#include <cstring>
#include <string>

#include "core/clock.h"
#include "core/evaluator.h"
#include "core/scenario.h"

static void printJson(const core::Scenario& sc, const core::IEvaluator& ev, std::uint64_t hash,
                      bool match) {
    const core::CalendarDate d = core::toCalendar(ev.clock().now_us());
    std::printf(
        "{\"hash\":%llu,\"now_us\":%lld,\"steps\":%lld,\"dt_us\":%lld,"
        "\"year\":%lld,\"month\":%d,\"day\":%d,\"hour\":%d,"
        "\"systems\":%zu,\"accelerating\":%s,"
        "\"expect\":%llu,\"match\":%s}\n",
        static_cast<unsigned long long>(hash),
        static_cast<long long>(ev.clock().now_us()),
        static_cast<long long>(sc.steps),
        static_cast<long long>(sc.dt_us),
        static_cast<long long>(d.year),
        d.month,
        d.day,
        d.hour,
        ev.world().systems().size(),
        ev.mode() == core::Mode::Accelerating ? "true" : "false",
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

    std::unique_ptr<core::IEvaluator> ev = core::buildEvaluator(scenario);
    for (std::int64_t i = 0; i < scenario.steps; ++i) {
        ev->advance(scenario.dt_us);
    }

    const std::uint64_t hash = ev->hash();
    const bool match = (scenario.expect_hash == 0) || (hash == scenario.expect_hash);

    if (json) {
        printJson(scenario, *ev, hash, match);
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
