// A run description: the parsed, validated data a run is created from.
//
// This is data and nothing else. It is the input the evaluator builds a world from,
// and it also carries the runner's own parameters. A world is created from it and
// keeps none of it.
//
// One file serves four purposes: a test, a bug report, a replay and a balance run
// (Codex note 08). Reproducibility is what OD-34 rests on.
//
// The format is deliberately trivial - sections in brackets, "key = value" lines,
// integers only, '#' starts a comment. It is not JSON because the core takes no
// third-party dependencies, and Python reads and writes this just as easily.
#pragma once

#include <cstdint>
#include <string>

#include "core/clock.h"

namespace core {

struct Scenario {
    // What the world is generated from.
    std::uint64_t seed = 0;
    std::int64_t start_day = 1;  // 1-based; seasons follow from it

    // Crops sown at start, on a small deterministic grid until the map exists.
    std::int64_t crop_count = 0;
    std::int64_t crop_rate = 0;  // units per second at coefficient 100

    // Growth coefficient per month, hundredths. Interpolated linearly between the
    // knots at month starts.
    std::int64_t coefficient[kMonthsPerYear] = {};

    // Temperature at the start of each month, hundredths of a degree. Also
    // interpolated linearly between knots.
    std::int64_t temperature[kMonthsPerYear] = {};

    // What the run is.
    std::int64_t steps = 0;
    std::int64_t dt_us = 0;
    std::uint64_t expect_hash = 0;  // 0 means: do not check
};

// Returns false and fills 'error' on any problem, including unknown keys: a
// silently ignored typo in a scenario would produce a green test that tests
// nothing.
bool loadScenario(const std::string& path, Scenario& out, std::string& error);

}  // namespace core
