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

```
cmake -S . -B build -G Ninja
cmake --build build
```

This produces `build\argos_engine_demo.exe`, a proving executable for the
rendering engine.
