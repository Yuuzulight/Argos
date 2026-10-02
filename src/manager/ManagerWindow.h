#pragma once
#include <functional>
#include <vector>
#include "engine/D2DWindow.h"
#include "manager/SkinRegistry.h"

namespace argos {

// The manager application's own window: lists every skin SetEntries() was
// given, one row each (a colored enabled/disabled/failed indicator plus
// its name), and a Refresh button. Pure presentation + input -- it knows
// nothing about loading skins or spawning widget windows; main.cpp wires
// onToggleRequested/onRefreshRequested to SkinRegistry + SkinWidgetWindow
// to make clicking actually do something (design spec section 8:
// list/enable/disable/refresh, no property editor).
class ManagerWindow : public D2DWindow {
public:
    void SetEntries(std::vector<SkinEntry> entries);

    // index into the vector last passed to SetEntries().
    std::function<void(size_t index)> onToggleRequested;
    std::function<void()> onRefreshRequested;

    void OnPaint(D2DContext& ctx, float dipWidth, float dipHeight) override;
    void OnLButtonUp(float dipX, float dipY) override;
    void OnDestroy() override;

private:
    std::vector<SkinEntry> m_entries;
};

}
