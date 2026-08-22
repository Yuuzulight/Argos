#include <windows.h>
#include <vector>
#include "manager/ManagerWindow.h"

using namespace argos;

namespace {
ManagerWindow* g_manager = nullptr;
std::vector<SkinEntry> g_entries;

void ToggleFake(size_t index) {
    if (index >= g_entries.size()) return;
    g_entries[index].enabled = !g_entries[index].enabled;
    g_manager->SetEntries(g_entries);
}
}

// Placeholder entry point for this task's visual smoke check only --
// Task 4 replaces this file's body with the real SkinRegistry +
// SkinWidgetWindow wiring (real scanning, real enable/disable, real
// refresh). This version's toggle just flips a local flag so the row
// re-renders, proving OnPaint/OnLButtonUp/SetEntries work end to end.
int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    static ManagerWindow manager;
    g_manager = &manager;
    manager.Create(L"Argos Manager", 400, 100, 320, 400);

    g_entries.resize(4);
    g_entries[0].name = L"Clock";
    g_entries[1].name = L"CPU";
    g_entries[2].name = L"RAM";
    g_entries[3].name = L"Disk";
    g_entries[3].loadFailed = true;
    g_entries[3].loadError = "placeholder failure for visual check";
    manager.SetEntries(g_entries);

    manager.onToggleRequested = [](size_t index) { ToggleFake(index); };

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
