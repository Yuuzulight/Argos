#pragma once
#include <string>
#include <vector>
#include <windows.h>

namespace argos {

// One discovered skin folder under a skins root directory:
// <root>/<Name>/<some-file>.ini. Per docs/SKIN_FORMAT.md, the config file
// has no mandated filename -- the first *.ini file found directly inside
// the folder (alphabetically) is used, matching LoadSkin's "accepts any
// path" contract.
struct SkinEntry {
    std::wstring name;        // the folder name, e.g. L"Clock"
    std::wstring iniPath;     // full path to the folder's *.ini file
    FILETIME lastWriteTime{}; // of iniPath, for change detection on refresh
    bool enabled = false;
    bool loadFailed = false;
    std::string loadError;    // set iff loadFailed
};

// Scans `root` for immediate subdirectories that contain at least one
// `*.ini` file, and returns one SkinEntry per such subdirectory, sorted by
// name. Subdirectories with no `.ini` file are skipped. Returns an empty
// vector (never throws) if `root` doesn't exist or is empty -- "no skins
// installed yet" is a normal, non-error state (design spec section 6:
// never a crash).
std::vector<SkinEntry> ScanSkinsDirectory(const std::wstring& root);

// Merges a fresh ScanSkinsDirectory() result into `existing` in place:
// entries whose name+iniPath match one in `existing` keep that entry's
// `enabled`/`loadFailed`/`loadError` state (a refresh must not silently
// disable a skin that's still there) but take `freshScan`'s
// lastWriteTime; entries no longer present in `freshScan` are removed;
// entirely new folders are appended, disabled by default. Callers that
// need to detect "this enabled skin's file changed on disk" must snapshot
// each entry's old lastWriteTime before calling this (it gets overwritten
// here) and compare against the corresponding entry's new value after.
void MergeSkinsScan(std::vector<SkinEntry>& existing, const std::vector<SkinEntry>& freshScan);

}
