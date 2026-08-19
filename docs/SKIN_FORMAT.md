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
