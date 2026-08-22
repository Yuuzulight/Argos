#include "engine/D2DWindow.h"

using namespace argos;

namespace {
const wchar_t* kWindowClassName = L"ArgosD2DWindowClass";

void RegisterClassOnce() {
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = D2DWindow::WndProcStatic;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    RegisterClassExW(&wc);
    registered = true;
}
}

D2DWindow::D2DWindow() = default;

D2DWindow::~D2DWindow() {
    if (m_hwnd) DestroyWindow(m_hwnd);
}

bool D2DWindow::Create(const wchar_t* title, int screenX, int screenY, int widthPx, int heightPx) {
    RegisterClassOnce();
    m_widthPx = widthPx;
    m_heightPx = heightPx;

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT rect{ 0, 0, widthPx, heightPx };
    AdjustWindowRect(&rect, style, FALSE);

    m_hwnd = CreateWindowExW(0, kWindowClassName, title, style,
                              screenX, screenY,
                              rect.right - rect.left, rect.bottom - rect.top,
                              nullptr, nullptr, GetModuleHandleW(nullptr), this);
    if (!m_hwnd) {
        return false;
    }

    m_dpi = GetDpiForWindow(m_hwnd);
    if (!m_context.CreateForHwnd(m_hwnd, m_widthPx, m_heightPx, m_dpi)) {
        return false;
    }

    ShowWindow(m_hwnd, SW_SHOWNORMAL);
    return true;
}

void D2DWindow::Render() {
    if (!m_hwnd || !m_context.IsReady()) return;
    float dipWidth = m_widthPx * 96.0f / static_cast<float>(m_dpi);
    float dipHeight = m_heightPx * 96.0f / static_cast<float>(m_dpi);
    m_context.BeginDraw(D2D1::ColorF(D2D1::ColorF::WhiteSmoke));
    OnPaint(m_context, dipWidth, dipHeight);
    m_context.EndDraw();
}

LRESULT CALLBACK D2DWindow::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    D2DWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<D2DWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<D2DWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) {
        return self->WndProc(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT D2DWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        Render();
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_SIZE: {
        m_widthPx = LOWORD(lParam);
        m_heightPx = HIWORD(lParam);
        if (m_widthPx > 0 && m_heightPx > 0) {
            m_context.CreateForHwnd(hwnd, m_widthPx, m_heightPx, m_dpi);
            Render();
        }
        return 0;
    }
    case WM_DPICHANGED: {
        m_dpi = HIWORD(wParam);
        auto* suggested = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_LBUTTONUP: {
        int xPx = static_cast<short>(LOWORD(lParam));
        int yPx = static_cast<short>(HIWORD(lParam));
        float dipX = xPx * 96.0f / static_cast<float>(m_dpi);
        float dipY = yPx * 96.0f / static_cast<float>(m_dpi);
        OnLButtonUp(dipX, dipY);
        return 0;
    }
    case WM_DESTROY:
        OnDestroy();
        return 0;
    case WM_NCDESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        m_hwnd = nullptr;
        break;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
