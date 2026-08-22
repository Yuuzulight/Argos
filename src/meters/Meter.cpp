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
        std::wstring wfont = ToWide(font);
        const std::string* sizeStr = config.Find("Size");
        float size = sizeStr ? static_cast<float>(std::atof(sizeStr->c_str())) : 14.0f;
        D2D1_COLOR_F color = ReadColor(config, "Color", D2D1::ColorF(D2D1::ColorF::White));

        auto format = ctx.CreateTextFormat(wfont.c_str(), size);
        if (!format) {
            outError = "failed to create text format for font \"" + font + "\"";
            return nullptr;
        }
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
