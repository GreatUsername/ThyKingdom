// Gameplay options: what the player configured in the settings menu.
//
// These are the player's preferences, not the world's state. They are read at
// launch, written back by the menu, and they outlive any run. Changing them
// happens while the game is paused or not running at all, so nothing in the
// simulation needs to react to a change.
//
// The layering is the point: this header lives in the config library, which core
// does not depend on. A simulation file cannot name Settings, so a presentation
// option cannot reach the world and cannot break replay (OD-34). If an option
// ever *should* affect the outcome, it stops being a setting and becomes part of
// the scenario, where the run record can see it.
#pragma once

#include <string>

namespace config {

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

}  // namespace config
