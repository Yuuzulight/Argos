#include "engine/LayeredWindow.h"

using namespace argos;

namespace {
const wchar_t* kWindowClassName = L"ArgosLayeredWindowClass";

void RegisterClassOnce() {
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = LayeredWindow::WndProcStatic;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);
    registered = true;
}
}

LayeredWindow::LayeredWindow() = default;

LayeredWindow::~LayeredWindow() {
    Destroy();
}

bool LayeredWindow::Create(const wchar_t* title, int screenX, int screenY, int widthPx, int heightPx) {
    RegisterClassOnce();

    m_screenX = screenX;
    m_screenY = screenY;
    m_widthPx = widthPx;
    m_heightPx = heightPx;

    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST;
    DWORD style = WS_POPUP;

    m_hwnd = CreateWindowExW(exStyle, kWindowClassName, title, style,
                              screenX, screenY, widthPx, heightPx,
                              nullptr, nullptr, GetModuleHandleW(nullptr), this);
    if (!m_hwnd) {
        return false;
    }

    m_dpi = GetDpiForWindow(m_hwnd);
    if (!m_context.Resize(widthPx, heightPx, m_dpi)) {
        return false;
    }
    // If SetClickThrough() was called before Create() (before m_hwnd
    // existed), it recorded the desired state in m_clickThrough but had
    // no HWND to apply it to. Re-apply now that one exists.
    if (m_clickThrough) {
        SetClickThrough(true);
    }
    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    return true;
}

void LayeredWindow::Destroy() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

void LayeredWindow::Render() {
    if (!m_hwnd) return;
    // If the last Resize() failed, m_context may hold a render target
    // bound to torn-down resources -- never reach OnPaint()/the drawing
    // primitives in that state.
    if (!m_context.IsReady()) return;
    // OnPaint() works in DIPs (device-independent pixels), matching
    // Direct2D's own coordinate system once the render target's DPI is
    // set -- convert from the window's physical pixel size using the
    // same formula Windows itself uses for DPI scaling.
    float dipWidth = m_widthPx * 96.0f / static_cast<float>(m_dpi);
    float dipHeight = m_heightPx * 96.0f / static_cast<float>(m_dpi);
    m_context.BeginDraw();
    OnPaint(m_context, dipWidth, dipHeight);
    m_context.EndDrawAndPresent(m_hwnd, m_screenX, m_screenY);
}

void LayeredWindow::SetClickThrough(bool enabled) {
    m_clickThrough = enabled;
    if (!m_hwnd) {
        // No window yet (called before Create()) -- m_clickThrough is
        // recorded and Create() re-applies it once the HWND exists.
        return;
    }
    LONG_PTR ex = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
    if (enabled) {
        ex |= WS_EX_TRANSPARENT;
    } else {
        ex &= ~WS_EX_TRANSPARENT;
    }
    SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, ex);
}

void LayeredWindow::SetPosition(int screenX, int screenY) {
    m_screenX = screenX;
    m_screenY = screenY;
    SetWindowPos(m_hwnd, nullptr, screenX, screenY, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    Render();
}

void LayeredWindow::PlaceOnMonitor(const std::vector<MonitorInfo>& monitors, int monitorIndex,
                                    int relX, int relY) {
    POINT p = MonitorRelativeToVirtualDesktop(monitors, monitorIndex, relX, relY);
    SetPosition(p.x, p.y);
}

LRESULT CALLBACK LayeredWindow::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LayeredWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<LayeredWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<LayeredWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) {
        return self->WndProc(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT LayeredWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_LBUTTONDOWN:
        m_dragging = true;
        GetCursorPos(&m_dragStartCursor);
        m_dragStartWindow = POINT{ m_screenX, m_screenY };
        SetCapture(hwnd);
        return 0;
    case WM_MOUSEMOVE:
        if (m_dragging && (wParam & MK_LBUTTON)) {
            POINT cursor;
            GetCursorPos(&cursor);
            SetPosition(m_dragStartWindow.x + (cursor.x - m_dragStartCursor.x),
                        m_dragStartWindow.y + (cursor.y - m_dragStartCursor.y));
        }
        return 0;
    case WM_LBUTTONUP:
        if (m_dragging) {
            m_dragging = false;
            ReleaseCapture();
        }
        return 0;
    // Fires whenever this window loses mouse capture for any reason --
    // including its own ReleaseCapture() call above, but also capture
    // being stolen by another window, a system dialog/UAC prompt, Alt+Tab,
    // etc. Without this, a capture loss that isn't this window's own
    // WM_LBUTTONUP would leave m_dragging stuck true, and the window would
    // start following the cursor on the next plain hover (no button held).
    case WM_CAPTURECHANGED:
        m_dragging = false;
        return 0;
    case WM_DPICHANGED: {
        m_dpi = HIWORD(wParam);
        auto* suggested = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        m_screenX = suggested->left;
        m_screenY = suggested->top;
        m_widthPx = suggested->right - suggested->left;
        m_heightPx = suggested->bottom - suggested->top;
        bool resized = m_context.Resize(m_widthPx, m_heightPx, m_dpi);
        OnDpiChanged(m_dpi);
        // Render()'s own IsReady() guard would already stop a failed
        // resize from reaching OnPaint(), but skip the pointless call
        // outright when we already know it failed.
        if (resized) {
            Render();
        }
        return 0;
    }
    case WM_KEYDOWN:
        OnKeyDown(wParam);
        return 0;
    case WM_DESTROY:
        OnDestroy();
        return 0;
    // Fires after the window has actually been torn down (the last
    // message an HWND ever receives). Clear our cached HWND and the
    // GWLP_USERDATA pointer back to this object so nothing downstream
    // mistakes this LayeredWindow for still owning a live window.
    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        m_hwnd = nullptr;
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
