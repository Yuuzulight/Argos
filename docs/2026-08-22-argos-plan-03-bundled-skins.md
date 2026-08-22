# Argos — Component 3: Bundled v1 Skins Implementation Plan

**Goal:** Ship the four bundled v1 skins the design spec calls for — Clock,
CPU usage, RAM usage, Disk usage — each a self-contained skin folder under
`skins/`, kept visually simple, each proven to actually load and render
live data through the existing engine.

**Architecture:** This component adds no new C++ code. Components 1 and 2
already built everything a skin needs: `argos_skin_demo.exe <path-to-skin.ini>`
(merged in Component 2) loads any skin file and renders it live in a
260x140 DIP window, updating on its own timer. Component 3 is purely
content: four `skins/<Name>/skin.ini` files, each written against
`docs/SKIN_FORMAT.md`, each proven by pointing the existing demo at it and
watching it render real system data.

**Tech Stack:** No new dependencies. Plain-text `.ini` skin files only,
verified against the already-built `argos_skin_demo.exe`.

**Spec:** [docs/2026-08-19-argos-design.md](../2026-08-19-argos-design.md)
— this plan implements design spec §7 (Component 3 — Bundled v1 skins):
"Exactly four, each proving the mechanism end-to-end, kept visually
simple: Clock, CPU usage, RAM usage, Disk usage (system drive default).
No attempt at a wider widget library — that's v2+."

## Global Constraints

- No new C++ source — this component only adds `.ini` content under
  `skins/`, per the repo layout in design spec §4 ("bundled v1 skins
  (Clock, CPU, RAM, Disk) — installed alongside the app").
- Every skin file must follow `docs/SKIN_FORMAT.md` exactly — no format
  extensions, no new measure/meter classes. v1 has exactly four measure
  classes (`Clock`, `CPUUsage`, `MemoryUsage`, `DiskUsage`) and two meter
  classes (`Text`, `Bar`); there is no static-label meter, so a skin can
  only display values bound to a live measure, never fixed caption text.
- Every meter needs `MeasureName=` bound to a measure section that exists
  in the same file (`Skin.cpp`'s loader rejects anything else with a
  specific error — proven in Component 2).
- Verification target is `build\argos_skin_demo.exe <skin path>`, which
  creates a fixed 260x140 DIP window (hardcoded in
  `src/demo/skin_demo.cpp`) — no host application resizes widgets per
  skin yet (that's Component 4, the manager app). Every bundled skin's
  meters must fit inside that 260x140 canvas.
- "System drive default" (design spec §7) means the `DiskUsage` measure's
  `[MeasureDisk]` section omits `Drive=` entirely — `CreateMeasure` in
  `src/measures/Measure.cpp` already defaults an absent/empty `Drive=` to
  `"C"`.
- Build via CMake + Ninja, MSVC, same Developer-environment command as
  Components 1-2:
  `cmd /c "\"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat\" x64 && <command>"`
  run from the repo root.
- **Testing adaptation:** no automated test to write — this component's
  entire deliverable is data (`.ini` files), and its "test" is exactly
  what the design spec calls for: prove each skin renders correctly by
  actually running it (`argos_skin_demo.exe`) and looking at a screenshot.
- One GitHub issue per component (this plan = one issue), one feature
  branch off `main`, one PR at the end. Merge policy: self-review, confirm
  CI is green, merge into `main` (design spec §11).

## Task 0: File the issue and branch

- [ ] **Step 1: Create the GitHub issue**

```bash
gh issue create --title "Component 3: Bundled v1 skins" --body "The four bundled v1 skins design spec section 7 calls for: Clock, CPU usage, RAM usage, Disk usage (system drive default). No new engine code -- pure skin content under skins/, each proven via the existing argos_skin_demo.exe."
```

Note the issue number returned.

- [ ] **Step 2: Create the feature branch**

```bash
git checkout -b component-3-bundled-skins
```

## Task 1: Clock skin

**Files:**
- Create: `skins/Clock/skin.ini`

**Interfaces:**
- Consumes: `Measure=Clock` (design spec §6 / `src/measures/Measure.cpp`
  — `ClockMeasure::ValueText()` returns `HH:MM:SS`, 24-hour, local time),
  `Meter=Text` (design spec §6 / `src/meters/Meter.cpp`).

- [ ] **Step 1: Write skins/Clock/skin.ini**

```ini
[Widget]
Monitor=0
X=40
Y=40
UpdateInterval=1000
ClickThrough=0

[MeasureClock]
Measure=Clock

[MeterClockText]
Meter=Text
MeasureName=MeasureClock
X=16
Y=44
W=228
H=56
Font=Segoe UI
Size=40
Color=FFFFFFFF
```

- [ ] **Step 2: Build (first task in a fresh worktree, so build from scratch)**

```bash
cmd /c "\"C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat\" x64 && cd /d \"D:\GitHub Projects\Argos\.claude\worktrees\component-3-bundled-skins\" && cmake -S . -B build -G Ninja && cmake --build build"
```
Expected: builds cleanly. This produces `build\argos_skin_demo.exe`
(nothing in this task changes its source, so this is just making sure a
build exists to run against).

- [ ] **Step 3: Run and verify**

From the repo root, launch `build\argos_skin_demo.exe skins\Clock\skin.ini`
in the background. Screenshot the desktop region around (40,40)-(300,180)
(same technique as Component 2: a short PowerShell script with
`Add-Type -AssemblyName System.Drawing` + `CopyFromScreen`, saved to the
scratchpad, viewed with the Read tool). Expected: large white text showing
the current time in `HH:MM:SS` format, readable against the desktop
showing through around it. Wait ~3 seconds, screenshot again, confirm the
seconds advanced (proves the update timer is actually driving this skin,
not just a static first frame). Kill the process afterward.

- [ ] **Step 4: Commit**

```bash
git add skins/Clock/skin.ini
git commit -m "Add bundled Clock skin"
```

## Task 2: CPU usage skin

**Files:**
- Create: `skins/CPU/skin.ini`

**Interfaces:**
- Consumes: `Measure=CPUUsage` (`src/measures/Measure.cpp` —
  `CPUUsageMeasure`: `ValueText()` returns `NN%`, `ValueFraction()`
  returns `0.0`-`1.0`; reads `0%` on its first `Update()` after load,
  since it's a delta measure with no prior sample yet), `Meter=Text` and
  `Meter=Bar` (`src/meters/Meter.cpp`).

- [ ] **Step 1: Write skins/CPU/skin.ini**

```ini
[Widget]
Monitor=0
X=40
Y=40
UpdateInterval=1000
ClickThrough=0

[MeasureCPU]
Measure=CPUUsage

[MeterCPUText]
Meter=Text
MeasureName=MeasureCPU
X=16
Y=30
W=228
H=44
Font=Segoe UI
Size=32
Color=FFFFFFFF

[MeterCPUBar]
Meter=Bar
MeasureName=MeasureCPU
X=16
Y=84
W=228
H=18
FillColor=FF4DA6F2
TrackColor=26FFFFFF
```

- [ ] **Step 2: Build**

Same build command as Task 1, Step 2 (no source changed since Task 1, so
this should be a no-op rebuild — run it anyway to keep the checkpoint
uniform across tasks).

- [ ] **Step 3: Run and verify**

From the repo root, launch `build\argos_skin_demo.exe skins\CPU\skin.ini`
in the background. Screenshot the same region as Task 1. Expected: large
percentage text at top, a filled bar below it showing the same value
proportionally. Since `CPUUsage` reads `0%` on its very first sample, wait
~2 seconds before the first screenshot so the timer has ticked past the
first `Update()` call at least once, then screenshot again ~3 seconds
later and confirm the bar/percentage are plausible CPU-usage values (not
frozen, not obviously wrong like negative or over 100%). Kill the process
afterward.

- [ ] **Step 4: Commit**

```bash
git add skins/CPU/skin.ini
git commit -m "Add bundled CPU usage skin"
```

## Task 3: RAM usage skin

**Files:**
- Create: `skins/RAM/skin.ini`

**Interfaces:**
- Consumes: `Measure=MemoryUsage` (`src/measures/Measure.cpp` —
  `MemoryUsageMeasure`: `ValueText()` returns `NN%`, `ValueFraction()`
  returns `0.0`-`1.0`, sourced from `GlobalMemoryStatusEx`'s
  `dwMemoryLoad`, valid from the very first `Update()` call — unlike CPU,
  no first-sample delay), `Meter=Text` and `Meter=Bar`.

- [ ] **Step 1: Write skins/RAM/skin.ini**

```ini
[Widget]
Monitor=0
X=40
Y=40
UpdateInterval=1000
ClickThrough=0

[MeasureRAM]
Measure=MemoryUsage

[MeterRAMText]
Meter=Text
MeasureName=MeasureRAM
X=16
Y=30
W=228
H=44
Font=Segoe UI
Size=32
Color=FFFFFFFF

[MeterRAMBar]
Meter=Bar
MeasureName=MeasureRAM
X=16
Y=84
W=228
H=18
FillColor=FF5FBF7A
TrackColor=26FFFFFF
```

(Fill color is a distinct green from CPU's blue, so the two skins are
visually distinguishable from each other at a glance -- both are still
plain solid fills, no gradients or extra elements, keeping with "visually
simple.")

- [ ] **Step 2: Build**

Same build command as Task 1, Step 2.

- [ ] **Step 3: Run and verify**

From the repo root, launch `build\argos_skin_demo.exe skins\RAM\skin.ini`
in the background. Screenshot the same region. Expected: large percentage
text and a green-filled bar, both showing a plausible memory-usage value
immediately (no first-sample delay for this measure). Wait ~3 seconds,
screenshot again, confirm the value is stable or has moved slightly
(memory usage doesn't need to change dramatically to prove the mechanism
-- what matters is a sane percentage rendered correctly, not a specific
delta). Kill the process afterward.

- [ ] **Step 4: Commit**

```bash
git add skins/RAM/skin.ini
git commit -m "Add bundled RAM usage skin"
```

## Task 4: Disk usage skin

**Files:**
- Create: `skins/Disk/skin.ini`

**Interfaces:**
- Consumes: `Measure=DiskUsage` (`src/measures/Measure.cpp` —
  `DiskUsageMeasure`: `ValueText()` returns `NN%`, `ValueFraction()`
  returns `0.0`-`1.0`, reads the drive named by `Drive=` -- omitted here
  so `CreateMeasure` defaults it to `"C"`, satisfying design spec §7's
  "system drive default"), `Meter=Text` and `Meter=Bar`.

- [ ] **Step 1: Write skins/Disk/skin.ini**

```ini
[Widget]
Monitor=0
X=40
Y=40
UpdateInterval=1000
ClickThrough=0

[MeasureDisk]
Measure=DiskUsage

[MeterDiskText]
Meter=Text
MeasureName=MeasureDisk
X=16
Y=30
W=228
H=44
Font=Segoe UI
Size=32
Color=FFFFFFFF

[MeterDiskBar]
Meter=Bar
MeasureName=MeasureDisk
X=16
Y=84
W=228
H=18
FillColor=FFE0A63A
TrackColor=26FFFFFF
```

(A third distinct fill color -- amber -- so all four bundled skins are
visually distinguishable from one another.)

- [ ] **Step 2: Build**

Same build command as Task 1, Step 2.

- [ ] **Step 3: Run and verify**

From the repo root, launch `build\argos_skin_demo.exe skins\Disk\skin.ini`
in the background. Screenshot the same region. Expected: large percentage
text and an amber-filled bar showing this machine's actual system-drive
(`C:`) usage -- cross-check the rendered percentage is in the right
ballpark by comparing against `Get-PSDrive C` or `Get-Volume C` output in
the same verification step, confirming `DiskUsageMeasure` is reading the
real drive, not a stub value. Kill the process afterward.

- [ ] **Step 4: Commit**

```bash
git add skins/Disk/skin.ini
git commit -m "Add bundled Disk usage skin"
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
skin loader), and the four bundled v1 skins (Clock, CPU, RAM, Disk, under
`skins/`) are built; the manager UI, persistence, and installer are still
to come. See `docs/2026-08-19-argos-design.md` for the full design and
`docs/SKIN_FORMAT.md` for the skin config format.
```

- [ ] **Step 2: Build one more time to make sure nothing broke**

Same build command as Task 1, Step 2.

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "Update README for the bundled skins component"
```

- [ ] **Step 4: Push and open the PR**

```bash
git push -u origin component-3-bundled-skins
gh pr create --title "Component 3: Bundled v1 skins" --body "Implements design spec section 7: the four bundled v1 skins (Clock, CPU usage, RAM usage, Disk usage with the system drive as default), each a self-contained skins/<Name>/skin.ini written against docs/SKIN_FORMAT.md. No new engine code -- each skin is proven end to end by pointing the existing argos_skin_demo.exe at it and confirming it renders live, correct data (including cross-checking the Disk skin's percentage against the actual C: drive usage). Closes #<issue-number-from-Task-0>." --base main
```

- [ ] **Step 5: Wait for CI, self-review, merge**

Watch the PR's checks with `gh pr checks --watch`. Once green, read through
the full diff once more (`gh pr diff`) for anything sloppy, fix and push if
needed, then:

```bash
gh pr merge --merge --delete-branch
```

Report back to the user: Component 3 is merged into `main`, with a
screenshot-backed proof point for each of the four bundled skins (Clock's
seconds advancing, CPU's percentage/bar moving off its first-sample zero,
RAM's immediate correct reading, Disk's reading cross-checked against the
real C: drive).
