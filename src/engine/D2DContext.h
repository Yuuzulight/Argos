#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

namespace argos {

using Microsoft::WRL::ComPtr;

// Owns this window's Direct2D factory and rendering resources, and a
// reference to the process-shared DirectWrite factory -- D2D1CreateFactory
// creates a fresh factory per D2DContext instance (one per LayeredWindow),
// while DWriteCreateFactory(..._SHARED...) really is one process-wide
// instance. Also owns one window's DC render target + backing 32bpp DIB
// section used to composite through UpdateLayeredWindow (the documented
// interop path for per-pixel-alpha layered windows: draw with Direct2D
// onto a GDI-compatible DC, then hand that DC to UpdateLayeredWindow).
class D2DContext {
public:
    D2DContext();
    ~D2DContext();

    D2DContext(const D2DContext&) = delete;
    D2DContext& operator=(const D2DContext&) = delete;

    // Creates/resizes the backing DIB + DC render target for the given
    // pixel size, and sets the render target's DPI so Direct2D's
    // DIP-to-pixel scale matches the window's actual DPI. Safe to call
    // again (e.g. on a DPI change). Returns false, and leaves the context
    // not-ready (see IsReady()), on any failure.
    bool Resize(int widthPx, int heightPx, UINT dpi);

    // True only if the most recent Resize() call fully succeeded. Starts
    // false, and goes false again if a later Resize() call fails -- even
    // though m_dcRenderTarget may still be a non-null pointer left over
    // from an earlier successful call, since a failed Resize() can have
    // already torn down the backing DIB that pointer was bound to.
    bool IsReady() const { return m_ready; }

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
    UINT m_dpi = 96;
    bool m_ready = false;

    void ReleaseDib();
};

}
