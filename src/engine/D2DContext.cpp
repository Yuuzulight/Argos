#include "engine/D2DContext.h"

using namespace argos;

D2DContext::D2DContext() {
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2dFactory.GetAddressOf());
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                         reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
}

D2DContext::~D2DContext() {
    ReleaseDib();
}

void D2DContext::ReleaseDib() {
    if (m_memDC && m_oldBitmap) {
        SelectObject(m_memDC, m_oldBitmap);
        m_oldBitmap = nullptr;
    }
    if (m_dib) {
        DeleteObject(m_dib);
        m_dib = nullptr;
    }
    if (m_memDC) {
        DeleteDC(m_memDC);
        m_memDC = nullptr;
    }
}

bool D2DContext::Resize(int widthPx, int heightPx) {
    if (widthPx <= 0 || heightPx <= 0) {
        m_widthPx = m_heightPx = 0;
        return false;
    }
    if (widthPx == m_widthPx && heightPx == m_heightPx && m_dcRenderTarget) {
        return true;
    }

    ReleaseDib();

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = widthPx;
    bmi.bmiHeader.biHeight = -heightPx; // negative = top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screenDC = GetDC(nullptr);
    m_memDC = CreateCompatibleDC(screenDC);
    m_dib = CreateDIBSection(screenDC, &bmi, DIB_RGB_COLORS, &m_dibPixels, nullptr, 0);
    ReleaseDC(nullptr, screenDC);
    if (!m_memDC || !m_dib) {
        m_widthPx = m_heightPx = 0;
        return false;
    }
    m_oldBitmap = static_cast<HBITMAP>(SelectObject(m_memDC, m_dib));

    if (!m_dcRenderTarget) {
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(
            D2D1_RENDER_TARGET_TYPE_DEFAULT,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        if (FAILED(m_d2dFactory->CreateDCRenderTarget(&props, m_dcRenderTarget.GetAddressOf()))) {
            m_widthPx = m_heightPx = 0;
            return false;
        }
    }

    RECT bindRect{ 0, 0, widthPx, heightPx };
    if (FAILED(m_dcRenderTarget->BindDC(m_memDC, &bindRect))) {
        m_widthPx = m_heightPx = 0;
        return false;
    }

    m_widthPx = widthPx;
    m_heightPx = heightPx;
    return true;
}

void D2DContext::BeginDraw() {
    if (!m_dcRenderTarget) {
        return;
    }
    m_dcRenderTarget->BeginDraw();
    m_dcRenderTarget->Clear(D2D1::ColorF(0, 0.0f));
}

void D2DContext::EndDrawAndPresent(HWND hwnd, int screenX, int screenY) {
    if (!m_dcRenderTarget) {
        return;
    }
    if (FAILED(m_dcRenderTarget->EndDraw())) {
        return;
    }

    POINT ptSrc{ 0, 0 };
    POINT ptDst{ screenX, screenY };
    SIZE size{ m_widthPx, m_heightPx };
    BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

    HDC screenDC = GetDC(nullptr);
    UpdateLayeredWindow(hwnd, screenDC, &ptDst, &size, m_memDC, &ptSrc, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, screenDC);
}

void D2DContext::FillRoundedRect(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color) {
    ComPtr<ID2D1SolidColorBrush> brush;
    m_dcRenderTarget->CreateSolidColorBrush(color, brush.GetAddressOf());
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, radius, radius);
    m_dcRenderTarget->FillRoundedRectangle(rr, brush.Get());
}

void D2DContext::FillBar(const D2D1_RECT_F& bounds, float fraction, const D2D1_COLOR_F& fillColor,
                          const D2D1_COLOR_F& trackColor) {
    FillRoundedRect(bounds, 3.0f, trackColor);
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    D2D1_RECT_F fillRect = bounds;
    fillRect.right = bounds.left + (bounds.right - bounds.left) * fraction;
    FillRoundedRect(fillRect, 3.0f, fillColor);
}

void D2DContext::DrawText(const D2D1_RECT_F& layoutRect, const wchar_t* text, IDWriteTextFormat* format,
                           const D2D1_COLOR_F& color) {
    ComPtr<ID2D1SolidColorBrush> brush;
    m_dcRenderTarget->CreateSolidColorBrush(color, brush.GetAddressOf());
    m_dcRenderTarget->DrawText(text, static_cast<UINT32>(wcslen(text)), format, layoutRect, brush.Get());
}

ComPtr<IDWriteTextFormat> D2DContext::CreateTextFormat(const wchar_t* fontFamily, float sizePt) {
    ComPtr<IDWriteTextFormat> format;
    m_dwriteFactory->CreateTextFormat(fontFamily, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                       DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                       sizePt, L"en-us", format.GetAddressOf());
    return format;
}
