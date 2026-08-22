#pragma once
#include <windows.h>
#include "engine/D2DContext.h"
#include "engine/MonitorUtil.h"

namespace argos {

// A borderless, always-on-top, layered (per-pixel alpha) Win32 window that
// draws itself with Direct2D. Owns drag-to-reposition and click-through
// behavior so every consumer (widgets, and later the manager UI) gets them
// for free. Note: a click-through window (WS_EX_TRANSPARENT) does not
// receive mouse messages at all, so it is not draggable by clicking it
// directly while click-through is on -- that matches the point of the
// feature (clicks pass through to the desktop underneath).
class LayeredWindow {
public:
    LayeredWindow();
    virtual ~LayeredWindow();

    LayeredWindow(const LayeredWindow&) = delete;
    LayeredWindow& operator=(const LayeredWindow&) = delete;

    bool Create(const wchar_t* title, int screenX, int screenY, int widthPx, int heightPx);
    void Destroy();

    HWND Handle() const { return m_hwnd; }
    D2DContext& Context() { return m_context; }

    // Calls OnPaint, then presents via UpdateLayeredWindow.
    void Render();

    void SetClickThrough(bool enabled);
    bool IsClickThrough() const { return m_clickThrough; }

    void SetPosition(int screenX, int screenY);
    void PlaceOnMonitor(const std::vector<MonitorInfo>& monitors, int monitorIndex, int relX, int relY);
    // Starts (or restarts, if already running) a periodic WM_TIMER that
    // calls OnTimer() every intervalMs. Pass 0 to stop it.
    void SetUpdateTimer(UINT intervalMs);
    int ScreenX() const { return m_screenX; }
    int ScreenY() const { return m_screenY; }
    UINT Dpi() const { return m_dpi; }

    // dipWidth/dipHeight are device-independent pixels (96ths of a logical
    // inch), not physical pixels -- Direct2D's own coordinate system once
    // the render target's DPI is set correctly, so drawing math in
    // overrides stays proportionally correct across a DPI change without
    // needing to know the window's actual pixel size.
    virtual void OnPaint(D2DContext& ctx, float dipWidth, float dipHeight) {}
    virtual void OnDpiChanged(UINT newDpi) {}
    virtual void OnTimer() {}
    virtual void OnKeyDown(WPARAM vk) {}
    // Called from WM_DESTROY. The library itself never posts a quit
    // message here (one widget closing shouldn't quit an app hosting
    // several), so a host that wants the message loop to exit when its
    // last/only window closes should override this and call
    // PostQuitMessage() itself.
    virtual void OnDestroy() {}

    // Public because RegisterClassOnce() (a free function outside the class,
    // in LayeredWindow.cpp) must pass it as the WNDCLASSEXW::lpfnWndProc
    // callback. The per-instance WndProc below stays protected/internal.
    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd = nullptr;
    D2DContext m_context;
    int m_screenX = 0;
    int m_screenY = 0;
    int m_widthPx = 0;
    int m_heightPx = 0;
    UINT m_dpi = 96;
    bool m_clickThrough = false;

    bool m_dragging = false;
    POINT m_dragStartCursor{};
    POINT m_dragStartWindow{};
};

}
