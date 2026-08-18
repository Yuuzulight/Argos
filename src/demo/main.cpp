#include <windows.h>
#include "engine/LayeredWindow.h"
#include "engine/MonitorUtil.h"

using namespace argos;

namespace {

class DemoWidget : public LayeredWindow {
public:
    void OnPaint(D2DContext& ctx, int w, int h) override {
        ctx.FillRoundedRect(D2D1::RectF(0, 0, (float)w, (float)h), 12.0f,
                             D2D1::ColorF(0.10f, 0.10f, 0.12f, 0.85f));
        auto textFormat = ctx.CreateTextFormat(L"Segoe UI", 14.0f);
        const wchar_t* label = IsClickThrough() ? L"Argos (click-through ON)"
                                                 : L"Argos (drag me / press T)";
        ctx.DrawText(D2D1::RectF(12, 10, w - 12.0f, 34.0f), label, textFormat.Get(),
                     D2D1::ColorF(D2D1::ColorF::White));
        ctx.FillBar(D2D1::RectF(12, 44, w - 12.0f, 60.0f), 0.42f,
                    D2D1::ColorF(0.30f, 0.65f, 0.95f, 1.0f), D2D1::ColorF(1, 1, 1, 0.15f));
    }

    void OnKeyDown(WPARAM vk) override {
        if (vk == 'T') {
            SetClickThrough(!IsClickThrough());
            Render();
        }
    }
};

}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int nCmdShow) {
    static DemoWidget widget;
    auto monitors = EnumerateMonitors();
    widget.Create(L"Argos Engine Demo", 0, 0, 260, 90);
    widget.PlaceOnMonitor(monitors, 0, 40, 40);
    widget.Render();

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
