#include "manager/SkinRegistry.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace argos;
namespace fs = std::filesystem;

namespace {
void WriteFile(const fs::path& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    f << content;
}
}

int main() {
    fs::path root = fs::temp_directory_path() / "argos_skin_registry_test";
    std::error_code ec;
    fs::remove_all(root, ec); // clean slate if a prior run left it behind
    fs::create_directories(root / "Clock");
    fs::create_directories(root / "CPU");
    fs::create_directories(root / "Empty"); // no .ini -- must be skipped
    WriteFile(root / "Clock" / "skin.ini", "[Widget]\n");
    WriteFile(root / "CPU" / "cpu.ini", "[Widget]\n"); // non-"skin.ini" name, must still be found

    // ScanSkinsDirectory: finds Clock and CPU, skips Empty, sorted by name.
    {
        auto entries = ScanSkinsDirectory(root.wstring());
        assert(entries.size() == 2);
        assert(entries[0].name == L"CPU");
        assert(entries[0].iniPath == (root / "CPU" / "cpu.ini").wstring());
        assert(entries[1].name == L"Clock");
        assert(entries[1].iniPath == (root / "Clock" / "skin.ini").wstring());
        assert(!entries[0].enabled && !entries[0].loadFailed);
    }

    // ScanSkinsDirectory on a nonexistent root: empty, no throw.
    {
        auto entries = ScanSkinsDirectory((root / "does_not_exist").wstring());
        assert(entries.empty());
    }

    // MergeSkinsScan: preserves enabled state for a skin still present,
    // drops one that's gone, adds a new one disabled by default.
    {
        std::vector<SkinEntry> existing = ScanSkinsDirectory(root.wstring());
        existing[1].enabled = true; // Clock, per the sorted order above

        fs::remove_all(root / "CPU");
        fs::create_directories(root / "RAM");
        WriteFile(root / "RAM" / "skin.ini", "[Widget]\n");

        auto fresh = ScanSkinsDirectory(root.wstring());
        MergeSkinsScan(existing, fresh);

        assert(existing.size() == 2); // Clock, RAM -- CPU dropped
        assert(existing[0].name == L"Clock");
        assert(existing[0].enabled == true); // preserved
        assert(existing[1].name == L"RAM");
        assert(existing[1].enabled == false); // new entry starts disabled
    }

    fs::remove_all(root, ec);
    printf("SkinRegistry: all checks passed\n");
    return 0;
}
