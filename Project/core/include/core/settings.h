// Gameplay options: what the player configured in the settings menu.
//
// These are read at launch and written back by the menu, so they live in a file of
// their own and outlive a run. They are the player's preferences, not the world's
// state, and the Evaluator may read them.
//
// One rule matters more than the fields: a setting that changed the outcome of a
// simulation would break replay (OD-34), because the run record contains the
// scenario and the signals, not the settings. So presentation options stay out of
// the simulation, and the test suite asserts that two runs with different settings
// produce the same hash. If a setting ever *should* affect the outcome, it belongs
// in the scenario, where a replay can see it.
#pragma once

#include <string>

namespace core {

struct Settings {
    // Presentation.
    int brightness = 100;  // percent
    bool fullscreen = false;
    int master_volume = 80;  // percent

    // Hotkeys, by name. Empty means "not bound".
    std::string key_pause = "Space";
    std::string key_accelerate = "Enter";
    std::string key_end_turn = "Tab";
};

bool operator==(const Settings& a, const Settings& b);
inline bool operator!=(const Settings& a, const Settings& b) { return !(a == b); }

// A missing file is not an error: it means "no preferences saved yet", and the
// defaults stand. A file that exists but is malformed *is* an error.
bool loadSettings(const std::string& path, Settings& out, std::string& error);

bool saveSettings(const std::string& path, const Settings& settings, std::string& error);

}  // namespace core
