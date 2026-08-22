# Argos

A native C++/Win32 desktop widget/skin engine for Windows — draws
transparent, always-on-top "widgets" showing live system data (clock,
CPU/RAM/disk usage), configured via plain-text skin files. Not a taskbar
replacement; Argos never touches, injects into, or patches `explorer.exe`
or any other process — every widget is Argos's own window, drawn with
standard Win32/Direct2D APIs.

## Status

Under active development. The rendering/window engine, the skin format
(INI-style config parser, the four v1 measures, the two v1 meters, and a
skin loader), and the four bundled v1 skins (Clock, CPU, RAM, Disk, under
`skins/`) are built; the manager UI, persistence, and installer are still
to come. See `docs/2026-08-19-argos-design.md` for the full design and
`docs/SKIN_FORMAT.md` for the skin config format.

## Building

Requires Visual Studio 2022 Build Tools (or Visual Studio 2022) with the
"Desktop development with C++" workload, and CMake 3.20+ (VS Build Tools
ships its own, under
`Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`).

From a "Developer Command Prompt for VS 2022" (this also puts VS's own
bundled Ninja on `PATH`, so `-G Ninja` below works without installing
Ninja separately):

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Ninja is a single-config generator, so `CMAKE_BUILD_TYPE` has to be
chosen at configure time -- use `Debug` in place of `Release` above for a
debug build.

This produces `build\argos_engine_demo.exe`, a proving executable for the
rendering engine. Close it like any normal window (Alt+F4, or its
taskbar entry) -- it exits cleanly.

This also produces `build\argos_skin_demo.exe`, which loads and renders
`test_skins\demo\skin.ini` (run it from the repo root so that relative
path resolves, or pass a skin path as its one command-line argument), and
`build\argos_ini_parser_test.exe`, an assert-based check of the INI
parser.
