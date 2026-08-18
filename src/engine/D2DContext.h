#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

namespace argos {

using Microsoft::WRL::ComPtr;

// Owns the process-wide Direct2D/DirectWrite factories, and one window's
// DC render target + backing 32bpp DIB section used to composite through
// UpdateLayeredWindow (the documented interop path for per-pixel-alpha
// layered windows: draw with Direct2D onto a GDI-compatible DC, then hand
// that DC to UpdateLayeredWindow).
class D2DContext {
public:
    D2DContext();
    ~D2DContext();

    D2DContext(const D2DContext&) = delete;
    D2DContext& operator=(const D2DContext&) = delete;

    // Creates/resizes the backing DIB + DC render target for the given
    // pixel size. Safe to call again (e.g. on a DPI change).
    bool Resize(int widthPx, int heightPx);

    void BeginDraw();
    // Ends the Direct2D draw and composites the frame onto hwnd at screen
    // position (screenX, screenY) with full per-pixel alpha.
    void EndDrawAndPresent(HWND hwnd, int screenX, int screenY);

    void FillRoundedRect(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color);
    void FillBar(const D2D1_RECT_F& bounds, float fraction, const D2D1_COLOR_F& fillColor,
                 const D2D1_COLOR_F& trackColor);
    void DrawText(const D2D1_RECT_F& layoutRect, const wchar_t* text, IDWriteTextFormat* format,
                  const D2D1_COLOR_F& color);
    ComPtr<IDWriteTextFormat> CreateTextFormat(const wchar_t* fontFamily, float sizePt);

private:
    ComPtr<ID2D1Factory> m_d2dFactory;
    ComPtr<IDWriteFactory> m_dwriteFactory;
    ComPtr<ID2D1DCRenderTarget> m_dcRenderTarget;

    HDC m_memDC = nullptr;
    HBITMAP m_dib = nullptr;
    HBITMAP m_oldBitmap = nullptr;
    void* m_dibPixels = nullptr;
    int m_widthPx = 0;
    int m_heightPx = 0;

    void ReleaseDib();
};

}
