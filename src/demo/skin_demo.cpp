#include <windows.h>
#include <string>
#include "engine/LayeredWindow.h"
#include "skin/Skin.h"

using namespace argos;

namespace {

class SkinWidget : public LayeredWindow {
public:
    void SetSkin(std::unique_ptr<Skin> skin) {
        m_skin = std::move(skin);
    }

    void OnPaint(D2DContext& ctx, float, float) override {
        if (m_skin) {
            m_skin->RenderMeters(ctx);
        }
    }

    void OnTimer() override {
        if (m_skin) {
            m_skin->UpdateMeasures();
            Render();
        }
    }

    void OnDestroy() override {
        PostQuitMessage(0);
    }

private:
    std::unique_ptr<Skin> m_skin;
};

}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR cmdLine, int nCmdShow) {
    static SkinWidget widget;
    widget.Create(L"Argos Skin Demo", 40, 40, 260, 140);

    std::wstring path = (cmdLine && cmdLine[0] != L'\0') ? cmdLine : L"test_skins\\demo\\skin.ini";
    SkinLoadResult loaded = LoadSkin(path, widget.Context());
    if (!loaded.skin) {
        MessageBoxW(nullptr, std::wstring(loaded.error.begin(), loaded.error.end()).c_str(),
                    L"Argos Skin Demo — failed to load skin", MB_ICONERROR);
        return 1;
    }

    loaded.skin->UpdateMeasures();
    int intervalMs = loaded.skin->widget.updateIntervalMs;
    widget.SetSkin(std::move(loaded.skin));
    widget.Render();
    widget.SetUpdateTimer(static_cast<UINT>(intervalMs));

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
