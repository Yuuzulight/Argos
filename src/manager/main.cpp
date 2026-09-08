#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <map>
#include <memory>
#include <vector>
#include "engine/MonitorUtil.h"
#include "manager/ManagerWindow.h"
#include "manager/SkinRegistry.h"
#include "manager/SkinWidgetWindow.h"

using namespace argos;

namespace {

std::wstring g_skinsDir;
std::vector<SkinEntry> g_entries;
std::vector<MonitorInfo> g_monitors;
std::map<std::wstring, std::unique_ptr<SkinWidgetWindow>> g_activeWidgets; // keyed by iniPath
ManagerWindow* g_manager = nullptr;

std::wstring DefaultSkinsDir() {
    PWSTR path = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
        result = std::wstring(path) + L"\\Argos\\Skins";
    }
    if (path) CoTaskMemFree(path);
    return result;
}

// Loads entry.iniPath and, on success, creates and shows its widget
// window (added to g_activeWidgets); on failure, marks the entry failed
// and leaves it out of g_activeWidgets. Always leaves entry.enabled
// consistent with whether a widget now exists for it.
void SpawnWidget(SkinEntry& entry) {
    auto widget = std::make_unique<SkinWidgetWindow>();
    // Create() needs an initial size before a skin is loaded; every v1
    // bundled skin (and this canvas convention generally) targets the
    // same 260x140 DIP canvas argos_skin_demo.exe uses (design spec
    // section 7 / docs/2026-08-22-argos-plan-03-bundled-skins.md).
    if (!widget->Create(entry.name.c_str(), 0, 0, 260, 140)) {
        entry.loadFailed = true;
        entry.loadError = "failed to create widget window";
        entry.enabled = false;
        return;
    }

    SkinLoadResult loaded = LoadSkin(entry.iniPath, widget->Context());
    if (!loaded.skin) {
        entry.loadFailed = true;
        entry.loadError = loaded.error;
        entry.enabled = false;
        return; // widget destructs here (goes out of scope), nothing shown
    }

    entry.loadFailed = false;
    entry.loadError.clear();
    loaded.skin->UpdateMeasures();
    int intervalMs = loaded.skin->widget.updateIntervalMs;
    int monitorIndex = loaded.skin->widget.monitorIndex;
    int x = loaded.skin->widget.x;
    int y = loaded.skin->widget.y;
    widget->SetSkin(std::move(loaded.skin));
    widget->PlaceOnMonitor(g_monitors, monitorIndex, x, y);
    widget->Render();
    widget->SetUpdateTimer(static_cast<UINT>(intervalMs));
    g_activeWidgets[entry.iniPath] = std::move(widget);
}

// Reconciles g_activeWidgets against g_entries' current enabled flags:
// drops widgets for entries that are gone or disabled, spawns widgets for
// entries that are enabled but don't have one yet. The single place that
// makes "what's on screen" match "what's marked enabled" -- called after
// every mutation (a toggle, or a refresh).
void SyncWidgetsToEntries() {
    for (auto it = g_activeWidgets.begin(); it != g_activeWidgets.end();) {
        auto entryIt = std::find_if(g_entries.begin(), g_entries.end(),
            [&](const SkinEntry& e) { return e.iniPath == it->first; });
        if (entryIt == g_entries.end() || !entryIt->enabled) {
            it = g_activeWidgets.erase(it);
        } else {
            ++it;
        }
    }
    for (auto& entry : g_entries) {
        if (entry.enabled && g_activeWidgets.find(entry.iniPath) == g_activeWidgets.end()) {
            SpawnWidget(entry);
        }
    }
}

void RefreshSkins() {
    auto fresh = ScanSkinsDirectory(g_skinsDir);

    // Snapshot each known entry's old lastWriteTime before MergeSkinsScan
    // overwrites it in place, so a changed file can be detected below.
    std::map<std::wstring, FILETIME> oldTimes;
    for (const auto& e : g_entries) oldTimes[e.iniPath] = e.lastWriteTime;

    MergeSkinsScan(g_entries, fresh);

    for (auto& entry : g_entries) {
        auto it = oldTimes.find(entry.iniPath);
        bool changed = it != oldTimes.end() &&
                       CompareFileTime(&it->second, &entry.lastWriteTime) != 0;
        if (changed && entry.enabled) {
            // Evict so SyncWidgetsToEntries() below treats it as
            // missing-and-enabled, which respawns (reloads) it.
            g_activeWidgets.erase(entry.iniPath);
        }
    }

    SyncWidgetsToEntries();
    g_manager->SetEntries(g_entries);
}

void ToggleSkin(size_t index) {
    if (index >= g_entries.size()) return;
    g_entries[index].enabled = !g_entries[index].enabled;
    SyncWidgetsToEntries();
    g_manager->SetEntries(g_entries);
}

}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR cmdLine, int) {
    g_skinsDir = (cmdLine && cmdLine[0] != L'\0') ? cmdLine : DefaultSkinsDir();
    g_monitors = EnumerateMonitors();

    static ManagerWindow manager;
    g_manager = &manager;
    manager.Create(L"Argos Manager", 400, 100, 320, 400);
    manager.onToggleRequested = [](size_t index) { ToggleSkin(index); };
    manager.onRefreshRequested = []() { RefreshSkins(); };

    RefreshSkins(); // initial scan

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
