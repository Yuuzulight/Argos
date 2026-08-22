# Argos — Component 2: Skin Format Implementation Plan

**Goal:** Build the plain-text, INI-style skin format: a dependency-free INI
parser, the four v1 measures (Clock, CPUUsage, MemoryUsage, DiskUsage), the
two v1 meters (Text, Bar), and a skin loader that turns a skin folder's
config file into a live, renderable `Skin` object — proven end-to-end by a
demo widget (reusing Component 1's `LayeredWindow`) that loads a real skin
file and renders live system data through it.

**Architecture:** A new static library, `argos_skin`, layered on top of
`argos_engine` (Component 1, already merged): `IniParser` (free function,
no Windows dependency, turns text into sections of key=value pairs) →
`Measure`/`CreateMeasure` (the four built-in measures, one abstract
interface + factory) → `Meter`/`CreateMeter` (the two built-in meters, same
shape, each meter holds a non-owning pointer to the `Measure` it's bound
to) → `Skin`/`LoadSkin` (reads a skin's `.ini` file, builds every measure
and meter it declares, wires meter→measure bindings by name, and reports
the first error encountered rather than crashing). `LayeredWindow` gets one
small addition — an update timer — so a widget can drive a skin's
`UpdateInterval=` on its own.

**Tech Stack:** C++17, same toolchain as Component 1 (CMake + Ninja,
MSVC/VS2022 Build Tools). No third-party libraries — the four measures use
only Windows SDK APIs already available via `kernel32` (`GetSystemTimes`,
`GlobalMemoryStatusEx`, `GetDiskFreeSpaceExW`), which MSVC links by default,
so no new `target_link_libraries` entries are needed beyond what
`argos_engine` already has.

**Spec:** [docs/2026-08-19-argos-design.md](../2026-08-19-argos-design.md)
— this plan implements design spec §6 (Skin format).

## Global Constraints

- C++17 standard, no third-party C++ dependencies — Windows SDK only.
- Build via CMake + Ninja, compiled with MSVC. All build/run commands in
  this plan assume a Developer environment; use:
  `cmd /c "\"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat\" x64 && <command>"`
  run from `D:\GitHub Projects\Argos\.claude\worktrees\component-2-skin-format`.
- No process injection, DLL injection, or hooking of another process.
- The skin format is plain-text, INI-style, pure `key=value` — no
  scripting language (design spec §6).
- Malformed skins fail gracefully: reported as a string with file/line
  context, never a crash (design spec §6). Component 2 is only responsible
  for *producing* that error string — actually surfacing it in a manager UI
  ("skin shows as failed to load with a reason") is Component 4's job; this
  plan's demo shows the failure via a `MessageBoxW` as a stand-in proof,
  not the real UI.
- **Testing adaptation (per approved design spec §3):** no general
  unit-test framework in this project; each engine/skin-model task's "test"
  step is build it, run it, verify concrete behavior. Exception: the INI
  parser is pure, dependency-free logic with no Windows API surface, so it
  gets a small `assert`-based standalone test executable — exactly the
  "parser-only test target" the spec's technology table calls out as
  worth adding once it earns its keep.
- One GitHub issue per component (this plan = one issue), one feature
  branch off `main`, one PR at the end. Merge policy: self-review, confirm
  CI is green, merge into `main` (design spec §11).

## Task 0: File the issue and branch

- [ ] **Step 1: Create the GitHub issue**

```bash
cd "D:/GitHub Projects/Argos/.claude/worktrees/component-2-skin-format"
gh issue create --title "Component 2: Skin format" --body "INI-style skin parser, the four v1 measures (Clock, CPUUsage, MemoryUsage, DiskUsage), the two v1 meters (Text, Bar), and a skin loader that builds a live Skin from a skin folder's config file. Implements design spec section 6 (docs/2026-08-19-argos-design.md). Proven via an argos_skin_demo executable rendering a real skin file's live data, plus an assert-based argos_ini_parser_test."
```

Note the issue number returned.

- [ ] **Step 2: Create the feature branch**

```bash
git checkout -b component-2-skin-format
```

## Task 1: IniParser

**Files:**
- Create: `src/skin/IniParser.h`
- Create: `src/skin/IniParser.cpp`
- Create: `src/skin/IniParserTest.cpp`
- Modify: `CMakeLists.txt` (add `argos_ini_parser_test` executable target)

**Interfaces:**
- Produces: `struct argos::IniSection { name, entries, Find(key) }`,
  `struct argos::IniParseError { line, message }`,
  `struct argos::IniParseResult { sections, error, Ok(), FindSection(name) }`,
  `argos::IniParseResult argos::ParseIni(const std::string& text)`.

- [ ] **Step 1: Write IniParser.h**

```cpp
#pragma once
#include <string>
#include <vector>
#include <optional>

namespace argos {

// One `[Section]` block: its name and its `key=value` entries, in the
// order they appeared in the file.
struct IniSection {
    std::string name;
    std::vector<std::pair<std::string, std::string>> entries;

    // Returns nullptr if key isn't present in this section.
    const std::string* Find(const std::string& key) const {
        for (const auto& kv : entries) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }
};

struct IniParseError {
    int line;
    std::string message;
};

struct IniParseResult {
    std::vector<IniSection> sections;
    std::optional<IniParseError> error;

    bool Ok() const { return !error.has_value(); }

    // Returns nullptr if no section with this name exists.
    const IniSection* FindSection(const std::string& name) const {
        for (const auto& s : sections) {
            if (s.name == name) return &s;
        }
        return nullptr;
    }
};

// Parses INI-style text: `[Section]` headers, `key=value` entries, full-line
// `;` or `#` comments, blank lines ignored (every line is trimmed of
// leading/trailing whitespace first). Stops at the first malformed line --
// a non-blank/non-comment line before any `[Section]` header, a line inside
// a section that's neither `[Section]` nor `key=value`, or a duplicate
// section name -- and reports it via IniParseResult::error rather than
// throwing: a skin's config is untrusted, hand-edited input, so a bad line
// is a data problem for the caller to surface, not a crash.
IniParseResult ParseIni(const std::string& text);

}
```

- [ ] **Step 2: Write IniParser.cpp**

```cpp
#include "skin/IniParser.h"

namespace {

std::string Trim(const std::string& s) {
    size_t begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}

std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else if (c != '\r') {
            current.push_back(c);
        }
    }
    lines.push_back(current);
    return lines;
}

}

argos::IniParseResult argos::ParseIni(const std::string& text) {
    IniParseResult result;
    std::vector<std::string> lines = SplitLines(text);

    IniSection* current = nullptr;
    for (size_t i = 0; i < lines.size(); ++i) {
        int lineNumber = static_cast<int>(i) + 1;
        std::string line = Trim(lines[i]);

        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
            std::string name = Trim(line.substr(1, line.size() - 2));
            if (name.empty()) {
                result.error = IniParseError{ lineNumber, "empty section name" };
                return result;
            }
            if (result.FindSection(name) != nullptr) {
                result.error = IniParseError{ lineNumber, "duplicate section [" + name + "]" };
                return result;
            }
            result.sections.push_back(IniSection{ name, {} });
            current = &result.sections.back();
            continue;
        }

        if (current == nullptr) {
            result.error = IniParseError{ lineNumber, "expected a [Section] header before this line" };
            return result;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            result.error = IniParseError{ lineNumber, "expected key=value or a [Section] header" };
            return result;
        }

        std::string key = Trim(line.substr(0, eq));
        std::string value = Trim(line.substr(eq + 1));
        if (key.empty()) {
            result.error = IniParseError{ lineNumber, "empty key" };
            return result;
        }
        current->entries.emplace_back(key, value);
    }

    return result;
}
```

- [ ] **Step 3: Write IniParserTest.cpp**

```cpp
#include "skin/IniParser.h"
#include <cassert>
#include <cstdio>

using namespace argos;

int main() {
    // Valid: sections, key=value, comments, blank lines.
    {
        auto result = ParseIni(
            "; comment\n"
            "[Widget]\n"
            "X=40\n"
            "Y=40\n"
            "\n"
            "# also a comment\n"
            "[MeasureClock]\n"
            "Measure=Clock\n");
        assert(result.Ok());
        assert(result.sections.size() == 2);
        assert(result.sections[0].name == "Widget");
        assert(*result.sections[0].Find("X") == "40");
        assert(result.sections[0].Find("Missing") == nullptr);
        assert(result.FindSection("MeasureClock") != nullptr);
        assert(*result.FindSection("MeasureClock")->Find("Measure") == "Clock");
    }

    // Error: key=value before any section header.
    {
        auto result = ParseIni("X=40\n");
        assert(!result.Ok());
        assert(result.error->line == 1);
    }

    // Error: line inside a section that isn't key=value.
    {
        auto result = ParseIni("[Widget]\nnotakeyvalue\n");
        assert(!result.Ok());
        assert(result.error->line == 2);
    }

    // Error: duplicate section name.
    {
        auto result = ParseIni("[Widget]\nX=1\n[Widget]\nY=2\n");
        assert(!result.Ok());
        assert(result.error->line == 3);
    }

    printf("IniParser: all checks passed\n");
    return 0;
}
```

- [ ] **Step 4: Add the CMake target**

Add to `CMakeLists.txt`, after the existing `argos_engine_demo` target:

```cmake
# Pure logic, no Windows/D2D dependency -- compiled as its own tiny target
# (not linked against argos_engine or argos_skin) so it builds fast and
# stays a true unit test of the parser alone.
add_executable(argos_ini_parser_test
    src/skin/IniParser.cpp
    src/skin/IniParserTest.cpp
)
target_include_directories(argos_ini_parser_test PRIVATE src)
```

- [ ] **Step 5: Build**

```bash
cmd /c "\"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat\" x64 && cd /d \"D:\GitHub Projects\Argos\.claude\worktrees\component-2-skin-format\" && cmake -S . -B build -G Ninja && cmake --build build --target argos_ini_parser_test"
```
Expected: builds cleanly, produces `build\argos_ini_parser_test.exe`.

- [ ] **Step 6: Run and verify**

```bash
build\argos_ini_parser_test.exe
```
Expected: prints `IniParser: all checks passed` and exits 0. If any
`assert` fails, the process aborts — that's a real failure, fix the parser
or the test before continuing.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt src/skin/IniParser.h src/skin/IniParser.cpp src/skin/IniParserTest.cpp
git commit -m "Add IniParser: dependency-free INI parsing with an assert-based test"
```

## Task 2: Measures — Clock, CPUUsage, MemoryUsage, DiskUsage

**Files:**
- Create: `src/measures/Measure.h`
- Create: `src/measures/Measure.cpp`

**Interfaces:**
- Consumes: `argos::IniSection` (Task 1).
- Produces: `class argos::Measure` with `Update()`, `ValueText() -> std::wstring`,
  `ValueFraction() -> double`; `std::unique_ptr<Measure> argos::CreateMeasure(const std::string& className, const IniSection& config, std::string& outError)`.

- [ ] **Step 1: Write Measure.h**

```cpp
#pragma once
#include <memory>
#include <string>
#include "skin/IniParser.h"

namespace argos {

// A live data source a Meter can bind to and render. Update() re-samples
// the underlying system value; ValueText()/ValueFraction() read the most
// recent sample without touching the system again, so a Skin can call
// Update() once per timer tick and then render as many meters off that
// same sample as are bound to it.
class Measure {
public:
    virtual ~Measure() = default;
    virtual void Update() = 0;
    // Human-readable current value, for Text meters.
    virtual std::wstring ValueText() const = 0;
    // Current value normalized to [0, 1], for Bar meters. Measures with no
    // natural fraction (e.g. Clock) return 0.0 rather than rejecting the
    // binding -- a Bar bound to Clock is a pointless but harmless skin.
    virtual double ValueFraction() const = 0;
};

// Builds one of the four v1 built-in measure classes ("Clock", "CPUUsage",
// "MemoryUsage", "DiskUsage") from its skin-file section. `config` supplies
// class-specific keys (currently only DiskUsage's `Drive=`). Returns
// nullptr and fills `outError` if `className` isn't one of the four.
std::unique_ptr<Measure> CreateMeasure(const std::string& className, const IniSection& config,
                                        std::string& outError);

}
```

- [ ] **Step 2: Write Measure.cpp**

```cpp
#include "measures/Measure.h"
#include <windows.h>
#include <cwchar>

using namespace argos;

namespace {

class ClockMeasure : public Measure {
public:
    void Update() override {
        GetLocalTime(&m_time);
    }
    std::wstring ValueText() const override {
        wchar_t buf[16];
        swprintf_s(buf, L"%02u:%02u:%02u", m_time.wHour, m_time.wMinute, m_time.wSecond);
        return buf;
    }
    double ValueFraction() const override {
        return 0.0;
    }

private:
    SYSTEMTIME m_time{};
};

class CPUUsageMeasure : public Measure {
public:
    void Update() override {
        FILETIME idleTime, kernelTime, userTime;
        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
            return;
        }
        ULONGLONG idle = ToULL(idleTime);
        // lpKernelTime includes idle time on Windows, so kernel+user is the
        // right "total" denominator and (total - idleDelta) is busy time.
        ULONGLONG total = ToULL(kernelTime) + ToULL(userTime);

        if (m_haveSample) {
            ULONGLONG idleDelta = idle - m_lastIdle;
            ULONGLONG totalDelta = total - m_lastTotal;
            m_fraction = totalDelta == 0 ? 0.0
                                          : 1.0 - (static_cast<double>(idleDelta) / totalDelta);
        }
        m_lastIdle = idle;
        m_lastTotal = total;
        m_haveSample = true;
    }
    std::wstring ValueText() const override {
        wchar_t buf[8];
        swprintf_s(buf, L"%d%%", static_cast<int>(m_fraction * 100.0 + 0.5));
        return buf;
    }
    double ValueFraction() const override {
        return m_fraction;
    }

private:
    static ULONGLONG ToULL(const FILETIME& ft) {
        return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    }

    bool m_haveSample = false;
    ULONGLONG m_lastIdle = 0;
    ULONGLONG m_lastTotal = 0;
    double m_fraction = 0.0;
};

class MemoryUsageMeasure : public Measure {
public:
    void Update() override {
        MEMORYSTATUSEX status{};
        status.dwLength = sizeof(status);
        if (GlobalMemoryStatusEx(&status)) {
            m_fraction = status.dwMemoryLoad / 100.0;
        }
    }
    std::wstring ValueText() const override {
        wchar_t buf[8];
        swprintf_s(buf, L"%d%%", static_cast<int>(m_fraction * 100.0 + 0.5));
        return buf;
    }
    double ValueFraction() const override {
        return m_fraction;
    }

private:
    double m_fraction = 0.0;
};

class DiskUsageMeasure : public Measure {
public:
    explicit DiskUsageMeasure(std::wstring rootPath) : m_rootPath(std::move(rootPath)) {}

    void Update() override {
        ULARGE_INTEGER freeBytes{}, totalBytes{};
        if (GetDiskFreeSpaceExW(m_rootPath.c_str(), nullptr, &totalBytes, &freeBytes) &&
            totalBytes.QuadPart != 0) {
            ULONGLONG used = totalBytes.QuadPart - freeBytes.QuadPart;
            m_fraction = static_cast<double>(used) / static_cast<double>(totalBytes.QuadPart);
        }
    }
    std::wstring ValueText() const override {
        wchar_t buf[8];
        swprintf_s(buf, L"%d%%", static_cast<int>(m_fraction * 100.0 + 0.5));
        return buf;
    }
    double ValueFraction() const override {
        return m_fraction;
    }

private:
    std::wstring m_rootPath;
    double m_fraction = 0.0;
};

std::wstring ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), len);
    w.resize(len - 1); // drop the null terminator MultiByteToWideChar counted
    return w;
}

}

std::unique_ptr<Measure> argos::CreateMeasure(const std::string& className, const IniSection& config,
                                               std::string& outError) {
    if (className == "Clock") {
        return std::make_unique<ClockMeasure>();
    }
    if (className == "CPUUsage") {
        return std::make_unique<CPUUsageMeasure>();
    }
    if (className == "MemoryUsage") {
        return std::make_unique<MemoryUsageMeasure>();
    }
    if (className == "DiskUsage") {
        const std::string* drive = config.Find("Drive");
        std::string letter = (drive && !drive->empty()) ? drive->substr(0, 1) : "C";
        std::wstring root = ToWide(letter) + L":\\";
        return std::make_unique<DiskUsageMeasure>(root);
    }
    outError = "unknown measure class \"" + className + "\"";
    return nullptr;
}
```

- [ ] **Step 3: Add sources to CMakeLists.txt**

This file becomes part of the new `argos_skin` library target added in Task
4 (that's the first task that also needs `Meter.cpp`, so the CMake edit is
made once, there, rather than twice). No CMake change in this step.

- [ ] **Step 4: Commit**

```bash
git add src/measures/Measure.h src/measures/Measure.cpp
git commit -m "Add the four v1 measures: Clock, CPUUsage, MemoryUsage, DiskUsage"
```

(This commit isn't independently buildable yet — nothing references these
files in CMakeLists.txt until Task 4. That's fine; Task 4's build step is
the first real compile/link check for this code, and it happens before the
PR is opened.)

## Task 3: Meters — Text, Bar

**Files:**
- Create: `src/meters/Meter.h`
- Create: `src/meters/Meter.cpp`

**Interfaces:**
- Consumes: `argos::Measure` (Task 2), `argos::D2DContext` (Component 1:
  `DrawText`, `FillBar`, `CreateTextFormat`).
- Produces: `class argos::Meter` with `Render(D2DContext&)`;
  `std::unique_ptr<Meter> argos::CreateMeter(const std::string& className, const IniSection& config, Measure* measure, D2DContext& ctx, std::string& outError)`.

- [ ] **Step 1: Write Meter.h**

```cpp
#pragma once
#include <memory>
#include <string>
#include "skin/IniParser.h"
#include "measures/Measure.h"
#include "engine/D2DContext.h"

namespace argos {

// A drawable element bound to a Measure, positioned at a fixed rect within
// its widget (DIPs, matching LayeredWindow::OnPaint's coordinate system).
// Holds a non-owning pointer to its Measure -- both are owned together by
// the same Skin and share its lifetime, so a Meter is never rendered after
// its Measure is destroyed.
class Meter {
public:
    virtual ~Meter() = default;
    virtual void Render(D2DContext& ctx) = 0;
};

// Builds one of the two v1 built-in meter classes ("Text", "Bar") from its
// skin-file section, already bound to `measure`. `ctx` is the D2DContext
// the meter will later be rendered through -- needed now so Text meters can
// create their IDWriteTextFormat up front rather than on every frame.
// Returns nullptr and fills `outError` if `className` isn't one of the two.
std::unique_ptr<Meter> CreateMeter(const std::string& className, const IniSection& config,
                                    Measure* measure, D2DContext& ctx, std::string& outError);

}
```

- [ ] **Step 2: Write Meter.cpp**

```cpp
#include "meters/Meter.h"
#include <cstdlib>

using namespace argos;

namespace {

D2D1_RECT_F ReadBounds(const IniSection& config) {
    auto readFloat = [&config](const char* key) -> float {
        const std::string* v = config.Find(key);
        return v ? static_cast<float>(std::atof(v->c_str())) : 0.0f;
    };
    float x = readFloat("X");
    float y = readFloat("Y");
    float w = readFloat("W");
    float h = readFloat("H");
    return D2D1::RectF(x, y, x + w, y + h);
}

D2D1_COLOR_F ReadColor(const IniSection& config, const char* key, D2D1_COLOR_F fallback) {
    const std::string* v = config.Find(key);
    if (!v || v->size() != 8) {
        return fallback;
    }
    // AARRGGBB, two hex digits per channel.
    auto hexByte = [&](size_t pos) {
        return static_cast<float>(std::strtoul(v->substr(pos, 2).c_str(), nullptr, 16)) / 255.0f;
    };
    float a = hexByte(0);
    float r = hexByte(2);
    float g = hexByte(4);
    float b = hexByte(6);
    return D2D1::ColorF(r, g, b, a);
}

class TextMeter : public Meter {
public:
    TextMeter(Measure* measure, D2D1_RECT_F bounds, Microsoft::WRL::ComPtr<IDWriteTextFormat> format,
              D2D1_COLOR_F color)
        : m_measure(measure), m_bounds(bounds), m_format(std::move(format)), m_color(color) {}

    void Render(D2DContext& ctx) override {
        ctx.DrawText(m_bounds, m_measure->ValueText().c_str(), m_format.Get(), m_color);
    }

private:
    Measure* m_measure;
    D2D1_RECT_F m_bounds;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> m_format;
    D2D1_COLOR_F m_color;
};

class BarMeter : public Meter {
public:
    BarMeter(Measure* measure, D2D1_RECT_F bounds, D2D1_COLOR_F fillColor, D2D1_COLOR_F trackColor)
        : m_measure(measure), m_bounds(bounds), m_fillColor(fillColor), m_trackColor(trackColor) {}

    void Render(D2DContext& ctx) override {
        ctx.FillBar(m_bounds, static_cast<float>(m_measure->ValueFraction()), m_fillColor, m_trackColor);
    }

private:
    Measure* m_measure;
    D2D1_RECT_F m_bounds;
    D2D1_COLOR_F m_fillColor;
    D2D1_COLOR_F m_trackColor;
};

}

std::unique_ptr<Meter> argos::CreateMeter(const std::string& className, const IniSection& config,
                                           Measure* measure, D2DContext& ctx, std::string& outError) {
    D2D1_RECT_F bounds = ReadBounds(config);

    if (className == "Text") {
        const std::string* fontName = config.Find("Font");
        std::string font = fontName ? *fontName : "Segoe UI";
        std::wstring wfont(font.begin(), font.end());
        const std::string* sizeStr = config.Find("Size");
        float size = sizeStr ? static_cast<float>(std::atof(sizeStr->c_str())) : 14.0f;
        D2D1_COLOR_F color = ReadColor(config, "Color", D2D1::ColorF(D2D1::ColorF::White));

        auto format = ctx.CreateTextFormat(wfont.c_str(), size);
        return std::make_unique<TextMeter>(measure, bounds, std::move(format), color);
    }
    if (className == "Bar") {
        D2D1_COLOR_F fill = ReadColor(config, "FillColor", D2D1::ColorF(0.30f, 0.65f, 0.95f, 1.0f));
        D2D1_COLOR_F track = ReadColor(config, "TrackColor", D2D1::ColorF(1, 1, 1, 0.15f));
        return std::make_unique<BarMeter>(measure, bounds, fill, track);
    }

    outError = "unknown meter class \"" + className + "\"";
    return nullptr;
}
```

- [ ] **Step 3: Commit**

```bash
git add src/meters/Meter.h src/meters/Meter.cpp
git commit -m "Add the two v1 meters: Text and Bar"
```

(Same note as Task 2 — not wired into CMake or compiled until Task 4.)

## Task 4: Skin loader, the `argos_skin` library, and SKIN_FORMAT.md

**Files:**
- Create: `src/skin/Skin.h`
- Create: `src/skin/Skin.cpp`
- Create: `docs/SKIN_FORMAT.md`
- Modify: `CMakeLists.txt` (add the `argos_skin` static library target)

**Interfaces:**
- Consumes: `argos::IniSection`/`ParseIni` (Task 1), `argos::Measure`/`CreateMeasure`
  (Task 2), `argos::Meter`/`CreateMeter` (Task 3).
- Produces: `struct argos::WidgetConfig { monitorIndex, x, y, updateIntervalMs, clickThrough }`,
  `class argos::Skin { widget, measures, meters, UpdateMeasures(), RenderMeters(D2DContext&) }`,
  `struct argos::SkinLoadResult { skin, error }`,
  `argos::SkinLoadResult argos::LoadSkin(const std::wstring& iniPath, D2DContext& ctx)`.

- [ ] **Step 1: Write Skin.h**

```cpp
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "measures/Measure.h"
#include "meters/Meter.h"

namespace argos {

struct WidgetConfig {
    int monitorIndex = 0;
    int x = 0;
    int y = 0;
    int updateIntervalMs = 1000;
    bool clickThrough = false;
};

// A loaded skin: its [Widget] placement/timing config, and every measure
// and meter it defined. Measures and meters are owned here -- a Meter's
// Measure* stays valid for the Skin's whole lifetime, since both come from
// the same Skin and are destroyed together.
class Skin {
public:
    WidgetConfig widget;
    std::vector<std::unique_ptr<Measure>> measures;
    std::vector<std::unique_ptr<Meter>> meters;

    void UpdateMeasures() {
        for (auto& m : measures) m->Update();
    }
    void RenderMeters(D2DContext& ctx) {
        for (auto& m : meters) m->Render(ctx);
    }
};

struct SkinLoadResult {
    std::unique_ptr<Skin> skin;  // null on failure
    std::string error;           // set iff skin is null
};

// Loads a skin from the config file at `iniPath`. `ctx` is the D2DContext
// the skin's meters will be rendered through (needed up front so Text
// meters can build their IDWriteTextFormat at load time). Never throws --
// a missing [Widget] section, an unknown measure/meter class, a meter's
// MeasureName= referencing an undefined measure, or a malformed line in
// the file itself are all reported via SkinLoadResult::error with
// file/line context baked into the message where available, per design
// spec section 6 ("a per-skin parse error is logged and surfaced ... never
// a crash").
SkinLoadResult LoadSkin(const std::wstring& iniPath, D2DContext& ctx);

}
```

- [ ] **Step 2: Write Skin.cpp**

```cpp
#include "skin/Skin.h"
#include "skin/IniParser.h"
#include <fstream>
#include <sstream>
#include <map>
#include <cstdlib>

using namespace argos;

namespace {

std::string ReadFile(const std::wstring& path, bool& ok) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        ok = false;
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    ok = true;
    return ss.str();
}

int ReadInt(const IniSection& section, const char* key, int fallback) {
    const std::string* v = section.Find(key);
    return v ? std::atoi(v->c_str()) : fallback;
}

bool ReadBool(const IniSection& section, const char* key, bool fallback) {
    const std::string* v = section.Find(key);
    if (!v) return fallback;
    return *v == "1" || *v == "true" || *v == "True";
}

}

SkinLoadResult argos::LoadSkin(const std::wstring& iniPath, D2DContext& ctx) {
    SkinLoadResult result;

    bool readOk = false;
    std::string text = ReadFile(iniPath, readOk);
    if (!readOk) {
        result.error = "could not open skin file";
        return result;
    }

    IniParseResult parsed = ParseIni(text);
    if (!parsed.Ok()) {
        result.error = "line " + std::to_string(parsed.error->line) + ": " + parsed.error->message;
        return result;
    }

    const IniSection* widgetSection = parsed.FindSection("Widget");
    if (!widgetSection) {
        result.error = "missing [Widget] section";
        return result;
    }

    auto skin = std::make_unique<Skin>();
    skin->widget.monitorIndex = ReadInt(*widgetSection, "Monitor", 0);
    skin->widget.x = ReadInt(*widgetSection, "X", 40);
    skin->widget.y = ReadInt(*widgetSection, "Y", 40);
    skin->widget.updateIntervalMs = ReadInt(*widgetSection, "UpdateInterval", 1000);
    skin->widget.clickThrough = ReadBool(*widgetSection, "ClickThrough", false);

    // Pass 1: every section with a Measure= key becomes a Measure, indexed
    // by section name so meters can bind to it by that name in pass 2.
    std::map<std::string, Measure*> measuresByName;
    for (const IniSection& section : parsed.sections) {
        if (section.name == "Widget") continue;
        const std::string* measureClass = section.Find("Measure");
        if (!measureClass) continue;

        std::string error;
        auto measure = CreateMeasure(*measureClass, section, error);
        if (!measure) {
            result.error = "[" + section.name + "]: " + error;
            return result;
        }
        measuresByName[section.name] = measure.get();
        skin->measures.push_back(std::move(measure));
    }

    // Pass 2: every section with a Meter= key becomes a Meter, bound to the
    // measure its MeasureName= key names.
    for (const IniSection& section : parsed.sections) {
        if (section.name == "Widget") continue;
        const std::string* meterClass = section.Find("Meter");
        if (!meterClass) continue;

        const std::string* measureName = section.Find("MeasureName");
        if (!measureName) {
            result.error = "[" + section.name + "]: meter is missing MeasureName=";
            return result;
        }
        auto it = measuresByName.find(*measureName);
        if (it == measuresByName.end()) {
            result.error = "[" + section.name + "]: MeasureName=" + *measureName + " is not a defined measure";
            return result;
        }

        std::string error;
        auto meter = CreateMeter(*meterClass, section, it->second, ctx, error);
        if (!meter) {
            result.error = "[" + section.name + "]: " + error;
            return result;
        }
        skin->meters.push_back(std::move(meter));
    }

    result.skin = std::move(skin);
    return result;
}
```

- [ ] **Step 3: Add the `argos_skin` library target**

Add to `CMakeLists.txt`, after the existing `argos_ini_parser_test` target
(from Task 1):

```cmake
add_library(argos_skin STATIC
    src/skin/IniParser.cpp
    src/skin/Skin.cpp
    src/measures/Measure.cpp
    src/meters/Meter.cpp
)
target_include_directories(argos_skin PUBLIC src)
target_link_libraries(argos_skin PUBLIC argos_engine)
```

(`argos_skin` doesn't repeat `argos_engine`'s `UNICODE`/`_WIN32_WINNT`
compile definitions or Windows SDK link libraries -- both are declared
`PUBLIC` on `argos_engine` already, so linking it pulls them in.)

- [ ] **Step 4: Write docs/SKIN_FORMAT.md**

```markdown
# Argos skin format

A skin is a folder containing one `.ini`-style config file (plain text,
`key=value`, no scripting) plus any assets it uses. This document is the
whole format -- if you can author an `.ini` file, you can author a skin.

## `[Widget]` (required, exactly one)

Controls where the widget appears and how it behaves.

| Key | Type | Default | Meaning |
|---|---|---|---|
| `Monitor` | integer | `0` | Index into the monitor list (0 = primary/first enumerated). |
| `X` | integer | `40` | Horizontal offset in pixels from the target monitor's top-left corner. |
| `Y` | integer | `40` | Vertical offset in pixels from the target monitor's top-left corner. |
| `UpdateInterval` | integer | `1000` | Milliseconds between measure updates/redraws. |
| `ClickThrough` | `0`/`1`/`true` | `0` | If `1` or `true`, clicks pass through the widget to the desktop underneath. |

## Measures

A measure is a live data source. Declare one per section, giving the
section any name you like, with a `Measure=` key naming its class:

```ini
[MeasureClock]
Measure=Clock
```

| Class | Extra keys | `ValueText()` | `ValueFraction()` |
|---|---|---|---|
| `Clock` | none | `HH:MM:SS`, 24-hour, local time | always `0` |
| `CPUUsage` | none | `NN%` system-wide CPU usage | `0.0`-`1.0` |
| `MemoryUsage` | none | `NN%` physical memory in use | `0.0`-`1.0` |
| `DiskUsage` | `Drive` (single letter, default `C`) | `NN%` of that drive's capacity in use | `0.0`-`1.0` |

## Meters

A meter is a drawable element bound to one measure by name. Every meter
needs geometry (`X`/`Y`/`W`/`H`, integers, DIPs relative to the widget's
top-left corner) and `MeasureName=` naming the measure section it reads:

```ini
[MeterClockText]
Meter=Text
MeasureName=MeasureClock
X=12
Y=10
W=236
H=24
Font=Segoe UI
Size=18
Color=FFFFFFFF
```

| Class | Extra keys | Renders |
|---|---|---|
| `Text` | `Font` (default `Segoe UI`), `Size` (points, default `14`), `Color` (`AARRGGBB` hex, default `FFFFFFFF`) | The bound measure's `ValueText()` |
| `Bar` | `FillColor`, `TrackColor` (`AARRGGBB` hex, default a light blue fill / faint white track) | A filled bar sized to the bound measure's `ValueFraction()` |

Colors are 8 hex digits: **A**lpha, **R**ed, **G**reen, **B**lue, each
`00`-`FF`. `FFFFFFFF` is opaque white; `80000000` is 50%-opaque black.

## A minimal skin

```ini
[Widget]
Monitor=0
X=40
Y=40
UpdateInterval=1000

[MeasureClock]
Measure=Clock

[MeterClockText]
Meter=Text
MeasureName=MeasureClock
X=12
Y=10
W=200
H=24
```

## Errors

A skin that fails to load — a bad line, a missing `[Widget]` section, an
unknown measure/meter class, or a meter's `MeasureName=` pointing at
nothing — never crashes Argos. The failure reason names the offending
section or line number; the manager application (a later component) shows
it next to that skin instead of loading it.
```

- [ ] **Step 5: Build**

```bash
cmd /c "\"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat\" x64 && cd /d \"D:\GitHub Projects\Argos\.claude\worktrees\component-2-skin-format\" && cmake -S . -B build -G Ninja && cmake --build build --target argos_skin"
```
Expected: `argos_skin.lib` builds cleanly (fix any errors in Task 2/3's
code now that it's actually compiled for the first time — this is the "run
to see it fail, then fix" cycle for this component, same as Component 1).

- [ ] **Step 6: Commit**

```bash
git add src/skin/Skin.h src/skin/Skin.cpp docs/SKIN_FORMAT.md CMakeLists.txt
git commit -m "Add Skin loader, argos_skin library target, and SKIN_FORMAT.md"
```

## Task 5: Update timer + live skin demo

**Files:**
- Modify: `src/engine/LayeredWindow.h` (add `SetUpdateTimer`/`OnTimer`)
- Modify: `src/engine/LayeredWindow.cpp` (implement it, handle `WM_TIMER`)
- Create: `src/demo/skin_demo.cpp`
- Create: `test_skins/demo/skin.ini`
- Create: `test_skins/broken/skin.ini`
- Modify: `CMakeLists.txt` (add `argos_skin_demo` executable target)

**Interfaces:**
- Consumes: `argos::Skin`/`LoadSkin` (Task 4), `argos::LayeredWindow` (Component 1).
- Produces: `LayeredWindow::SetUpdateTimer(UINT intervalMs)`, `LayeredWindow::OnTimer()` virtual hook.

- [ ] **Step 1: Add SetUpdateTimer/OnTimer to LayeredWindow.h**

In `src/engine/LayeredWindow.h`, add to the public section, right after
`PlaceOnMonitor`'s declaration:

```cpp
    // Starts (or restarts, if already running) a periodic WM_TIMER that
    // calls OnTimer() every intervalMs. Pass 0 to stop it.
    void SetUpdateTimer(UINT intervalMs);
```

And add to the virtual hooks section, next to `OnDpiChanged`:

```cpp
    virtual void OnTimer() {}
```

- [ ] **Step 2: Implement it in LayeredWindow.cpp**

Add a timer ID constant next to `kWindowClassName`:

```cpp
const wchar_t* kWindowClassName = L"ArgosLayeredWindowClass";
constexpr UINT_PTR kUpdateTimerId = 1;
```

Add the method definition, after `PlaceOnMonitor`'s definition:

```cpp
void LayeredWindow::SetUpdateTimer(UINT intervalMs) {
    KillTimer(m_hwnd, kUpdateTimerId);
    if (intervalMs > 0) {
        SetTimer(m_hwnd, kUpdateTimerId, intervalMs, nullptr);
    }
}
```

Add a case to the `switch` in `WndProc`, right before `case WM_KEYDOWN:`:

```cpp
    case WM_TIMER:
        OnTimer();
        return 0;
```

- [ ] **Step 3: Write the demo skin fixture**

`test_skins/demo/skin.ini`:

```ini
[Widget]
Monitor=0
X=40
Y=40
UpdateInterval=1000
ClickThrough=0

[MeasureClock]
Measure=Clock

[MeasureCPU]
Measure=CPUUsage

[MeasureMem]
Measure=MemoryUsage

[MeterClockText]
Meter=Text
MeasureName=MeasureClock
X=12
Y=10
W=236
H=24
Font=Segoe UI
Size=18
Color=FFFFFFFF

[MeterCPUBar]
Meter=Bar
MeasureName=MeasureCPU
X=12
Y=44
W=236
H=16
FillColor=FF4DA6F2
TrackColor=26FFFFFF

[MeterMemText]
Meter=Text
MeasureName=MeasureMem
X=12
Y=66
W=236
H=20
Font=Segoe UI
Size=12
Color=CCFFFFFF
```

`test_skins/broken/skin.ini` (a meter binding to a measure that doesn't
exist, to prove the graceful-failure path):

```ini
[Widget]
X=40
Y=40

[MeterOops]
Meter=Text
MeasureName=DoesNotExist
X=0
Y=0
W=10
H=10
```

These are dev fixtures for this proof, not the four bundled skins Component
3 will ship — those live under `skins/` per the repo layout in design spec
section 4, and are out of scope here.

- [ ] **Step 4: Write skin_demo.cpp**

```cpp
#include <windows.h>
#include <string>
#include "engine/LayeredWindow.h"
#include "skin/Skin.h"

using namespace argos;

namespace {

class SkinWidget : public LayeredWindow {
public:
    void SetSkin(std::unique_ptr<Skin> skin) {
        m_skin = std::move(skin);
    }

    void OnPaint(D2DContext& ctx, float, float) override {
        if (m_skin) {
            m_skin->RenderMeters(ctx);
        }
    }

    void OnTimer() override {
        if (m_skin) {
            m_skin->UpdateMeasures();
            Render();
        }
    }

    void OnDestroy() override {
        PostQuitMessage(0);
    }

private:
    std::unique_ptr<Skin> m_skin;
};

}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR cmdLine, int nCmdShow) {
    static SkinWidget widget;
    widget.Create(L"Argos Skin Demo", 40, 40, 260, 140);

    std::wstring path = (cmdLine && cmdLine[0] != L'\0') ? cmdLine : L"test_skins\\demo\\skin.ini";
    SkinLoadResult loaded = LoadSkin(path, widget.Context());
    if (!loaded.skin) {
        MessageBoxW(nullptr, std::wstring(loaded.error.begin(), loaded.error.end()).c_str(),
                    L"Argos Skin Demo \u2014 failed to load skin", MB_ICONERROR);
        return 1;
    }

    loaded.skin->UpdateMeasures();
    int intervalMs = loaded.skin->widget.updateIntervalMs;
    widget.SetSkin(std::move(loaded.skin));
    widget.Render();
    widget.SetUpdateTimer(static_cast<UINT>(intervalMs));

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
```

- [ ] **Step 5: Add the CMake target**

Add to `CMakeLists.txt`, after the `argos_skin` library target:

```cmake
add_executable(argos_skin_demo WIN32
    src/demo/skin_demo.cpp
)
target_link_libraries(argos_skin_demo PRIVATE argos_skin)
if(MSVC)
  set_target_properties(argos_skin_demo PROPERTIES
      LINK_FLAGS "/MANIFEST:EMBED /MANIFESTINPUT:\"${CMAKE_SOURCE_DIR}/app.manifest\" /INCREMENTAL:NO")
endif()
```

- [ ] **Step 6: Build**

```bash
cmd /c "\"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat\" x64 && cd /d \"D:\GitHub Projects\Argos\.claude\worktrees\component-2-skin-format\" && cmake -S . -B build -G Ninja && cmake --build build"
```
Expected: builds cleanly (all five targets: `argos_engine`, `argos_skin`,
`argos_engine_demo`, `argos_skin_demo`, `argos_ini_parser_test`), produces
`build\argos_skin_demo.exe`.

- [ ] **Step 7: Run and verify live rendering**

From `D:\GitHub Projects\Argos\.claude\worktrees\component-2-skin-format` (so the demo's relative skin path
resolves), launch `build\argos_skin_demo.exe` in the background. Take a
screenshot of the desktop region around (40,40)-(300,180) using a short
PowerShell script (`Add-Type -AssemblyName System.Drawing` +
`CopyFromScreen`), save it to the scratchpad, and view it with the Read
tool. Expected: a dark rounded-looking widget-less box (no background fill
is drawn by this demo's skin — only the three meters) showing the current
time, a CPU-usage bar, and a memory-usage percentage, all readable against
the desktop showing through around them. Wait 3 seconds, screenshot again,
and confirm the clock text advanced by approximately 3 seconds — proof
`UpdateInterval`/`WM_TIMER`/`Measure::Update()` are wired correctly end to
end, not just rendered once at load. Kill the process afterward.

- [ ] **Step 8: Run and verify graceful failure**

```bash
build\argos_skin_demo.exe test_skins\broken\skin.ini
```
Expected: a message box titled "Argos Skin Demo — failed to load skin"
appears with the text `[MeterOops]: MeasureName=DoesNotExist is not a
defined measure`, and the process exits (code 1) after it's dismissed --
no crash, no partially-drawn window. Dismiss the dialog to let the process
exit.

- [ ] **Step 9: Commit**

```bash
git add -A
git commit -m "Add update timer to LayeredWindow and a live skin_demo"
```

## Task 6: README update, open the PR

**Files:**
- Modify: `README.md`

**Interfaces:** none (project glue only).

- [ ] **Step 1: Update README.md's Status and Building sections**

Replace the `## Status` section with:

```markdown
## Status

Under active development. The rendering/window engine (layered,
draggable, click-through-capable, per-monitor-DPI-aware windows drawn
with Direct2D/DirectWrite) and the skin format (INI-style config parser,
the four v1 measures, the two v1 meters, and a skin loader) are built; the
bundled skins, manager UI, persistence, and installer are still to come.
See `docs/2026-08-19-argos-design.md` for the full design and
`docs/SKIN_FORMAT.md` for the skin config format.
```

Append to the end of `## Building`, after the existing paragraph about
`build\argos_engine_demo.exe`:

```markdown

This also produces `build\argos_skin_demo.exe`, which loads and renders
`test_skins\demo\skin.ini` (run it from the repo root so that relative
path resolves, or pass a skin path as its one command-line argument), and
`build\argos_ini_parser_test.exe`, an assert-based check of the INI
parser.
```

- [ ] **Step 2: Build one more time to make sure nothing broke**

Same build command as Task 5, Step 6.

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "Update README for the skin format component"
```

- [ ] **Step 4: Push and open the PR**

```bash
git push -u origin component-2-skin-format
gh pr create --title "Component 2: Skin format" --body "Implements design spec section 6: an INI-style skin parser, the four v1 measures (Clock, CPUUsage, MemoryUsage, DiskUsage), the two v1 meters (Text, Bar), and a skin loader that builds a live Skin from a skin folder's config file, reporting malformed skins gracefully instead of crashing. Proven via argos_skin_demo (a real skin file rendering live, updating system data through LayeredWindow's new update timer, plus a verified graceful-failure path) and an assert-based argos_ini_parser_test. Closes #<issue-number-from-Task-0>." --base main
```

- [ ] **Step 5: Wait for CI, self-review, merge**

Watch the PR's checks with `gh pr checks --watch`. Once green, read through
the full diff once more (`gh pr diff`) for anything sloppy, fix and push if
needed, then:

```bash
gh pr merge --merge --delete-branch
```

Report back to the user: Component 2 is merged into `main`, what got
proven at each step (parser assertions, live time/CPU/memory rendering
updating on its own timer, and the graceful-failure message box for a
skin with a bad measure reference).
