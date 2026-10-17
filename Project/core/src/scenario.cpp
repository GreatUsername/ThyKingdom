#include "core/scenario.h"

#include <fstream>

static std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const std::size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) {
        return std::string();
    }
    const std::size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

static bool parseI64(const std::string& s, std::int64_t& out) {
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

World Scenario::makeWorld() const {
    World w;
    w.seed = seed;
    w.crop_rate = crop_rate;
    w.rng.reseed(seed, 1);
    return w;
}

bool loadScenario(const std::string& path, Scenario& out, std::string& error) {
    std::ifstream in(path.c_str());
    if (!in) {
        error = "cannot open " + path;
        return false;
    }

    std::string section;
    std::string line;
    int line_no = 0;

    while (std::getline(in, line)) {
        ++line_no;

        // A BOM is not content. Scenario files get edited in Notepad and in
        // PowerShell, both of which can prepend one, and a BOM must not turn the
        // first line into a syntax error.
        if (line_no == 1 && line.size() >= 3
            && static_cast<unsigned char>(line[0]) == 0xEF
            && static_cast<unsigned char>(line[1]) == 0xBB
            && static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }

        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }

        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) {
            error = "line " + std::to_string(line_no) + ": expected 'key = value'";
            return false;
        }

        const std::string key = trim(line.substr(0, eq));
        const std::string value = trim(line.substr(eq + 1));

        std::int64_t iv = 0;
        if (!parseI64(value, iv)) {
            error = "line " + std::to_string(line_no) + ": '" + value + "' is not an integer";
            return false;
        }

        const std::string full = section + "." + key;
        if (full == "scenario.seed") {
            out.seed = static_cast<std::uint64_t>(iv);
        } else if (full == "state.crop_rate") {
            out.crop_rate = iv;
        } else if (full == "run.steps") {
            out.steps = iv;
        } else if (full == "run.dt_us") {
            out.dt_us = iv;
        } else if (full == "expect.hash") {
            out.expect_hash = static_cast<std::uint64_t>(iv);
        } else {
            error = "line " + std::to_string(line_no) + ": unknown key '" + full + "'";
            return false;
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
    return true;
}
