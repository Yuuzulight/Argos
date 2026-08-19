#pragma once
#include <string>
#include <vector>
#include <optional>

namespace argos {

// One `[Section]` block: its name and its `key=value` entries, in the
// order they appeared in the file.
struct IniSection {
    std::string name;
    std::vector<std::pair<std::string, std::string>> entries;

    // Returns nullptr if key isn't present in this section.
    const std::string* Find(const std::string& key) const {
        for (const auto& kv : entries) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }
};

struct IniParseError {
    int line;
    std::string message;
};

struct IniParseResult {
    std::vector<IniSection> sections;
    std::optional<IniParseError> error;

    bool Ok() const { return !error.has_value(); }

    // Returns nullptr if no section with this name exists.
    const IniSection* FindSection(const std::string& name) const {
        for (const auto& s : sections) {
            if (s.name == name) return &s;
        }
        return nullptr;
    }
};

// Parses INI-style text: `[Section]` headers, `key=value` entries, full-line
// `;` or `#` comments, blank lines ignored (every line is trimmed of
// leading/trailing whitespace first). Stops at the first malformed line --
// a non-blank/non-comment line before any `[Section]` header, a line inside
// a section that's neither `[Section]` nor `key=value`, or a duplicate
// section name -- and reports it via IniParseResult::error rather than
// throwing: a skin's config is untrusted, hand-edited input, so a bad line
// is a data problem for the caller to surface, not a crash.
IniParseResult ParseIni(const std::string& text);

}
