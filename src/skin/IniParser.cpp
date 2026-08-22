#include "skin/IniParser.h"

namespace {

std::string Trim(const std::string& s) {
    size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else if (c != '\r') {
            current.push_back(c);
        }
    }
    lines.push_back(current);
    return lines;
}

}

argos::IniParseResult argos::ParseIni(const std::string& text) {
    IniParseResult result;
    std::vector<std::string> lines = SplitLines(text);

    IniSection* current = nullptr;
    for (size_t i = 0; i < lines.size(); ++i) {
        int lineNumber = static_cast<int>(i) + 1;
        std::string line = Trim(lines[i]);

        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
            std::string name = Trim(line.substr(1, line.size() - 2));
            if (name.empty()) {
                result.error = IniParseError{ lineNumber, "empty section name" };
                return result;
            }
            if (result.FindSection(name) != nullptr) {
                result.error = IniParseError{ lineNumber, "duplicate section [" + name + "]" };
                return result;
            }
            result.sections.push_back(IniSection{ name, {} });
            current = &result.sections.back();
            continue;
        }

        if (current == nullptr) {
            result.error = IniParseError{ lineNumber, "expected a [Section] header before this line" };
            return result;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            result.error = IniParseError{ lineNumber, "expected key=value or a [Section] header" };
            return result;
        }

        std::string key = Trim(line.substr(0, eq));
        std::string value = Trim(line.substr(eq + 1));
        if (key.empty()) {
            result.error = IniParseError{ lineNumber, "empty key" };
            return result;
        }
        current->entries.emplace_back(key, value);
    }

    return result;
}
