#include "engine/MonitorUtil.h"
#include <shellscalingapi.h>

using namespace argos;

namespace {
BOOL CALLBACK MonitorEnumProc(HMONITOR hMon, HDC, LPRECT, LPARAM lParam) {
    auto* monitors = reinterpret_cast<std::vector<MonitorInfo>*>(lParam);
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(hMon, &info)) {
        return TRUE;
    }
    UINT dpiX = 96, dpiY = 96;
    GetDpiForMonitor(hMon, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    MonitorInfo m;
    m.index = static_cast<int>(monitors->size());
    m.handle = hMon;
    m.rect = info.rcMonitor;
    m.dpiX = dpiX;
    m.dpiY = dpiY;
    monitors->push_back(m);
    return TRUE;
}
}

std::vector<MonitorInfo> argos::EnumerateMonitors() {
    std::vector<MonitorInfo> monitors;
    EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitors));
    return monitors;
}

POINT argos::MonitorRelativeToVirtualDesktop(const std::vector<MonitorInfo>& monitors,
                                              int monitorIndex, int relX, int relY) {
    if (monitors.empty()) {
        return POINT{ relX, relY };
    }
    if (monitorIndex < 0 || monitorIndex >= static_cast<int>(monitors.size())) {
        monitorIndex = 0;
    }
    const MonitorInfo& m = monitors[monitorIndex];
    return POINT{ m.rect.left + relX, m.rect.top + relY };
}

int argos::MonitorIndexAtPoint(const std::vector<MonitorInfo>& monitors, POINT pt) {
    for (const auto& m : monitors) {
        if (PtInRect(&m.rect, pt)) {
            return m.index;
        }
    }
    return -1;
}
