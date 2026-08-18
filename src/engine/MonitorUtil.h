#pragma once
#include <windows.h>
#include <vector>

namespace argos {

struct MonitorInfo {
    int index;
    HMONITOR handle;
    RECT rect; // virtual-desktop pixel rect
    UINT dpiX;
    UINT dpiY;
};

std::vector<MonitorInfo> EnumerateMonitors();

// Converts a position expressed relative to a monitor's top-left corner
// into an absolute virtual-desktop pixel point. Falls back to monitor 0
// (or the raw offset, if no monitors were found) if monitorIndex is out
// of range -- e.g. a saved skin referenced a monitor that's since been
// unplugged.
POINT MonitorRelativeToVirtualDesktop(const std::vector<MonitorInfo>& monitors,
                                       int monitorIndex, int relX, int relY);

// Index of the monitor containing pt, or -1 if none does.
int MonitorIndexAtPoint(const std::vector<MonitorInfo>& monitors, POINT pt);

}
