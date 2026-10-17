// A run description: the one artefact that serves four purposes.
//
// It is a test, a bug report, a replay and a balance run at once (Codex note 08).
// Cheap now, expensive later, and reproducibility is what OD-34 rests on.
//
// The format is deliberately trivial - sections in brackets, "key = value" lines,
// integers only, '#' starts a comment. It is not JSON because the core takes no
// third-party dependencies, and Python reads and writes this just as easily.
//
// The command stream will be added here before M2. It is absent now on purpose:
// M1 has no player commands yet, and inventing a shape before there is a use for
// it would mean changing the format twice.
#pragma once

#include <cstdint>
#include <string>

#include "core/world.h"

namespace core {

struct Scenario {
    std::uint64_t seed = 0;
    std::int64_t crop_rate = 0;
    std::int64_t steps = 0;
    std::int64_t dt_us = 0;
    std::uint64_t expect_hash = 0;  // 0 means: do not check

    // The world a fresh run of this scenario starts from.
    World makeWorld() const;
};

// Returns false and fills 'error' on any problem, including unknown keys: a
// silently ignored typo in a scenario would produce a green test that tests
// nothing.
bool loadScenario(const std::string& path, Scenario& out, std::string& error);

}  // namespace core
