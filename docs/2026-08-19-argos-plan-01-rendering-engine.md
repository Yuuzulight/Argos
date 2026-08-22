# Argos — Component 1: Rendering/Window Engine Implementation Plan

**Goal:** Build and prove the layered, transparent, always-on-top, draggable, click-through-capable, multi-monitor-and-DPI-aware Win32 window engine that every later Argos component (widgets and the manager UI) will be built on.

**Architecture:** A small reusable static library (`argos_engine`) with three pieces — `D2DContext` (owns the Direct2D/DirectWrite factories and a per-window DC render target + backing DIB, and composites frames onto the desktop via `UpdateLayeredWindow`), `LayeredWindow` (owns the HWND, drag-to-reposition, click-through toggle, and DPI-change handling, drawing through a `D2DContext`), and `MonitorUtil` (enumerates monitors and converts monitor-relative positions to virtual-desktop coordinates). A throwaway-but-kept `argos_engine_demo` executable exercises all of it so each task is proven by actually running.

**Tech Stack:** C++17, Win32, Direct2D + DirectWrite (via `ID2D1DCRenderTarget` + GDI's `UpdateLayeredWindow`, the documented interop path for per-pixel-alpha layered windows), CMake + Ninja, MSVC (VS2022 Build Tools). No third-party libraries.

**Spec:** [docs/2026-08-19-argos-design.md](../2026-08-19-argos-design.md) — this plan implements design spec §5 (Rendering/window engine) plus the CMake/repo-layout parts of §4 and the CI part of §11.

## Global Constraints

- C++17 standard, no third-party C++ dependencies — Windows SDK only (`d2d1`, `dwrite`, `gdi32`, `user32`, `shcore`).
- Build via CMake + Ninja, compiled with MSVC. All build/run commands in this plan assume a Developer environment; use:
  `cmd /c "\"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat\" x64 && <command>"`
- No process injection, DLL injection, or hooking of another process — the engine only ever creates and manages Argos's own windows.
- **Testing adaptation for this component (per approved design spec §3):** there is no unit-test framework in this project. Each task's "test" step is: build it, run it, and verify concrete, checkable behavior (a screenshot, a `GetWindowRect` delta, a DPI-awareness-context query) — real verification, just not `assert`-based unit tests. Do not skip the verify step; do not mark a task done on "it compiles" alone.
- One GitHub issue per component (this plan = one issue), one feature branch off `main`, one PR at the end of the plan. Merge policy: self-review, confirm CI is green, merge into `main` (per design spec §11).

## Task 0: File the issue and branch

- [ ] **Step 1: Create the GitHub issue**

```bash
cd "C:/Users/Yuuzu/Desktop/Github Projects/Argos"
gh issue create --title "Component 1: Rendering/window engine" --body "Layered, transparent, always-on-top, draggable, click-through-capable, multi-monitor + per-monitor-DPI-aware Win32 window engine, drawn with Direct2D/DirectWrite. Implements design spec section 5 (docs/2026-08-19-argos-design.md). Proven via an argos_engine_demo executable at each step."
```

Note the issue number returned.

- [ ] **Step 2: Create the feature branch**

```bash
git checkout -b component-1-rendering-engine
```

## Task 1: Build scaffold + plain window smoke test

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/demo/main.cpp`
- Create: `.gitignore`

**Interfaces:**
- Produces: CMake targets `argos_engine` (static lib, empty for now) and `argos_engine_demo` (executable), both consuming C++17 and linking `d2d1;dwrite;gdi32;user32;shcore`.

- [ ] **Step 1: Write the CMake project**

```cmake
cmake_minimum_required(VERSION 3.20)
project(Argos LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "Argos only builds for Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(argos_engine STATIC
    src/engine/placeholder.cpp
)
target_include_directories(argos_engine PUBLIC src)
target_compile_definitions(argos_engine PUBLIC UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_link_libraries(argos_engine PUBLIC d2d1 dwrite gdi32 user32 shcore)

add_executable(argos_engine_demo WIN32
    src/demo/main.cpp
)
target_link_libraries(argos_engine_demo PRIVATE argos_engine)
```

`argos_engine` needs at least one translation unit to exist as a static
library target; `src/engine/placeholder.cpp` is a one-line stub removed in
Task 2 once `D2DContext.cpp` gives the library real content.

- [ ] **Step 2: Add the placeholder and demo main**

`src/engine/placeholder.cpp`:
```cpp
// Removed once D2DContext.cpp gives argos_engine real content (Task 2).
```

`src/demo/main.cpp`:
```cpp
#include <windows.h>

namespace {
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ArgosScaffoldSmokeTest";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Argos Scaffold Smoke Test",
                                 WS_OVERLAPPEDWINDOW, 100, 100, 300, 150,
                                 nullptr, nullptr, hInstance, nullptr);
    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
```

`.gitignore`:
```
/build/
*.obj
*.pdb
*.ilk
```

- [ ] **Step 3: Build**

```bash
cmd /c "\"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat\" x64 && cd /d \"C:\Users\Yuuzu\Desktop\Github Projects\Argos\" && cmake -S . -B build -G Ninja && cmake --build build"
```
Expected: builds cleanly, produces `build/argos_engine_demo.exe`.

- [ ] **Step 4: Run and verify**

Launch `build/argos_engine_demo.exe` in the background, confirm the process
is running and a window titled "Argos Scaffold Smoke Test" exists (e.g. via
`tasklist` and a `FindWindow`-based PowerShell check), then kill the
process. This only proves the toolchain end-to-end — no Direct2D yet.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/demo/main.cpp src/engine/placeholder.cpp .gitignore
git commit -m "Add CMake scaffold and plain-window smoke test"
```

## Task 2: D2DContext — layered-window Direct2D compositing

**Files:**
- Create: `src/engine/D2DContext.h`
- Create: `src/engine/D2DContext.cpp`
- Delete: `src/engine/placeholder.cpp`
- Modify: `CMakeLists.txt` (swap placeholder.cpp for D2DContext.cpp in `argos_engine`'s sources)
- Modify: `src/demo/main.cpp` (draw a rounded rect + text through a real layered window)

**Interfaces:**
- Produces: `class argos::D2DContext` with `Resize(int,int)`, `BeginDraw()`,
  `EndDrawAndPresent(HWND,int,int)`, `FillRoundedRect(D2D1_RECT_F,float,D2D1_COLOR_F)`,
  `FillBar(D2D1_RECT_F,float,D2D1_COLOR_F,D2D1_COLOR_F)`,
  `DrawText(D2D1_RECT_F,const wchar_t*,IDWriteTextFormat*,D2D1_COLOR_F)`,
  `CreateTextFormat(const wchar_t*,float) -> ComPtr<IDWriteTextFormat>`.

- [ ] **Step 1: Write D2DContext.h**

```cpp
#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

namespace argos {

using Microsoft::WRL::ComPtr;

// Owns the process-wide Direct2D/DirectWrite factories, and one window's
// DC render target + backing 32bpp DIB section used to composite through
// UpdateLayeredWindow (the documented interop path for per-pixel-alpha
// layered windows: draw with Direct2D onto a GDI-compatible DC, then hand
// that DC to UpdateLayeredWindow).
class D2DContext {
public:
    D2DContext();
    ~D2DContext();

    D2DContext(const D2DContext&) = delete;
    D2DContext& operator=(const D2DContext&) = delete;

    // Creates/resizes the backing DIB + DC render target for the given
    // pixel size. Safe to call again (e.g. on a DPI change).
    bool Resize(int widthPx, int heightPx);

    void BeginDraw();
    // Ends the Direct2D draw and composites the frame onto hwnd at screen
    // position (screenX, screenY) with full per-pixel alpha.
    void EndDrawAndPresent(HWND hwnd, int screenX, int screenY);

    void FillRoundedRect(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color);
    void FillBar(const D2D1_RECT_F& bounds, float fraction, const D2D1_COLOR_F& fillColor,
                 const D2D1_COLOR_F& trackColor);
    void DrawText(const D2D1_RECT_F& layoutRect, const wchar_t* text, IDWriteTextFormat* format,
                  const D2D1_COLOR_F& color);
    ComPtr<IDWriteTextFormat> CreateTextFormat(const wchar_t* fontFamily, float sizePt);

private:
    ComPtr<ID2D1Factory> m_d2dFactory;
    ComPtr<IDWriteFactory> m_dwriteFactory;
    ComPtr<ID2D1DCRenderTarget> m_dcRenderTarget;

    HDC m_memDC = nullptr;
    HBITMAP m_dib = nullptr;
    HBITMAP m_oldBitmap = nullptr;
    void* m_dibPixels = nullptr;
    int m_widthPx = 0;
    int m_heightPx = 0;

    void ReleaseDib();
};

}
```

- [ ] **Step 2: Write D2DContext.cpp**

```cpp
#include "engine/D2DContext.h"

using namespace argos;

D2DContext::D2DContext() {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2dFactory.GetAddressOf());
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                         reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
}

D2DContext::~D2DContext() {
    ReleaseDib();
}

void D2DContext::ReleaseDib() {
    if (m_memDC && m_oldBitmap) {
        SelectObject(m_memDC, m_oldBitmap);
        m_oldBitmap = nullptr;
    }
    if (m_dib) {
        DeleteObject(m_dib);
        m_dib = nullptr;
    }
    if (m_memDC) {
        DeleteDC(m_memDC);
        m_memDC = nullptr;
    }
}

bool D2DContext::Resize(int widthPx, int heightPx) {
    if (widthPx <= 0 || heightPx <= 0) {
        return false;
    }
    if (widthPx == m_widthPx && heightPx == m_heightPx && m_dcRenderTarget) {
        return true;
    }

    ReleaseDib();

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = widthPx;
    bmi.bmiHeader.biHeight = -heightPx; // negative = top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screenDC = GetDC(nullptr);
    m_memDC = CreateCompatibleDC(screenDC);
    m_dib = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &m_dibPixels, nullptr, 0);
    ReleaseDC(nullptr, screenDC);
    if (!m_memDC || !m_dib) {
        return false;
    }
    m_oldBitmap = static_cast<HBITMAP>(SelectObject(m_memDC, m_dib));

    if (!m_dcRenderTarget) {
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        if (FAILED(m_d2dFactory->CreateDCRenderTarget(&props, m_dcRenderTarget.GetAddressOf()))) {
            return false;
        }
    }

    RECT bindRect{ 0, 0, widthPx, heightPx };
    if (FAILED(m_dcRenderTarget->BindDC(m_memDC, &bindRect))) {
        return false;
    }

    m_widthPx = widthPx;
    m_heightPx = heightPx;
    return true;
}

void D2DContext::BeginDraw() {
    m_dcRenderTarget->BeginDraw();
    m_dcRenderTarget->Clear(D2D1::ColorF(0, 0.0f));
}

void D2DContext::EndDrawAndPresent(HWND hwnd, int screenX, int screenY) {
    if (FAILED(m_dcRenderTarget->EndDraw())) {
        return;
    }

    POINT ptSrc{ 0, 0 };
    POINT ptDst{ screenX, screenY };
    SIZE size{ m_widthPx, m_heightPx };
    BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    HDC screenDC = GetDC(nullptr);
    UpdateLayeredWindow(hwnd, screenDC, &ptDst, &size, m_memDC, &ptSrc, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screenDC);
}

void D2DContext::FillRoundedRect(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color) {
    ComPtr<ID2D1SolidColorBrush> brush;
    m_dcRenderTarget->CreateSolidColorBrush(color, brush.GetAddressOf());
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, radius, radius);
    m_dcRenderTarget->FillRoundedRectangle(rr, brush.Get());
}

void D2DContext::FillBar(const D2D1_RECT_F& bounds, float fraction, const D2D1_COLOR_F& fillColor,
                          const D2D1_COLOR_F& trackColor) {
    FillRoundedRect(bounds, 3.0f, trackColor);
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    D2D1_RECT_F fillRect = bounds;
    fillRect.right = bounds.left + (bounds.right - bounds.left) * fraction;
    FillRoundedRect(fillRect, 3.0f, fillColor);
}

void D2DContext::DrawText(const D2D1_RECT_F& layoutRect, const wchar_t* text, IDWriteTextFormat* format,
                           const D2D1_COLOR_F& color) {
    ComPtr<ID2D1SolidColorBrush> brush;
    m_dcRenderTarget->CreateSolidColorBrush(color, brush.GetAddressOf());
    m_dcRenderTarget->DrawText(text, static_cast<UINT32>(wcslen(text)), format, layoutRect, brush.Get());
}

ComPtr<IDWriteTextFormat> D2DContext::CreateTextFormat(const wchar_t* fontFamily, float sizePt) {
    ComPtr<IDWriteTextFormat> format;
    m_dwriteFactory->CreateTextFormat(fontFamily, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                       DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                       sizePt, L"en-us", format.GetAddressOf());
    return format;
}
```

- [ ] **Step 3: Update CMakeLists.txt**

Replace `src/engine/placeholder.cpp` with `src/engine/D2DContext.cpp` in
`argos_engine`'s source list. Delete `src/engine/placeholder.cpp`.

- [ ] **Step 4: Replace demo main.cpp**

```cpp
#include <windows.h>
#include "engine/D2DContext.h"

using namespace argos;

namespace {
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ArgosD2DSmokeTest";
    RegisterClassExW(&wc);

    const int width = 260;
    const int height = 90;
    HWND hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                                 wc.lpszClassName, L"Argos D2D Smoke Test", WS_POPUP,
                                 200, 200, width, height, nullptr, nullptr, hInstance, nullptr);

    D2DContext ctx;
    ctx.Resize(width, height);
    ShowWindow(hwnd, nCmdShow);

    ctx.BeginDraw();
    ctx.FillRoundedRect(D2D1::RectF(0, 0, (float)width, (float)height), 12.0f,
                         D2D1::ColorF(0.10f, 0.10f, 0.12f, 0.85f));
    auto textFormat = ctx.CreateTextFormat(L"Segoe UI", 20.0f);
    ctx.DrawText(D2D1::RectF(12, 12, width - 12.0f, height - 12.0f), L"Argos engine online",
                 textFormat.Get(), D2D1::ColorF(D2D1::ColorF::White));
    ctx.EndDrawAndPresent(hwnd, 200, 200);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
```

- [ ] **Step 5: Build**

Same build command as Task 1, Step 3.
Expected: builds cleanly (fix any COM/link errors — e.g. missing
`#include <shlwapi.h>` or lib order — before moving on; this is the "run to
see it fail, then fix" cycle for this component).

- [ ] **Step 6: Run and verify**

Launch `build/argos_engine_demo.exe` in the background. Take a screenshot
of the desktop region around (200,200)-(460,290) using a short PowerShell
script (`Add-Type -AssemblyName System.Drawing` + `CopyFromScreen`), save
it to the scratchpad, and view it with the Read tool. Expected: a dark
rounded rectangle with white "Argos engine online" text, with the desktop
visible (semi-transparently) around its rounded corners — proof that
per-pixel alpha compositing through `UpdateLayeredWindow` is actually
working, not just an opaque rectangle. Kill the process afterward.

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "Add D2DContext: Direct2D-through-layered-window compositing"
```

## Task 3: LayeredWindow — drag, click-through, reusable Render()

**Files:**
- Create: `src/engine/LayeredWindow.h`
- Create: `src/engine/LayeredWindow.cpp`
- Modify: `CMakeLists.txt` (add `src/engine/LayeredWindow.cpp` to `argos_engine`)
- Modify: `src/demo/main.cpp` (subclass `LayeredWindow`, draw rect + bar + text, wire a click-through hotkey)

**Interfaces:**
- Consumes: `argos::D2DContext` from Task 2 (`Resize`, `BeginDraw`, `EndDrawAndPresent`, `FillRoundedRect`, `FillBar`, `DrawText`, `CreateTextFormat`).
- Produces: `class argos::LayeredWindow` with `Create(const wchar_t*,int,int,int,int) -> bool`,
  `Destroy()`, `Render()`, `SetClickThrough(bool)`, `IsClickThrough() -> bool`,
  `SetPosition(int,int)`, `ScreenX()/ScreenY() -> int`, `Handle() -> HWND`,
  `Context() -> D2DContext&`, `Dpi() -> UINT`, and virtual hooks
  `OnPaint(D2DContext&,int,int)`, `OnDpiChanged(UINT)`, `OnKeyDown(WPARAM)`.

- [ ] **Step 1: Write LayeredWindow.h**

```cpp
#pragma once
#include <windows.h>
#include "engine/D2DContext.h"

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
    int ScreenX() const { return m_screenX; }
    int ScreenY() const { return m_screenY; }
    UINT Dpi() const { return m_dpi; }

    virtual void OnPaint(D2DContext& ctx, int widthPx, int heightPx) {}
    virtual void OnDpiChanged(UINT newDpi) {}
    virtual void OnKeyDown(WPARAM vk) {}

protected:
    static LRESULT CALLBACK WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
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
```

- [ ] **Step 2: Write LayeredWindow.cpp**

```cpp
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
```

- [ ] **Step 3: Update CMakeLists.txt**

Add `src/engine/LayeredWindow.cpp` to `argos_engine`'s source list.

- [ ] **Step 4: Replace demo main.cpp**

```cpp
#include <windows.h>
#include "engine/LayeredWindow.h"

using namespace argos;

namespace {

class DemoWidget : public LayeredWindow {
public:
    void OnPaint(D2DContext& ctx, int w, int h) override {
        ctx.FillRoundedRect(D2D1::RectF(0, 0, (float)w, (float)h), 12.0f,
                             D2D1::ColorF(0.10f, 0.10f, 0.12f, 0.85f));
        auto textFormat = ctx.CreateTextFormat(L"Segoe UI", 14.0f);
        const wchar_t* label = IsClickThrough() ? L"Argos (click-through ON)"
                                                 : L"Argos (drag me / press T)";
        ctx.DrawText(D2D1::RectF(12, 10, w - 12.0f, 34.0f), label, textFormat.Get(),
                     D2D1::ColorF(D2D1::ColorF::White));
        ctx.FillBar(D2D1::RectF(12, 44, w - 12.0f, 60.0f), 0.42f,
                    D2D1::ColorF(0.30f, 0.65f, 0.95f, 1.0f), D2D1::ColorF(1, 1, 1, 0.15f));
    }

    void OnKeyDown(WPARAM vk) override {
        if (vk == 'T') {
            SetClickThrough(!IsClickThrough());
            Render();
        }
    }
};

}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int nCmdShow) {
    static DemoWidget widget;
    widget.Create(L"Argos Engine Demo", 200, 200, 260, 90);
    widget.Render();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
```

- [ ] **Step 5: Build**

Same build command as Task 1, Step 3.

- [ ] **Step 6: Run and verify drag**

Launch `build/argos_engine_demo.exe` in the background. Write a short
PowerShell script in the scratchpad that P/Invokes `FindWindowW` (title
"Argos Engine Demo"), reads its rect with `GetWindowRect`, then uses
`SetCursorPos` + `mouse_event(MOUSEEVENTF_LEFTDOWN)` over a point inside the
window, several `SetCursorPos` calls tracing a diagonal move, then
`mouse_event(MOUSEEVENTF_LEFTUP)`, then reads `GetWindowRect` again.
Expected: the window's rect moved by (approximately) the same delta the
cursor moved — proof drag-to-reposition works.

- [ ] **Step 7: Run and verify click-through**

With the demo still running and focused (the click sequence above should
have activated it), send a synthetic `T` keypress via the same PowerShell
script's `keybd_event`/`SendInput`. Confirm via a second drag attempt (Step
6's script) that the window's rect does *not* move this time — proof
`WS_EX_TRANSPARENT` is now blocking input to the widget. Send `T` again to
toggle it back, and confirm dragging works again. Kill the process
afterward.

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "Add LayeredWindow: drag-to-reposition and click-through"
```

## Task 4: Per-monitor DPI awareness

**Files:**
- Create: `app.manifest`
- Modify: `CMakeLists.txt` (embed the manifest into `argos_engine_demo` via the linker)
- Modify: `src/engine/LayeredWindow.cpp` (handle `WM_DPICHANGED`)

**Interfaces:**
- No new public methods; `LayeredWindow::OnDpiChanged(UINT)` (already declared in Task 3) is now actually invoked.

- [ ] **Step 1: Write app.manifest**

```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0"
          xmlns:asmv3="urn:schemas-microsoft-com:asm.v3">
  <assemblyIdentity type="win32" name="Argos.EngineDemo" version="1.0.0.0" processorArchitecture="*"/>
  <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
    <application>
      <!-- Windows 10 -->
      <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
    </application>
  </compatibility>
  <asmv3:application>
    <asmv3:windowsSettings xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">
      <dpiAwareness>PerMonitorV2</dpiAwareness>
    </asmv3:windowsSettings>
  </asmv3:application>
</assembly>
```

- [ ] **Step 2: Embed it in CMakeLists.txt**

Add, on the `argos_engine_demo` target:

```cmake
if(MSVC)
  set_target_properties(argos_engine_demo PROPERTIES
      LINK_FLAGS "/MANIFEST:EMBED /MANIFESTINPUT:${CMAKE_SOURCE_DIR}/app.manifest")
endif()
```

- [ ] **Step 3: Handle WM_DPICHANGED in LayeredWindow.cpp**

Add a case to the `switch` in `LayeredWindow::WndProc` (Task 3, Step 2),
right before `case WM_KEYDOWN:`:

```cpp
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
        m_context.Resize(m_widthPx, m_heightPx);
        OnDpiChanged(m_dpi);
        Render();
        return 0;
    }
```

This rescales the Direct2D render target to the new pixel size and moves
the window into the rect Windows suggests, so content stays crisp and
correctly positioned instead of being GDI-stretched.

- [ ] **Step 4: Build**

Same build command as Task 1, Step 3.

- [ ] **Step 5: Run and verify the manifest took effect**

This machine cannot physically change monitor DPI to trigger a real
`WM_DPICHANGED`, so verify the manifest declaration itself took effect:
launch the demo, then run a short PowerShell script that P/Invokes
`FindWindowW` for "Argos Engine Demo", calls
`GetWindowDpiAwarenessContext(hwnd)`, and compares it with
`AreDpiAwarenessContextsEqual` against
`DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2` (value `-4` as a
`DPI_AWARENESS_CONTEXT`). Expected: they're equal, proving the process is
genuinely running as Per-Monitor-V2 DPI aware, not falling back to
system-DPI-aware. Kill the process afterward.

**Hardware-verification note (already flagged in the design spec, §12):**
the `WM_DPICHANGED` rescale/reposition logic above is code-reviewed and
matches Microsoft's documented pattern, but cannot be exercised end-to-end
here without a second, differently-scaled monitor. Please drag a widget
between two differently-scaled monitors once you're on real hardware and
confirm it stays crisp and lands at the right spot.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "Add per-monitor DPI awareness manifest and WM_DPICHANGED handling"
```

## Task 5: Multi-monitor coordinate model

**Files:**
- Create: `src/engine/MonitorUtil.h`
- Create: `src/engine/MonitorUtil.cpp`
- Modify: `CMakeLists.txt` (add `src/engine/MonitorUtil.cpp` to `argos_engine`)
- Modify: `src/engine/LayeredWindow.h` / `.cpp` (add `PlaceOnMonitor`)
- Modify: `src/demo/main.cpp` (place the demo widget via monitor index + relative offset)

**Interfaces:**
- Produces: `struct argos::MonitorInfo { int index; HMONITOR handle; RECT rect; UINT dpiX; UINT dpiY; }`,
  `std::vector<MonitorInfo> EnumerateMonitors()`,
  `POINT MonitorRelativeToVirtualDesktop(const std::vector<MonitorInfo>&, int monitorIndex, int relX, int relY)`,
  `int MonitorIndexAtPoint(const std::vector<MonitorInfo>&, POINT)`.
- Adds to `LayeredWindow`: `void PlaceOnMonitor(const std::vector<MonitorInfo>& monitors, int monitorIndex, int relX, int relY)`.

- [ ] **Step 1: Write MonitorUtil.h**

```cpp
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
```

- [ ] **Step 2: Write MonitorUtil.cpp**

```cpp
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
```

- [ ] **Step 3: Update CMakeLists.txt**

Add `src/engine/MonitorUtil.cpp` to `argos_engine`'s source list.

- [ ] **Step 4: Add PlaceOnMonitor to LayeredWindow**

In `LayeredWindow.h`, add to the public section (after `SetPosition`):

```cpp
    void PlaceOnMonitor(const std::vector<MonitorInfo>& monitors, int monitorIndex, int relX, int relY);
```

Add `#include "engine/MonitorUtil.h"` to `LayeredWindow.h`'s includes.

In `LayeredWindow.cpp`, add after `SetPosition`:

```cpp
void LayeredWindow::PlaceOnMonitor(const std::vector<MonitorInfo>& monitors, int monitorIndex,
                                    int relX, int relY) {
    POINT p = MonitorRelativeToVirtualDesktop(monitors, monitorIndex, relX, relY);
    SetPosition(p.x, p.y);
}
```

- [ ] **Step 5: Wire it into the demo**

In `src/demo/main.cpp`, replace the `widget.Create(...)` line and add
monitor placement:

```cpp
#include "engine/MonitorUtil.h"
// ... inside wWinMain, replacing the previous Create call:
    auto monitors = EnumerateMonitors();
    widget.Create(L"Argos Engine Demo", 0, 0, 260, 90);
    widget.PlaceOnMonitor(monitors, 0, 40, 40);
```

- [ ] **Step 6: Build**

Same build command as Task 1, Step 3.

- [ ] **Step 7: Run and verify**

Launch the demo, screenshot the region near (monitor 0's origin + 40, 40)
the same way as Task 2 Step 6, and confirm the widget renders there.
Additionally write a tiny scratchpad throwaway that calls
`EnumerateMonitors()` from a standalone console `.cpp` (compiled directly
with `cl.exe` for a one-off check, not added to the CMake project) and
prints each monitor's rect/DPI, to confirm enumeration returns sane values
on this machine's actual monitor configuration. Kill the process
afterward.

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "Add MonitorUtil: monitor enumeration and relative-position placement"
```

## Task 6: CI workflow, open the PR

**Files:**
- Create: `.github/workflows/build.yml`
- Create: `README.md` content for the engine (append a short "Building" section — see step 2)

**Interfaces:** none (project glue only).

- [ ] **Step 1: Write the CI workflow**

```yaml
name: build

on:
  push:
    branches: [main]
  pull_request:

jobs:
  windows-build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - uses: ilammy/msvc-dev-cmd@v1
      - name: CMake configure
        run: cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
      - name: Build
        run: cmake --build build --config Release
```

`ilammy/msvc-dev-cmd` is a GitHub Actions marketplace action that sets up
the Developer environment on the `windows-latest` runner — it is CI
tooling, not a dependency of the Argos codebase itself, so it doesn't
conflict with the "no third-party C++ dependencies" constraint.

- [ ] **Step 2: Start the README**

Create `README.md` at the repo root (it currently only has a placeholder
line from the initial commit) with:

```markdown
# Argos

A native C++/Win32 desktop widget/skin engine for Windows — draws
transparent, always-on-top "widgets" showing live system data (clock,
CPU/RAM/disk usage), configured via plain-text skin files. Not a taskbar
replacement; Argos never touches, injects into, or patches `explorer.exe`
or any other process — every widget is Argos's own window, drawn with
standard Win32/Direct2D APIs.

## Status

Under active development. The rendering/window engine (layered,
draggable, click-through-capable, per-monitor-DPI-aware windows drawn
with Direct2D/DirectWrite) is built; the skin format, bundled skins,
manager UI, persistence, and installer are still to come. See
`docs/2026-08-19-argos-design.md` for the full design.

## Building

Requires Visual Studio 2022 Build Tools (or Visual Studio 2022) with the
"Desktop development with C++" workload, and CMake 3.20+ (VS Build Tools
ships its own, under
`Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`).

From a "Developer Command Prompt for VS 2022":

\```
cmake -S . -B build -G Ninja
cmake --build build
\```

This produces `build\argos_engine_demo.exe`, a proving executable for the
rendering engine.
```

(Use literal triple-backtick fences, not escaped, when writing the file —
the `\```` above is only to keep this plan's own code block from closing
early.)

- [ ] **Step 3: Build one more time to make sure nothing broke**

Same build command as Task 1, Step 3.

- [ ] **Step 4: Commit**

```bash
git add .github/workflows/build.yml README.md
git commit -m "Add CI build workflow and project README"
```

- [ ] **Step 5: Push and open the PR**

```bash
git push -u origin component-1-rendering-engine
gh pr create --title "Component 1: Rendering/window engine" --body "Implements design spec section 5: layered/transparent/always-on-top/draggable widget windows, click-through mode, per-monitor DPI awareness, and the monitor-relative coordinate model. Proven at each task via an actual running argos_engine_demo (screenshots, scripted drag/click-through checks, and a DPI-awareness-context check -- see commit history for what each step verified). Closes #<issue-number-from-Task-0>." --base main
```

- [ ] **Step 6: Wait for CI, self-review, merge**

Watch the PR's checks with `gh pr checks --watch`. Once green, read through
the full diff once more (`gh pr diff`) for anything sloppy (leftover debug
code, inconsistent naming), fix and push if needed, then:

```bash
gh pr merge --merge --delete-branch
```

Report back to the user: Component 1 is merged into `main`, what got
proven at each step, and the two hardware-verification assumptions (DPI
rescale on a real second monitor, and — later — the SmartScreen flow) that
still need a human check.
