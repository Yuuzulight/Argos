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
