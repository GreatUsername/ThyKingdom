#include "core/scenario.h"

#include <vector>

#include "core/ini.h"

namespace core {
namespace {

bool parseI64(const std::string& s, std::int64_t& out) {
    if (s.empty()) {
        return false;
    }
    std::size_t i = 0;
    bool negative = false;
    if (s[0] == '+' || s[0] == '-') {
        negative = s[0] == '-';
        i = 1;
    }
    if (i >= s.size()) {
        return false;
    }
    std::int64_t value = 0;
    for (; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
        value = value * 10 + (s[i] - '0');
    }
    out = negative ? -value : value;
    return true;
}

// "growth.m7" -> 7, "weather.m12" -> 12, otherwise 0.
int monthFromKey(const std::string& key, const char* section) {
    const std::string prefix = std::string(section) + ".m";
    if (key.size() <= prefix.size() || key.compare(0, prefix.size(), prefix) != 0) {
        return 0;
    }
    const std::string rest = key.substr(prefix.size());
    if (rest.size() != 1 && rest.size() != 2) {
        return 0;
    }
    std::int64_t month = 0;
    if (!parseI64(rest, month)) {
        return 0;
    }
    return (month >= 1 && month <= kMonthsPerYear) ? static_cast<int>(month) : 0;
}

}  // namespace

bool loadScenario(const std::string& path, Scenario& out, std::string& error) {
    std::vector<IniEntry> entries;
    std::string parse_error;
    if (!parseIni(path, entries, parse_error)) {
        error = parse_error;
        return false;
    }

    for (const auto& e : entries) {
        const std::string full = e.section + "." + e.key;
        std::int64_t iv = 0;
        if (!parseI64(e.value, iv)) {
            error = "line " + std::to_string(e.line) + ": '" + e.value + "' is not an integer";
            return false;
        }
        if (full == "scenario.seed") {
            out.seed = static_cast<std::uint64_t>(iv);
        } else if (full == "state.start_day") {
            out.start_day = iv;
        } else if (full == "state.crops") {
            out.crop_count = iv;
        } else if (full == "state.crop_rate") {
            out.crop_rate = iv;
        } else if (full == "run.steps") {
            out.steps = iv;
        } else if (full == "run.dt_us") {
            out.dt_us = iv;
        } else if (full == "expect.hash") {
            out.expect_hash = static_cast<std::uint64_t>(iv);
        } else {
            const int growth_month = monthFromKey(full, "growth");
            const int weather_month = monthFromKey(full, "weather");
            if (growth_month != 0) {
                out.coefficient[growth_month - 1] = iv;
            } else if (weather_month != 0) {
                out.temperature[weather_month - 1] = iv;
            } else {
                error = "line " + std::to_string(e.line) + ": unknown key '" + full + "'";
                return false;
            }
        }
    }

    if (out.steps <= 0) {
        error = "run.steps must be positive";
        return false;
    }
    if (out.dt_us <= 0) {
        error = "run.dt_us must be positive";
        return false;
    }
    if (out.start_day < 1) {
        error = "state.start_day must be 1 or later";
        return false;
    }
    if (out.crop_count < 0) {
        error = "state.crops must not be negative";
        return false;
    }
    return true;
}

}  // namespace core
