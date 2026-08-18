# Argos — Design Spec

Date: 2026-08-19
Status: Approved

## 1. What Argos is

A native C++/Win32 desktop widget/skin engine for Windows, in the spirit of
Rainmeter: a background app that draws its own transparent, always-on-top
windows ("widgets") on the desktop showing live system data (clock, CPU%,
RAM%, disk usage), fully configurable via a plain-text skin format, managed
through an in-app UI.

Argos is explicitly **not** a taskbar replacement or shell modification. It
never touches, injects into, or patches `explorer.exe` or any other process.
Every widget is Argos's own window, drawn with standard, documented Windows
APIs (layered windows + Direct2D). This is the core safety constraint for the
whole project: **no process injection, DLL injection, or memory patching of
another process, ever, for any feature.**

Target audience is real external users from day one — a stranger should be
able to download the installer, run it, and get a working first impression
without touching the source.

## 2. Non-goals for v1

- Taskbar modification/replacement of any kind
- Process injection / hooking / patching another process's memory
- A plugin system or third-party native-code (DLL) loading
- A GUI property editor for skin appearance (skins are hand-edited configs)
- A full/expanded widget library beyond the 4 bundled sample skins
- A scripting language for skins
- Code signing

## 3. Technology stack

| Concern | Choice | Why |
|---|---|---|
| Language | C++17 | Enough for COM-heavy Win32/Direct2D code; keeps toolchain requirements low. |
| Windowing | Win32, layered windows (`WS_EX_LAYERED`) | Required for per-pixel alpha transparency. |
| Rendering | Direct2D + DirectWrite, for widgets *and* the manager UI | One graphics technology, one visual style app-wide; no Win32 Common Controls. |
| Build system | CMake + Ninja | Standard, well-supported, works headlessly from a Developer command prompt. |
| Compiler | MSVC (cl.exe), via VS2022 Build Tools | Native Windows toolchain; no MinGW/GCC quirks with COM/Direct2D headers. |
| Third-party deps | **None** beyond the Windows SDK | Direct2D/DirectWrite/Win32 ship in the SDK. Skin format and persisted state both use a hand-written INI parser — one text format for the whole app, zero deps to vendor or fetch. |
| Installer | Inno Setup | Single readable script, no XML, standard choice for small unsigned open-source Win32 tools. |
| Testing | Manual verification at each checkpoint + a debug log file | The quality bar in scope is graceful failure and reasoned-through DPI/multi-monitor logic, not a unit-test suite. A parser-only test target can be added later if it earns its keep. |

## 4. Repo layout

```
Argos/
  CMakeLists.txt
  src/
    engine/       # layered window + Direct2D/DirectWrite rendering, drag, click-through, per-monitor DPI
    skin/         # INI parser, Measure/Meter model, skin loader
    measures/     # Clock, CPUUsage, MemoryUsage, DiskUsage
    meters/       # Text, Bar
    manager/       # manager UI (Direct2D-rendered), skin list, enable/disable, refresh
    persistence/   # state.ini read/write, Startup-folder shortcut creation
    app/           # entry point, wiring
  skins/           # bundled v1 skins (Clock, CPU, RAM, Disk) — installed alongside the app
  docs/
    SKIN_FORMAT.md
    2026-08-19-argos-design.md   # this file
  installer/       # Inno Setup script
  .github/workflows/build.yml    # CI build check
  README.md
```

## 5. Component 1 — Rendering / window engine

Built and proven first, before anything else depends on it.

- Per-widget: layered (`WS_EX_LAYERED`), borderless, always-on-top, transparent
  Win32 window.
- Drawing via Direct2D: solid fills, DirectWrite text, basic shapes/bars.
- Drag-to-reposition is part of the window engine itself (hit-test in
  `WM_LBUTTONDOWN`/`WM_MOUSEMOVE`, not bolted on later) — the manager's
  reposition feature (§8) reuses this directly rather than re-implementing
  drag.
- Per-skin click-through mode (`WS_EX_TRANSPARENT` toggle), default off.
- Multi-monitor coordinates: a widget's position is stored as (monitor,
  x-offset-from-monitor-origin, y-offset), not a raw virtual-desktop pixel,
  so a saved position still makes sense if monitors are rearranged.
- Per-monitor DPI awareness: manifest declares
  `PerMonitorV2` DPI awareness; `WM_DPICHANGED` is handled by rescaling the
  Direct2D render target and repositioning the window into the suggested
  rect Windows provides in `lParam`.

**Hardware-verification note:** this dev machine has two physical monitors
at different DPI (96 and 192, i.e. 100%/200% scale), so `WM_DPICHANGED`
fires for real when a widget is moved between them via `PlaceOnMonitor` --
this was exercised directly, not just reasoned through, and it's how a real
DPI-rescale bug was found and verified fixed (the render target wasn't
being told the new DPI, so drawing didn't rescale with the window). Widget
crispness and proportional rendering across a DPI change were confirmed on
this hardware via before/after screenshots. What's still unverified on
real hardware: behavior with more than two monitors, and scale factors
other than 100%/200% (e.g. 125%, 150%, 175%) -- neither combination exists
on this dev setup.

## 6. Component 2 — Skin format

Plain-text, INI-style, Rainmeter's Measure/Meter split conceptually, but not
byte-compatible with Rainmeter syntax.

- A skin is a folder: a config file + any assets (images, fonts).
- `[Widget]` top-level section: target monitor, default x/y, update interval
  (ms), click-through on/off.
- Measures (v1 built-ins only): `Clock`, `CPUUsage`, `MemoryUsage`,
  `DiskUsage` (configurable drive letter).
- Meters: `Text` (font/size/color, bound to a measure), `Bar` (filled
  percentage bar, bound to a measure).
- No scripting. Pure key=value.
- Documented in `docs/SKIN_FORMAT.md`, written so someone can author a new
  skin without reading the C++ source.
- Malformed configs fail gracefully: a per-skin parse error is logged and
  surfaced in the manager (skin shows as "failed to load" with a reason),
  never a crash of the whole app.

## 7. Component 3 — Bundled v1 skins

Exactly four, each proving the mechanism end-to-end, kept visually simple:
Clock, CPU usage, RAM usage, Disk usage (system drive default). No attempt at
a wider widget library — that's v2+.

## 8. Component 4 — Manager application

Single Direct2D-rendered window (no Win32 Common Controls anywhere in the
UI, matching the rendering stack decision):

- Lists installed skins, scanned from `%LOCALAPPDATA%\Argos\Skins\`.
- Enable/disable per skin (spawns/closes its widget window).
- Reposition: **dragging the widget itself** (reusing the window engine's
  drag support from §5) rather than a second drag implementation inside the
  manager's list — one code path, less to get wrong, and it's a hard
  requirement of the engine anyway.
- Refresh: re-scans the skins folder for new skins and reloads a skin's
  config if its files changed on disk.
- Out of scope: no property editor for colors/fonts, no plugin/DLL loading.

## 9. Component 5 — Persistence & auto-start

- `%LOCALAPPDATA%\Argos\state.ini` — which skins are enabled, and each
  skin's current position (x, y, monitor). INI, matching the skin format's
  style rather than introducing a second text format.
- `%LOCALAPPDATA%\Argos\argos.log` — debug/error log (parse errors, startup
  issues).
- Auto-start via a shortcut in the current user's Startup folder
  (`shell:startup`), created by the installer — not a registry Run key, so
  it's visible and easy for a cautious user to remove by hand. Removal is
  documented in the README (delete the shortcut from `shell:startup`); no
  separate toggle is added in the manager UI for v1 — the brief allows
  either, and a documented manual removal is simpler and keeps the manager
  focused on skins.

## 10. Distribution

- Inno Setup installer.
- No code signing (intentional for v1). Installer UI and README both
  explain, plainly and without alarm, that SmartScreen will likely show
  "Windows protected your PC" on first run, why (new + unsigned, not
  malicious), and how to proceed (More info → Run anyway).
- Installer also creates the Startup-folder shortcut (§9) and installs the
  four bundled skins into the default skins folder so the app isn't empty on
  first launch.

## 11. Development workflow (this project specifically)

- One GitHub Issue per major component (§5–§10 above, six issues total:
  Rendering Engine, Skin Format, Bundled Skins, Manager, Persistence &
  Auto-start, Installer & Distribution), filed on
  `github.com/Yuuzulight/Argos` before work on that component starts.
- One feature branch + PR per issue. PR description says what was built and
  how it was verified.
- **CI:** `.github/workflows/build.yml` runs a Windows-runner CMake+MSVC
  build on every PR, so PRs carry a real pass/fail check beyond local
  verification.
- **Merge policy:** solo-repo flow — after the branch builds locally, CI is
  green, and I've self-reviewed the diff, I merge the PR into `main` myself
  and move to the next component's issue/branch. Progress is reported to you
  at each component boundary regardless (per your original instruction to
  show progress rather than build everything silently).

## 12. Assumptions to verify once on real hardware

1. Widget crispness/proportional rendering when moved between DPI-different
   monitors -- verified on this dev machine's two monitors (96/192 DPI,
   i.e. 100%/200% scale) (§5). Still open: more than two monitors, and
   intermediate scale factors (125%, 150%, 175%) not present on this
   dev setup.
2. Actual on-screen "Windows protected your PC" SmartScreen flow, since it
   can't be triggered/observed from this dev environment (§10).
