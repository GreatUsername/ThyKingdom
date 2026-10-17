// A tiny INI reader: sections in brackets, "key = value" lines, '#' comments.
//
// Shared by the scenario loader and the settings loader. Values stay strings here;
// each caller decides which keys it knows and what type they must be, which is what
// lets an unknown key be a hard error instead of a silent no-op.
#pragma once

#include <string>
#include <vector>

namespace core {

struct IniEntry {
    std::string section;
    std::string key;
    std::string value;
    int line = 0;
};

// Returns false and fills 'error' on a malformed line or an unreadable file. A
// UTF-8 BOM on the first line is tolerated: these files get edited in Notepad.
bool parseIni(const std::string& path, std::vector<IniEntry>& out, std::string& error);

}  // namespace core
