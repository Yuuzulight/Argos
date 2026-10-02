#include "manager/SkinRegistry.h"
#include <algorithm>
#include <filesystem>

using namespace argos;
namespace fs = std::filesystem;

namespace {
FILETIME GetLastWriteTime(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        return data.ftLastWriteTime;
    }
    return FILETIME{};
}
}

std::vector<SkinEntry> argos::ScanSkinsDirectory(const std::wstring& root) {
    std::vector<SkinEntry> entries;
    try {
        if (!fs::is_directory(root)) {
            return entries;
        }
        for (const auto& dirEntry : fs::directory_iterator(root)) {
            if (!dirEntry.is_directory()) continue;
            std::wstring iniPath;
            for (const auto& fileEntry : fs::directory_iterator(dirEntry.path())) {
                if (fileEntry.is_regular_file() && fileEntry.path().extension() == L".ini") {
                    iniPath = fileEntry.path().wstring();
                    break;
                }
            }
            if (iniPath.empty()) continue;
            SkinEntry entry;
            entry.name = dirEntry.path().filename().wstring();
            entry.iniPath = iniPath;
            entry.lastWriteTime = GetLastWriteTime(iniPath);
            entries.push_back(std::move(entry));
        }
    } catch (const std::exception&) {
        // A directory disappearing/permission error mid-scan, etc. --
        // treated the same as "nothing found yet", per design spec
        // section 6's never-crash-on-a-skin-problem policy.
    }
    std::sort(entries.begin(), entries.end(),
              [](const SkinEntry& a, const SkinEntry& b) { return a.name < b.name; });
    return entries;
}

void argos::MergeSkinsScan(std::vector<SkinEntry>& existing, const std::vector<SkinEntry>& freshScan) {
    std::vector<SkinEntry> merged;
    merged.reserve(freshScan.size());
    for (const auto& fresh : freshScan) {
        auto it = std::find_if(existing.begin(), existing.end(), [&](const SkinEntry& e) {
            return e.name == fresh.name && e.iniPath == fresh.iniPath;
        });
        if (it != existing.end()) {
            SkinEntry kept = *it;
            kept.lastWriteTime = fresh.lastWriteTime;
            merged.push_back(std::move(kept));
        } else {
            merged.push_back(fresh);
        }
    }
    existing = std::move(merged);
}
