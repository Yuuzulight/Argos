# Argos — Component 4: Manager Application Implementation Plan

**Goal:** Ship a single Direct2D-rendered manager window that lists installed
skins, lets the user enable/disable each one (spawning/closing its widget
window), and refreshes the list (picking up new/removed skins and reloading
ones whose file changed on disk) — design spec section 8.

**Architecture:** Two new pieces of engine-level infrastructure plus the
manager itself. `D2DContext` (Component 1) gains a second, non-layered
render-target mode (`ID2D1HwndRenderTarget`, the standard WM_PAINT-driven
Direct2D presentation path) alongside its existing per-pixel-alpha DC/layered
mode, since the manager is a normal titled window, not a widget. A new
generic `D2DWindow` base class (parallel to `LayeredWindow`, both live in
`src/engine/`) wraps that mode for any normal app window. `ManagerWindow`
(a `D2DWindow` subclass) is pure presentation: it draws whatever
`SkinEntry` list it's given and reports clicks via callbacks, with no
knowledge of loading skins or spawning widgets. `src/manager/main.cpp` owns
the actual behavior — scanning the skins folder (`SkinRegistry`'s free
functions), and spawning/destroying `SkinWidgetWindow` instances (a
reusable version of `skin_demo.cpp`'s widget-hosting pattern) as skins are
enabled/disabled or reloaded. Reposition works for free: an enabled skin's
widget is a plain `LayeredWindow` subclass, so it already has
click-and-drag from Component 1 (design spec section 8: "reusing the window
engine's drag support ... rather than a second drag implementation inside
the manager's list").

**Tech Stack:** No new third-party dependencies. `std::filesystem` (C++17
standard library) for directory scanning — first use in this codebase, but
it's stdlib, not a new dependency. Two additional Windows import libraries
for the manager executable only: `shell32` (`SHGetKnownFolderPath`, to find
`%LOCALAPPDATA%`) and `ole32` (`CoTaskMemFree`, required to free what that
call returns).

**Spec:** [docs/2026-08-19-argos-design.md](../2026-08-19-argos-design.md)
— this plan implements design spec section 8 (Component 4 — Manager
application):
"Single Direct2D-rendered window (no Win32 Common Controls anywhere in the
UI, matching the rendering stack decision): Lists installed skins, scanned
from `%LOCALAPPDATA%\Argos\Skins\`. Enable/disable per skin (spawns/closes
its widget window). Reposition: dragging the widget itself (reusing the
window engine's drag support from §5) rather than a second drag
implementation inside the manager's list. Refresh: re-scans the skins
folder for new skins and reloads a skin's config if its files changed on
disk. Out of scope: no property editor for colors/fonts, no plugin/DLL
loading."

## Global Constraints

- No Win32 Common Controls (no listview, no buttons) — the manager draws
  its own list rows and refresh button with Direct2D/DirectWrite, per
  design spec section 8.
- No property editor for colors/fonts, no plugin/DLL loading — out of
  scope per design spec section 8.
- A skin's config file has no mandated filename (`docs/SKIN_FORMAT.md`):
  scanning must look for *any* `.ini` file inside each skin subfolder, not
  a hardcoded `skin.ini`.
- A skin that fails to load must show inline in the list (design spec
  section 6: "the manager application ... shows it next to that skin
  instead of loading it"), never crash the manager.
- Default scan root is `%LOCALAPPDATA%\Argos\Skins\` per design spec
  section 8, but `argos_manager.exe` accepts an optional command-line
  override of that path — the same pattern `argos_skin_demo.exe` already
  uses for its skin path (Component 2) — so verification never has to
  write test fixtures into the user's real profile.
- Build via CMake + Ninja, MSVC, same Developer-environment command as
  Components 1-3:
  `cmd /c "\"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat\" x64 && <command>"`
  run from the repo root. Note: this exact invocation does not work from
  the Bash tool (Git Bash/MSYS mangles `/c` into a path) — use `cmd //c`
  (double slash) or write it to a temporary `.bat` file and run that
  instead, per Component 3's implementers' findings.
- One GitHub issue for this component (this plan = one issue), one feature
  branch off `main`, one PR at the end. Merge policy: self-review, confirm
  CI is green, merge into `main` (design spec section 11).

## Task 0: File the issue and branch

- [ ] **Step 1: Create the GitHub issue**

```bash
gh issue create --title "Component 4: Manager application" --body "The manager application design spec section 8 calls for: a single Direct2D-rendered window listing installed skins, enable/disable per skin (spawn/close its widget window), reposition by dragging the widget itself, and a refresh that re-scans the skins folder and reloads changed configs. No Win32 Common Controls, no property editor, no plugin/DLL loading."
```

Note the issue number returned.

- [ ] **Step 2: Create the feature branch**

```bash
git checkout -b component-4-manager-app
```

## Task 1: D2DContext HWND-render-target mode + D2DWindow base class

**Files:**
- Modify: `src/engine/D2DContext.h`
- Modify: `src/engine/D2DContext.cpp`
- Create: `src/engine/D2DWindow.h`
- Create: `src/engine/D2DWindow.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: nothing new (extends Component 1's existing `D2DContext`).
- Produces: `D2DContext::CreateForHwnd(HWND, int, int, UINT) -> bool`,
  `D2DContext::EndDraw()`, `D2DContext::BeginDraw(const D2D1_COLOR_F&
  clearColor = D2D1::ColorF(0, 0.0f))` (existing callers that pass no
  argument keep their exact prior behavior). `class D2DWindow` with
  `Create(title, screenX, screenY, widthPx, heightPx) -> bool`, `Render()`,
  virtuals `OnPaint(D2DContext&, float dipWidth, float dipHeight)`,
  `OnLButtonUp(float dipX, float dipY)`, `OnDestroy()`.

- [ ] **Step 1: Add the HWND render-target mode to D2DContext.h**

In `src/engine/D2DContext.h`, add a new member alongside the existing
`m_dcRenderTarget`:

```cpp
    ComPtr<ID2D1DCRenderTarget> m_dcRenderTarget;
    ComPtr<ID2D1HwndRenderTarget> m_hwndRenderTarget;
```

Add these new public methods, right after the existing `Resize()`
declaration (before `IsReady()`):

```cpp
    // Creates (on first call) or resizes (on later calls) an
    // ID2D1HwndRenderTarget bound directly to hwnd -- the standard
    // non-layered Direct2D presentation path (WM_PAINT-driven
    // BeginDraw/EndDraw), for normal application windows like the manager
    // UI, as opposed to Resize()'s DC/DIB path used by layered widgets.
    // A D2DContext should be used in one mode for its whole lifetime.
    bool CreateForHwnd(HWND hwnd, int widthPx, int heightPx, UINT dpi);
```

Change the existing `BeginDraw()` declaration to take an optional clear
color (existing no-argument call sites keep behaving exactly as before):

```cpp
    void BeginDraw(const D2D1_COLOR_F& clearColor = D2D1::ColorF(0, 0.0f));
```

Add `EndDraw()` right after the existing `EndDrawAndPresent()` declaration:

```cpp
    // Ends the draw for HWND-render-target mode. Unlike
    // EndDrawAndPresent() (the DC/layered path's manual blit),
    // ID2D1HwndRenderTarget presents to its window automatically on
    // EndDraw -- no hwnd/position arguments needed.
    void EndDraw();
```

Add a private helper right after the existing private members, before
`ReleaseDib()`:

```cpp
    ID2D1RenderTarget* Target() const;
```

- [ ] **Step 2: Implement the new methods in D2DContext.cpp**

Change `FillRoundedRect`, `FillBar`'s callee, and `DrawText` to go through
the new `Target()` helper instead of `m_dcRenderTarget` directly, so both
render-target modes share these draw helpers unchanged:

```cpp
ID2D1RenderTarget* D2DContext::Target() const {
    if (m_hwndRenderTarget) return m_hwndRenderTarget.Get();
    return m_dcRenderTarget.Get();
}

void D2DContext::FillRoundedRect(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color) {
    ComPtr<ID2D1SolidColorBrush> brush;
    Target()->CreateSolidColorBrush(color, brush.GetAddressOf());
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, radius, radius);
    Target()->FillRoundedRectangle(rr, brush.Get());
}
```

(`FillBar` itself is unchanged — it only calls `FillRoundedRect`, never
`m_dcRenderTarget` directly.)

```cpp
void D2DContext::DrawText(const D2D1_RECT_F& layoutRect, const wchar_t* text, IDWriteTextFormat* format,
                           const D2D1_COLOR_F& color) {
    ComPtr<ID2D1SolidColorBrush> brush;
    Target()->CreateSolidColorBrush(color, brush.GetAddressOf());
    Target()->DrawText(text, static_cast<UINT32>(wcslen(text)), format, layoutRect, brush.Get());
}
```

Change `BeginDraw()` to accept the clear color and use `Target()`:

```cpp
void D2DContext::BeginDraw(const D2D1_COLOR_F& clearColor) {
    if (!IsReady()) {
        return;
    }
    Target()->BeginDraw();
    Target()->Clear(clearColor);
}
```

Add `CreateForHwnd` and `EndDraw`, after the existing `EndDrawAndPresent`:

```cpp
bool D2DContext::CreateForHwnd(HWND hwnd, int widthPx, int heightPx, UINT dpi) {
    m_ready = false;
    if (!m_d2dFactory || !m_dwriteFactory) {
        return false;
    }
    if (widthPx <= 0 || heightPx <= 0) {
        return false;
    }

    if (!m_hwndRenderTarget) {
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
        D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
            hwnd, D2D1::SizeU(static_cast<UINT32>(widthPx), static_cast<UINT32>(heightPx)));
        if (FAILED(m_d2dFactory->CreateHwndRenderTarget(props, hwndProps, m_hwndRenderTarget.GetAddressOf()))) {
            return false;
        }
    } else if (FAILED(m_hwndRenderTarget->Resize(
                   D2D1::SizeU(static_cast<UINT32>(widthPx), static_cast<UINT32>(heightPx))))) {
        return false;
    }

    m_hwndRenderTarget->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
    m_widthPx = widthPx;
    m_heightPx = heightPx;
    m_dpi = dpi;
    m_ready = true;
    return true;
}

void D2DContext::EndDraw() {
    if (!IsReady() || !m_hwndRenderTarget) {
        return;
    }
    // D2DERR_RECREATE_TARGET means the underlying device was lost (driver
    // reset, etc.) -- the documented recovery is to drop the render target
    // so the next CreateForHwnd() call rebuilds it from scratch.
    if (m_hwndRenderTarget->EndDraw() == D2DERR_RECREATE_TARGET) {
        m_hwndRenderTarget.Reset();
        m_ready = false;
    }
}
```

- [ ] **Step 3: Create the D2DWindow header**

Create `src/engine/D2DWindow.h`:

```cpp
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
```

- [ ] **Step 4: Create the D2DWindow implementation**

Create `src/engine/D2DWindow.cpp`:

```cpp
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
```

- [ ] **Step 5: Add D2DWindow.cpp to the argos_engine target**

In `CMakeLists.txt`, change:

```cmake
add_library(argos_engine STATIC
    src/engine/D2DContext.cpp
    src/engine/LayeredWindow.cpp
    src/engine/MonitorUtil.cpp
)
```

to:

```cmake
add_library(argos_engine STATIC
    src/engine/D2DContext.cpp
    src/engine/LayeredWindow.cpp
    src/engine/MonitorUtil.cpp
    src/engine/D2DWindow.cpp
)
```

- [ ] **Step 6: Build and confirm no regression**

```bash
cmd //c "\"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat\" x64 && cd /d \"D:\GitHub Projects\Argos\.claude\worktrees\component-4-manager-app\" && cmake -S . -B build -G Ninja && cmake --build build"
```

Expected: builds cleanly with no errors. Every pre-existing target
(`argos_engine`, `argos_engine_demo`, `argos_ini_parser_test`,
`argos_skin`, `argos_skin_demo`) must still build — this step only added
code, it changed no existing method's signature in a way callers use
differently (`BeginDraw()`'s new parameter has a default, so
`LayeredWindow::Render()`'s existing no-argument call still compiles and
behaves identically). `D2DWindow`/`D2DContext::CreateForHwnd` have no
consumer yet in this task — that's exercised starting in Task 3.

- [ ] **Step 7: Commit**

```bash
git add src/engine/D2DContext.h src/engine/D2DContext.cpp src/engine/D2DWindow.h src/engine/D2DWindow.cpp CMakeLists.txt
git commit -m "Add D2DContext HWND-render-target mode and D2DWindow base class"
```

## Task 2: SkinRegistry (scan + merge)

**Files:**
- Create: `src/manager/SkinRegistry.h`
- Create: `src/manager/SkinRegistry.cpp`
- Test: `src/manager/SkinRegistryTest.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: nothing (pure `std::filesystem` + Win32 file-time APIs).
- Produces: `struct SkinEntry { std::wstring name; std::wstring iniPath;
  FILETIME lastWriteTime; bool enabled; bool loadFailed; std::string
  loadError; }`, `ScanSkinsDirectory(const std::wstring& root) ->
  std::vector<SkinEntry>`, `MergeSkinsScan(std::vector<SkinEntry>&
  existing, const std::vector<SkinEntry>& freshScan)`. Task 3
  (`ManagerWindow`) consumes `SkinEntry` for display; Task 4 (`main.cpp`)
  consumes both functions plus `SkinEntry` for the real scan/reload logic.

- [ ] **Step 1: Write the failing test**

Create `src/manager/SkinRegistryTest.cpp`:

```cpp
#include "manager/SkinRegistry.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace argos;
namespace fs = std::filesystem;

namespace {
void WriteFile(const fs::path& path, const std::string& content) {
    std::ofstream f(path, std::ios::binary);
    f << content;
}
}

int main() {
    fs::path root = fs::temp_directory_path() / "argos_skin_registry_test";
    std::error_code ec;
    fs::remove_all(root, ec); // clean slate if a prior run left it behind
    fs::create_directories(root / "Clock");
    fs::create_directories(root / "CPU");
    fs::create_directories(root / "Empty"); // no .ini -- must be skipped
    WriteFile(root / "Clock" / "skin.ini", "[Widget]\n");
    WriteFile(root / "CPU" / "cpu.ini", "[Widget]\n"); // non-"skin.ini" name, must still be found

    // ScanSkinsDirectory: finds Clock and CPU, skips Empty, sorted by name.
    {
        auto entries = ScanSkinsDirectory(root.wstring());
        assert(entries.size() == 2);
        assert(entries[0].name == L"CPU");
        assert(entries[0].iniPath == (root / "CPU" / "cpu.ini").wstring());
        assert(entries[1].name == L"Clock");
        assert(entries[1].iniPath == (root / "Clock" / "skin.ini").wstring());
        assert(!entries[0].enabled && !entries[0].loadFailed);
    }

    // ScanSkinsDirectory on a nonexistent root: empty, no throw.
    {
        auto entries = ScanSkinsDirectory((root / "does_not_exist").wstring());
        assert(entries.empty());
    }

    // MergeSkinsScan: preserves enabled state for a skin still present,
    // drops one that's gone, adds a new one disabled by default.
    {
        std::vector<SkinEntry> existing = ScanSkinsDirectory(root.wstring());
        existing[1].enabled = true; // Clock, per the sorted order above

        fs::remove_all(root / "CPU");
        fs::create_directories(root / "RAM");
        WriteFile(root / "RAM" / "skin.ini", "[Widget]\n");

        auto fresh = ScanSkinsDirectory(root.wstring());
        MergeSkinsScan(existing, fresh);

        assert(existing.size() == 2); // Clock, RAM -- CPU dropped
        assert(existing[0].name == L"Clock");
        assert(existing[0].enabled == true); // preserved
        assert(existing[1].name == L"RAM");
        assert(existing[1].enabled == false); // new entry starts disabled
    }

    fs::remove_all(root, ec);
    printf("SkinRegistry: all checks passed\n");
    return 0;
}
```

- [ ] **Step 2: Add the CMake test target and confirm it fails to build (no implementation yet)**

In `CMakeLists.txt`, add after the `argos_ini_parser_test` block:

```cmake
add_executable(argos_skin_registry_test
    src/manager/SkinRegistry.cpp
    src/manager/SkinRegistryTest.cpp
)
target_include_directories(argos_skin_registry_test PRIVATE src)
```

Run (same build command as Task 1 Step 6). Expected: FAILS — `src/manager/SkinRegistry.h` and `.cpp` don't exist yet.

- [ ] **Step 3: Write SkinRegistry.h**

Create `src/manager/SkinRegistry.h`:

```cpp
#pragma once
#include <string>
#include <vector>
#include <windows.h>

namespace argos {

// One discovered skin folder under a skins root directory:
// <root>/<Name>/<some-file>.ini. Per docs/SKIN_FORMAT.md, the config file
// has no mandated filename -- the first *.ini file found directly inside
// the folder (alphabetically) is used, matching LoadSkin's "accepts any
// path" contract.
struct SkinEntry {
    std::wstring name;        // the folder name, e.g. L"Clock"
    std::wstring iniPath;     // full path to the folder's *.ini file
    FILETIME lastWriteTime{}; // of iniPath, for change detection on refresh
    bool enabled = false;
    bool loadFailed = false;
    std::string loadError;    // set iff loadFailed
};

// Scans `root` for immediate subdirectories that contain at least one
// `*.ini` file, and returns one SkinEntry per such subdirectory, sorted by
// name. Subdirectories with no `.ini` file are skipped. Returns an empty
// vector (never throws) if `root` doesn't exist or is empty -- "no skins
// installed yet" is a normal, non-error state (design spec section 6:
// never a crash).
std::vector<SkinEntry> ScanSkinsDirectory(const std::wstring& root);

// Merges a fresh ScanSkinsDirectory() result into `existing` in place:
// entries whose name+iniPath match one in `existing` keep that entry's
// `enabled`/`loadFailed`/`loadError` state (a refresh must not silently
// disable a skin that's still there) but take `freshScan`'s
// lastWriteTime; entries no longer present in `freshScan` are removed;
// entirely new folders are appended, disabled by default. Callers that
// need to detect "this enabled skin's file changed on disk" must snapshot
// each entry's old lastWriteTime before calling this (it gets overwritten
// here) and compare against the corresponding entry's new value after.
void MergeSkinsScan(std::vector<SkinEntry>& existing, const std::vector<SkinEntry>& freshScan);

}
```

- [ ] **Step 4: Write SkinRegistry.cpp**

Create `src/manager/SkinRegistry.cpp`:

```cpp
#include "manager/SkinRegistry.h"
#include <algorithm>
#include <filesystem>

using namespace argos;
namespace fs = std::filesystem;

namespace {
FILETIME GetLastWriteTime(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        return data.ftLastWriteTime;
    }
    return FILETIME{};
}
}

std::vector<SkinEntry> argos::ScanSkinsDirectory(const std::wstring& root) {
    std::vector<SkinEntry> entries;
    try {
        if (!fs::is_directory(root)) {
            return entries;
        }
        for (const auto& dirEntry : fs::directory_iterator(root)) {
            if (!dirEntry.is_directory()) continue;
            std::wstring iniPath;
            for (const auto& fileEntry : fs::directory_iterator(dirEntry.path())) {
                if (fileEntry.is_regular_file() && fileEntry.path().extension() == L".ini") {
                    iniPath = fileEntry.path().wstring();
                    break;
                }
            }
            if (iniPath.empty()) continue;
            SkinEntry entry;
            entry.name = dirEntry.path().filename().wstring();
            entry.iniPath = iniPath;
            entry.lastWriteTime = GetLastWriteTime(iniPath);
            entries.push_back(std::move(entry));
        }
    } catch (const std::exception&) {
        // A directory disappearing/permission error mid-scan, etc. --
        // treated the same as "nothing found yet", per design spec
        // section 6's never-crash-on-a-skin-problem policy.
    }
    std::sort(entries.begin(), entries.end(),
              [](const SkinEntry& a, const SkinEntry& b) { return a.name < b.name; });
    return entries;
}

void argos::MergeSkinsScan(std::vector<SkinEntry>& existing, const std::vector<SkinEntry>& freshScan) {
    std::vector<SkinEntry> merged;
    merged.reserve(freshScan.size());
    for (const auto& fresh : freshScan) {
        auto it = std::find_if(existing.begin(), existing.end(), [&](const SkinEntry& e) {
            return e.name == fresh.name && e.iniPath == fresh.iniPath;
        });
        if (it != existing.end()) {
            SkinEntry kept = *it;
            kept.lastWriteTime = fresh.lastWriteTime;
            merged.push_back(std::move(kept));
        } else {
            merged.push_back(fresh);
        }
    }
    existing = std::move(merged);
}
```

- [ ] **Step 5: Build and run the test**

```bash
cmd //c "\"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat\" x64 && cd /d \"D:\GitHub Projects\Argos\.claude\worktrees\component-4-manager-app\" && cmake -S . -B build -G Ninja && cmake --build build && build\argos_skin_registry_test.exe"
```

Expected: builds, then prints `SkinRegistry: all checks passed` and exits 0.

- [ ] **Step 6: Commit**

```bash
git add src/manager/SkinRegistry.h src/manager/SkinRegistry.cpp src/manager/SkinRegistryTest.cpp CMakeLists.txt
git commit -m "Add SkinRegistry: scan a skins directory, merge rescans"
```

## Task 3: ManagerWindow (list rendering + hit-testing) with a visual smoke check

**Files:**
- Create: `src/manager/ManagerWindow.h`
- Create: `src/manager/ManagerWindow.cpp`
- Create: `src/manager/main.cpp` (placeholder fake data — Task 4 replaces its body)
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `argos::SkinEntry` (Task 2), `argos::D2DWindow` (Task 1).
- Produces: `class ManagerWindow : public D2DWindow` with
  `SetEntries(std::vector<SkinEntry>)`, and public callbacks
  `std::function<void(size_t index)> onToggleRequested` /
  `std::function<void()> onRefreshRequested` that Task 4's `main.cpp`
  wires to real behavior.

- [ ] **Step 1: Write ManagerWindow.h**

Create `src/manager/ManagerWindow.h`:

```cpp
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
```

- [ ] **Step 2: Write ManagerWindow.cpp**

Create `src/manager/ManagerWindow.cpp`:

```cpp
#include "manager/ManagerWindow.h"

using namespace argos;

namespace {
constexpr float kRefreshButtonX = 12.0f;
constexpr float kRefreshButtonY = 12.0f;
constexpr float kRefreshButtonW = 88.0f;
constexpr float kRefreshButtonH = 28.0f;
constexpr float kListTop = 52.0f;
constexpr float kRowHeight = 36.0f;
constexpr float kRowLeft = 12.0f;
constexpr float kRowWidth = 296.0f;
constexpr float kToggleSize = 16.0f;
}

void ManagerWindow::SetEntries(std::vector<SkinEntry> entries) {
    m_entries = std::move(entries);
    Render();
}

void ManagerWindow::OnPaint(D2DContext& ctx, float dipWidth, float dipHeight) {
    auto format = ctx.CreateTextFormat(L"Segoe UI", 14.0f);

    D2D1_RECT_F refreshRect{ kRefreshButtonX, kRefreshButtonY,
                             kRefreshButtonX + kRefreshButtonW, kRefreshButtonY + kRefreshButtonH };
    ctx.FillRoundedRect(refreshRect, 4.0f, D2D1::ColorF(0.30f, 0.30f, 0.32f));
    if (format) {
        ctx.DrawText(refreshRect, L"Refresh", format.Get(), D2D1::ColorF(D2D1::ColorF::White));
    }

    for (size_t i = 0; i < m_entries.size(); ++i) {
        const SkinEntry& entry = m_entries[i];
        float rowTop = kListTop + static_cast<float>(i) * kRowHeight;
        if (rowTop + kRowHeight > dipHeight) break; // v1 has no scrolling -- see class comment

        D2D1_RECT_F toggleRect{ kRowLeft, rowTop + (kRowHeight - kToggleSize) / 2.0f,
                                 kRowLeft + kToggleSize, rowTop + (kRowHeight - kToggleSize) / 2.0f + kToggleSize };
        D2D1_COLOR_F toggleColor = entry.loadFailed ? D2D1::ColorF(0.80f, 0.25f, 0.25f)
                                  : entry.enabled   ? D2D1::ColorF(0.35f, 0.70f, 0.40f)
                                                     : D2D1::ColorF(0.45f, 0.45f, 0.47f);
        ctx.FillRoundedRect(toggleRect, 3.0f, toggleColor);

        std::wstring label = entry.name;
        if (entry.loadFailed) {
            label += L" (failed to load)";
        } else if (entry.enabled) {
            label += L" (enabled)";
        }
        if (format) {
            D2D1_RECT_F labelRect{ kRowLeft + kToggleSize + 10.0f, rowTop,
                                    kRowLeft + kRowWidth, rowTop + kRowHeight };
            ctx.DrawText(labelRect, label.c_str(), format.Get(), D2D1::ColorF(D2D1::ColorF::Black));
        }
    }
}

void ManagerWindow::OnLButtonUp(float dipX, float dipY) {
    if (dipX >= kRefreshButtonX && dipX <= kRefreshButtonX + kRefreshButtonW &&
        dipY >= kRefreshButtonY && dipY <= kRefreshButtonY + kRefreshButtonH) {
        if (onRefreshRequested) onRefreshRequested();
        return;
    }
    for (size_t i = 0; i < m_entries.size(); ++i) {
        float rowTop = kListTop + static_cast<float>(i) * kRowHeight;
        if (dipY >= rowTop && dipY < rowTop + kRowHeight &&
            dipX >= kRowLeft && dipX <= kRowLeft + kRowWidth) {
            if (onToggleRequested) onToggleRequested(i);
            return;
        }
    }
}

void ManagerWindow::OnDestroy() {
    PostQuitMessage(0);
}
```

- [ ] **Step 3: Write a placeholder main.cpp with fake data**

Create `src/manager/main.cpp`:

```cpp
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
```

- [ ] **Step 4: Add the argos_manager CMake target**

In `CMakeLists.txt`, add after the `argos_skin_demo` block:

```cmake
add_executable(argos_manager WIN32
    src/manager/SkinRegistry.cpp
    src/manager/ManagerWindow.cpp
    src/manager/main.cpp
)
target_link_libraries(argos_manager PRIVATE argos_skin)
if(MSVC)
  set_target_properties(argos_manager PROPERTIES
      LINK_FLAGS "/MANIFEST:EMBED /MANIFESTINPUT:\"${CMAKE_SOURCE_DIR}/app.manifest\" /INCREMENTAL:NO")
endif()
```

- [ ] **Step 5: Build and visually verify**

```bash
cmd //c "\"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat\" x64 && cd /d \"D:\GitHub Projects\Argos\.claude\worktrees\component-4-manager-app\" && cmake -S . -B build -G Ninja && cmake --build build"
```

Expected: builds cleanly. Launch `build\argos_manager.exe` in the
background (no arguments — this task's main.cpp doesn't read any yet).
Screenshot the window (it opens at screen position 400,100; screenshot a
generous region like (380,80)-(760,540) to include the title bar). Expect:
a normal titled window ("Argos Manager"), a dark "Refresh" rounded button
top-left, and four rows below it (Clock, CPU, RAM, Disk) each with a gray
toggle indicator and name, except Disk's indicator is red with "(failed to
load)" appended to its label. Click the Clock row (use the screenshot to
find its on-screen coordinates), screenshot again, confirm Clock's
indicator turned green and its label now reads "Clock (enabled)". Click it
again, confirm it reverts to gray/"Clock". Kill the process afterward.

- [ ] **Step 6: Commit**

```bash
git add src/manager/ManagerWindow.h src/manager/ManagerWindow.cpp src/manager/main.cpp CMakeLists.txt
git commit -m "Add ManagerWindow: skin list rendering and click hit-testing"
```

## Task 4: Real behavior — SkinWidgetWindow + SkinRegistry wiring in main.cpp

**Files:**
- Create: `src/manager/SkinWidgetWindow.h`
- Modify: `src/manager/main.cpp` (full rewrite of Task 3's placeholder body)
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `argos::LayeredWindow` (Component 1), `argos::Skin` /
  `argos::LoadSkin` (Component 2), `argos::EnumerateMonitors` /
  `argos::MonitorInfo` (Component 1), `argos::SkinEntry` /
  `ScanSkinsDirectory` / `MergeSkinsScan` (Task 2), `ManagerWindow` (Task
  3).
- Produces: `class SkinWidgetWindow : public LayeredWindow` (header-only,
  no separate `.cpp` needed — every member is a one-line inline function,
  matching this class's own small scope).

- [ ] **Step 1: Write SkinWidgetWindow.h**

Create `src/manager/SkinWidgetWindow.h`:

```cpp
#pragma once
#include <memory>
#include "engine/LayeredWindow.h"
#include "skin/Skin.h"

namespace argos {

// Hosts one loaded skin as its own on-screen widget window. Identical in
// spirit to skin_demo.cpp's single hardcoded widget, but reusable: the
// manager creates and destroys any number of these as skins are
// enabled/disabled or reloaded. Reuses LayeredWindow's own built-in
// drag-to-reposition (design spec section 8's "reposition: dragging the
// widget itself") -- no drag code needed here.
class SkinWidgetWindow : public LayeredWindow {
public:
    void SetSkin(std::unique_ptr<Skin> skin) { m_skin = std::move(skin); }

    void OnPaint(D2DContext& ctx, float, float) override {
        if (m_skin) m_skin->RenderMeters(ctx);
    }
    void OnTimer() override {
        if (m_skin) {
            m_skin->UpdateMeasures();
            Render();
        }
    }
    // Deliberately no OnDestroy() override: LayeredWindow's default is a
    // no-op, which is correct here -- a widget closing (because the
    // manager disabled it) must never quit the whole manager process,
    // only the manager's own window closing does that (see
    // ManagerWindow::OnDestroy).

private:
    std::unique_ptr<Skin> m_skin;
};

}
```

- [ ] **Step 2: Rewrite main.cpp with the real wiring**

Replace the full contents of `src/manager/main.cpp`:

```cpp
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <map>
#include <memory>
#include <vector>
#include "engine/MonitorUtil.h"
#include "manager/ManagerWindow.h"
#include "manager/SkinRegistry.h"
#include "manager/SkinWidgetWindow.h"

using namespace argos;

namespace {

std::wstring g_skinsDir;
std::vector<SkinEntry> g_entries;
std::vector<MonitorInfo> g_monitors;
std::map<std::wstring, std::unique_ptr<SkinWidgetWindow>> g_activeWidgets; // keyed by iniPath
ManagerWindow* g_manager = nullptr;

std::wstring DefaultSkinsDir() {
    PWSTR path = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path))) {
        result = std::wstring(path) + L"\\Argos\\Skins";
    }
    if (path) CoTaskMemFree(path);
    return result;
}

// Loads entry.iniPath and, on success, creates and shows its widget
// window (added to g_activeWidgets); on failure, marks the entry failed
// and leaves it out of g_activeWidgets. Always leaves entry.enabled
// consistent with whether a widget now exists for it.
void SpawnWidget(SkinEntry& entry) {
    auto widget = std::make_unique<SkinWidgetWindow>();
    // Create() needs an initial size before a skin is loaded; every v1
    // bundled skin (and this canvas convention generally) targets the
    // same 260x140 DIP canvas argos_skin_demo.exe uses (design spec
    // section 7 / docs/2026-08-22-argos-plan-03-bundled-skins.md).
    if (!widget->Create(entry.name.c_str(), 0, 0, 260, 140)) {
        entry.loadFailed = true;
        entry.loadError = "failed to create widget window";
        entry.enabled = false;
        return;
    }

    SkinLoadResult loaded = LoadSkin(entry.iniPath, widget->Context());
    if (!loaded.skin) {
        entry.loadFailed = true;
        entry.loadError = loaded.error;
        entry.enabled = false;
        return; // widget destructs here (goes out of scope), nothing shown
    }

    entry.loadFailed = false;
    entry.loadError.clear();
    loaded.skin->UpdateMeasures();
    int intervalMs = loaded.skin->widget.updateIntervalMs;
    int monitorIndex = loaded.skin->widget.monitorIndex;
    int x = loaded.skin->widget.x;
    int y = loaded.skin->widget.y;
    widget->SetSkin(std::move(loaded.skin));
    widget->PlaceOnMonitor(g_monitors, monitorIndex, x, y);
    widget->Render();
    widget->SetUpdateTimer(static_cast<UINT>(intervalMs));
    g_activeWidgets[entry.iniPath] = std::move(widget);
}

// Reconciles g_activeWidgets against g_entries' current enabled flags:
// drops widgets for entries that are gone or disabled, spawns widgets for
// entries that are enabled but don't have one yet. The single place that
// makes "what's on screen" match "what's marked enabled" -- called after
// every mutation (a toggle, or a refresh).
void SyncWidgetsToEntries() {
    for (auto it = g_activeWidgets.begin(); it != g_activeWidgets.end();) {
        auto entryIt = std::find_if(g_entries.begin(), g_entries.end(),
            [&](const SkinEntry& e) { return e.iniPath == it->first; });
        if (entryIt == g_entries.end() || !entryIt->enabled) {
            it = g_activeWidgets.erase(it);
        } else {
            ++it;
        }
    }
    for (auto& entry : g_entries) {
        if (entry.enabled && g_activeWidgets.find(entry.iniPath) == g_activeWidgets.end()) {
            SpawnWidget(entry);
        }
    }
}

void RefreshSkins() {
    auto fresh = ScanSkinsDirectory(g_skinsDir);

    // Snapshot each known entry's old lastWriteTime before MergeSkinsScan
    // overwrites it in place, so a changed file can be detected below.
    std::map<std::wstring, FILETIME> oldTimes;
    for (const auto& e : g_entries) oldTimes[e.iniPath] = e.lastWriteTime;

    MergeSkinsScan(g_entries, fresh);

    for (auto& entry : g_entries) {
        auto it = oldTimes.find(entry.iniPath);
        bool changed = it != oldTimes.end() &&
                       CompareFileTime(&it->second, &entry.lastWriteTime) != 0;
        if (changed && entry.enabled) {
            // Evict so SyncWidgetsToEntries() below treats it as
            // missing-and-enabled, which respawns (reloads) it.
            g_activeWidgets.erase(entry.iniPath);
        }
    }

    SyncWidgetsToEntries();
    g_manager->SetEntries(g_entries);
}

void ToggleSkin(size_t index) {
    if (index >= g_entries.size()) return;
    g_entries[index].enabled = !g_entries[index].enabled;
    SyncWidgetsToEntries();
    g_manager->SetEntries(g_entries);
}

}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR cmdLine, int) {
    g_skinsDir = (cmdLine && cmdLine[0] != L'\0') ? cmdLine : DefaultSkinsDir();
    g_monitors = EnumerateMonitors();

    static ManagerWindow manager;
    g_manager = &manager;
    manager.Create(L"Argos Manager", 400, 100, 320, 400);
    manager.onToggleRequested = [](size_t index) { ToggleSkin(index); };
    manager.onRefreshRequested = []() { RefreshSkins(); };

    RefreshSkins(); // initial scan

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
```

- [ ] **Step 3: Link the two extra Windows import libraries**

In `CMakeLists.txt`, change the `argos_manager` target's link line from:

```cmake
target_link_libraries(argos_manager PRIVATE argos_skin)
```

to:

```cmake
target_link_libraries(argos_manager PRIVATE argos_skin shell32 ole32)
```

(`shell32` for `SHGetKnownFolderPath`, `ole32` for `CoTaskMemFree` — both
used only by `DefaultSkinsDir()` in `main.cpp`.)

- [ ] **Step 4: Build**

```bash
cmd //c "\"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat\" x64 && cd /d \"D:\GitHub Projects\Argos\.claude\worktrees\component-4-manager-app\" && cmake -S . -B build -G Ninja && cmake --build build"
```

Expected: builds cleanly.

- [ ] **Step 5: Set up a test skins directory (does not touch the real `%LOCALAPPDATA%`)**

```bash
mkdir -p "$TEMP/argos_manager_test_skins"
cp -r "D:/GitHub Projects/Argos/.claude/worktrees/component-4-manager-app/skins/Clock" "$TEMP/argos_manager_test_skins/"
cp -r "D:/GitHub Projects/Argos/.claude/worktrees/component-4-manager-app/skins/CPU" "$TEMP/argos_manager_test_skins/"
cp -r "D:/GitHub Projects/Argos/.claude/worktrees/component-4-manager-app/skins/RAM" "$TEMP/argos_manager_test_skins/"
cp -r "D:/GitHub Projects/Argos/.claude/worktrees/component-4-manager-app/skins/Disk" "$TEMP/argos_manager_test_skins/"
mkdir -p "$TEMP/argos_manager_test_skins/Broken"
cp "D:/GitHub Projects/Argos/.claude/worktrees/component-4-manager-app/test_skins/broken/skin.ini" "$TEMP/argos_manager_test_skins/Broken/"
```

(If `test_skins/broken/skin.ini` isn't present in this worktree, write one
directly — any content `docs/SKIN_FORMAT.md` documents as invalid works,
e.g. a `[Widget]` section followed by a line that isn't `key=value`.)

- [ ] **Step 6: Run and verify end to end**

Launch `build\argos_manager.exe "<the test skins dir from Step 5>"` in the
background. Screenshot the manager window (region around
(380,80)-(760,540)). Expect five rows, alphabetical: Broken, CPU, Clock,
Disk, RAM, all gray/disabled.

Click the Clock row. Screenshot again: Clock's indicator is green,
labeled "(enabled)", and a small 260x140 Clock widget window has appeared
elsewhere on screen showing the current time (screenshot that region too
and confirm the seconds are advancing across a ~2 second gap, same
technique as Component 3's Clock skin verification).

Click the Clock row again: confirm the indicator returns to gray and the
Clock widget window closes (no longer present on screen).

Click Clock to re-enable it, then edit the test copy's
`Clock/skin.ini` (`X=40` -> `X=400` in the `[Widget]` section, a large,
visually obvious move) and click Refresh. Screenshot: confirm the Clock
widget visibly moved to the new position — proving refresh detected the
file change and reloaded it (not just re-listed it).

Click the Broken row (the deliberately invalid skin). Confirm: its
indicator turns red, its label gets "(failed to load)" appended, no
widget window appears, and the manager itself does not crash (design spec
section 6).

Kill `argos_manager.exe` and any still-open widget windows afterward
(`taskkill /IM argos_manager.exe /F`, `taskkill /IM SkinWidgetWindow.exe
/F` is not a real process name — widgets run in-process with the manager,
so killing `argos_manager.exe` closes them too).

- [ ] **Step 7: Commit**

```bash
git add src/manager/SkinWidgetWindow.h src/manager/main.cpp CMakeLists.txt
git commit -m "Wire ManagerWindow to SkinRegistry and SkinWidgetWindow"
```

## Task 5: README update, open the PR

**Files:**
- Modify: `README.md`

**Interfaces:** none (project glue only).

- [ ] **Step 1: Update README.md's Status section**

Replace the `## Status` section with:

```markdown
## Status

Under active development. The rendering/window engine, the skin format
(INI-style config parser, the four v1 measures, the two v1 meters, and a
skin loader), the four bundled v1 skins (Clock, CPU, RAM, Disk, under
`skins/`), and the manager application (lists installed skins, enables/
disables them, and refreshes/reloads on change — `argos_manager.exe`) are
built; persistence/auto-start and the installer are still to come. See
`docs/2026-08-19-argos-design.md` for the full design and
`docs/SKIN_FORMAT.md` for the skin config format.
```

- [ ] **Step 2: Build one more time to make sure nothing broke**

Same build command as Task 1 Step 6.

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "Update README for the manager application component"
```

- [ ] **Step 4: Push and open the PR**

```bash
git push -u origin component-4-manager-app
gh pr create --title "Component 4: Manager application" --body "Implements design spec section 8: a single Direct2D-rendered manager window (argos_manager.exe) that lists installed skins, enables/disables each one (spawning/closing its widget window), and refreshes (re-scanning for new/removed skins and reloading ones whose file changed on disk). No Win32 Common Controls, no property editor, no plugin/DLL loading. Reposition works via the existing widget drag support (Component 1) -- no new drag code. Verified end to end against the four bundled skins plus a deliberately broken one: enable/disable, live reload on file change, and graceful failure display all confirmed live. Closes #<issue-number-from-Task-0>." --base main
```

- [ ] **Step 5: Wait for CI, self-review, merge**

Watch the PR's checks with `gh pr checks --watch`. Once green, read through
the full diff once more (`gh pr diff`) for anything sloppy, fix and push if
needed, then:

```bash
gh pr merge --merge --delete-branch
```

Report back to the user: Component 4 is merged into `main`, with the
manager application listing/enabling/disabling/refreshing skins verified
live against all four bundled skins plus a deliberately broken one.
