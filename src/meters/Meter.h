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
