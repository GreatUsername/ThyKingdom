#include "core/settings.h"

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <vector>

#include "core/ini.h"

namespace core {
namespace {

bool toInt(const std::string& s, int& out) {
    if (s.empty()) {
        return false;
    }
    const char* p = s.c_str();
    char* end = nullptr;
    errno = 0;
    const long v = std::strtol(p, &end, 10);
    if (errno != 0 || end == p || *end != '\0') {
        return false;
    }
    out = static_cast<int>(v);
    return true;
}

bool toBool(const std::string& s, bool& out) {
    int v = 0;
    if (!toInt(s, v)) {
        return false;
    }
    out = (v != 0);
    return true;
}

}  // namespace

bool operator==(const Settings& a, const Settings& b) {
    return a.brightness == b.brightness
        && a.fullscreen == b.fullscreen
        && a.master_volume == b.master_volume
        && a.key_pause == b.key_pause
        && a.key_accelerate == b.key_accelerate
        && a.key_end_turn == b.key_end_turn;
}

bool loadSettings(const std::string& path, Settings& out, std::string& error) {
    std::vector<IniEntry> entries;
    std::string parse_error;
    if (!parseIni(path, entries, parse_error)) {
        // No file is normal; an unreadable-but-present file is not.
        std::ifstream probe(path.c_str());
        if (!probe) {
            return true;
        }
        error = parse_error;
        return false;
    }

    for (const auto& e : entries) {
        const std::string full = e.section + "." + e.key;
        bool ok = true;
        if (full == "graphics.brightness") {
            ok = toInt(e.value, out.brightness);
        } else if (full == "graphics.fullscreen") {
            ok = toBool(e.value, out.fullscreen);
        } else if (full == "audio.master_volume") {
            ok = toInt(e.value, out.master_volume);
        } else if (full == "keys.pause") {
            out.key_pause = e.value;
        } else if (full == "keys.accelerate") {
            out.key_accelerate = e.value;
        } else if (full == "keys.end_turn") {
            out.key_end_turn = e.value;
        } else {
            error = "line " + std::to_string(e.line) + ": unknown key '" + full + "'";
            return false;
        }
        if (!ok) {
            error = "line " + std::to_string(e.line) + ": '" + e.value + "' has the wrong type";
            return false;
        }
    }
    return true;
}

bool saveSettings(const std::string& path, const Settings& settings, std::string& error) {
    std::ofstream out(path.c_str());
    if (!out) {
        error = "cannot write " + path;
        return false;
    }
    out << "# Written by the settings menu. Missing file means these defaults.\n\n";
    out << "[graphics]\n";
    out << "brightness = " << settings.brightness << "\n";
    out << "fullscreen = " << (settings.fullscreen ? 1 : 0) << "\n\n";
    out << "[audio]\n";
    out << "master_volume = " << settings.master_volume << "\n\n";
    out << "[keys]\n";
    out << "pause = " << settings.key_pause << "\n";
    out << "accelerate = " << settings.key_accelerate << "\n";
    out << "end_turn = " << settings.key_end_turn << "\n";
    if (!out) {
        error = "failed while writing " + path;
        return false;
    }
    return true;
}

}  // namespace core
