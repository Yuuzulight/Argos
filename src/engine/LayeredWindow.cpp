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
    m_context.Resize(widthPx, heightPx);
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
    m_context.BeginDraw();
    OnPaint(m_context, m_widthPx, m_heightPx);
    m_context.EndDrawAndPresent(m_hwnd, m_screenX, m_screenY);
}

void LayeredWindow::SetClickThrough(bool enabled) {
    m_clickThrough = enabled;
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
        if (m_dragging) {
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
    case WM_KEYDOWN:
        OnKeyDown(wParam);
        return 0;
    case WM_DESTROY:
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
