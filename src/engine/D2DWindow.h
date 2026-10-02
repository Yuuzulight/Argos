#pragma once
#include <windows.h>
#include "engine/D2DContext.h"

namespace argos {

// A normal, titled, non-layered Win32 window (a standard title bar/close
// button, WS_OVERLAPPED) that draws its client area with Direct2D via
// D2DContext's HWND render-target mode -- for app-level UI like the
// manager (design spec section 8), as opposed to LayeredWindow's
// borderless, always-on-top, per-pixel-alpha widgets (design spec section
// 5). Fixed-size only for v1 (no WS_THICKFRAME/WS_MAXIMIZEBOX) -- a short,
// non-resizable skin list needs no resizable-layout logic.
class D2DWindow {
public:
    D2DWindow();
    virtual ~D2DWindow();

    D2DWindow(const D2DWindow&) = delete;
    D2DWindow& operator=(const D2DWindow&) = delete;

    bool Create(const wchar_t* title, int screenX, int screenY, int widthPx, int heightPx);
    HWND Handle() const { return m_hwnd; }

    void Render();

    virtual void OnPaint(D2DContext& ctx, float dipWidth, float dipHeight) {}
    // dipX/dipY are client-area coordinates in DIPs, matching OnPaint's
    // coordinate system, so hit-testing against the same rects OnPaint
    // drew needs no separate pixel/DIP conversion.
    virtual void OnLButtonUp(float dipX, float dipY) {}
    virtual void OnDestroy() {}

    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd = nullptr;
    D2DContext m_context;
    int m_widthPx = 0;
    int m_heightPx = 0;
    UINT m_dpi = 96;
};

}
