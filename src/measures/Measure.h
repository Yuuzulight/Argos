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

// Converts a UTF-8 string to UTF-16 via MultiByteToWideChar(CP_UTF8, ...).
// Returns an empty string both for empty input and if the conversion fails.
std::wstring ToWide(const std::string& s);

}
