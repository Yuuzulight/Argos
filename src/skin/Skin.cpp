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
