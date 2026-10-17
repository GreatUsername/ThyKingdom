#include "core/ini.h"

#include <fstream>

namespace core {
namespace {

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const std::size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos) {
        return std::string();
    }
    const std::size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

}  // namespace

bool parseIni(const std::string& path, std::vector<IniEntry>& out, std::string& error) {
    std::ifstream in(path.c_str());
    if (!in) {
        error = "cannot open " + path;
        return false;
    }

    out.clear();
    std::string section;
    std::string line;
    int line_no = 0;

    while (std::getline(in, line)) {
        ++line_no;

        // A BOM is not content.
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

        IniEntry entry;
        entry.section = section;
        entry.key = trim(line.substr(0, eq));
        entry.value = trim(line.substr(eq + 1));
        entry.line = line_no;
        out.push_back(entry);
    }
    return true;
}

}  // namespace core
