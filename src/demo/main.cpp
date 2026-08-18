#include <windows.h>
#include "engine/D2DContext.h"

using namespace argos;

namespace {
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ArgosD2DSmokeTest";
    RegisterClassExW(&wc);

    const int width = 260;
    const int height = 90;
    HWND hwnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                                 wc.lpszClassName, L"Argos D2D Smoke Test", WS_POPUP,
                                 200, 200, width, height, nullptr, nullptr, hInstance, nullptr);

    D2DContext ctx;
    ctx.Resize(width, height);
    ShowWindow(hwnd, nCmdShow);

    ctx.BeginDraw();
    ctx.FillRoundedRect(D2D1::RectF(0, 0, (float)width, (float)height), 12.0f,
                         D2D1::ColorF(0.10f, 0.10f, 0.12f, 0.85f));
    auto textFormat = ctx.CreateTextFormat(L"Segoe UI", 20.0f);
    ctx.DrawText(D2D1::RectF(12, 12, width - 12.0f, height - 12.0f), L"Argos engine online",
                 textFormat.Get(), D2D1::ColorF(D2D1::ColorF::White));
    ctx.EndDrawAndPresent(hwnd, 200, 200);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
