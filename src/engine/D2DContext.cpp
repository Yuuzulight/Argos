#include "engine/D2DContext.h"

using namespace argos;

D2DContext::D2DContext() {
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, m_d2dFactory.GetAddressOf()))) {
        m_d2dFactory.Reset();
    }
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                    reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf())))) {
        m_dwriteFactory.Reset();
    }
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

bool D2DContext::Resize(int widthPx, int heightPx, UINT dpi) {
    // Downgrade to not-ready up front. Every early-return failure path
    // below leaves this false, so a failed Resize() can never leave
    // IsReady() reporting true over a render target bound to resources
    // that this call may have already torn down.
    m_ready = false;

    if (!m_d2dFactory || !m_dwriteFactory) {
        // Constructor failed to create the factories; nothing usable to
        // resize/bind.
        return false;
    }
    if (widthPx <= 0 || heightPx <= 0) {
        return false;
    }
    if (widthPx == m_widthPx && heightPx == m_heightPx && m_dcRenderTarget) {
        // Pixel size unchanged, but DPI may not be -- keep the render
        // target's DPI in sync regardless.
        m_dcRenderTarget->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
        m_dpi = dpi;
        m_ready = true;
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

    m_dcRenderTarget->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
    m_dpi = dpi;
    m_widthPx = widthPx;
    m_heightPx = heightPx;
    m_ready = true;
    return true;
}

ID2D1RenderTarget* D2DContext::Target() const {
    if (m_hwndRenderTarget) return m_hwndRenderTarget.Get();
    return m_dcRenderTarget.Get();
}

void D2DContext::BeginDraw(const D2D1_COLOR_F& clearColor) {
    if (!IsReady()) {
        return;
    }
    Target()->BeginDraw();
    Target()->Clear(clearColor);
}

void D2DContext::EndDrawAndPresent(HWND hwnd, int screenX, int screenY) {
    if (!IsReady()) {
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

bool D2DContext::CreateForHwnd(HWND hwnd, int widthPx, int heightPx, UINT dpi) {
    m_ready = false;
    if (!m_d2dFactory || !m_dwriteFactory) {
        return false;
    }
    if (widthPx <= 0 || heightPx <= 0) {
        return false;
    }

    if (!m_hwndRenderTarget) {
        D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties();
        D2D1_HWND_RENDER_TARGET_PROPERTIES hwndProps = D2D1::HwndRenderTargetProperties(
            hwnd, D2D1::SizeU(static_cast<UINT32>(widthPx), static_cast<UINT32>(heightPx)));
        if (FAILED(m_d2dFactory->CreateHwndRenderTarget(props, hwndProps, m_hwndRenderTarget.GetAddressOf()))) {
            return false;
        }
    } else if (FAILED(m_hwndRenderTarget->Resize(
                   D2D1::SizeU(static_cast<UINT32>(widthPx), static_cast<UINT32>(heightPx))))) {
        return false;
    }

    m_hwndRenderTarget->SetDpi(static_cast<float>(dpi), static_cast<float>(dpi));
    m_widthPx = widthPx;
    m_heightPx = heightPx;
    m_dpi = dpi;
    m_ready = true;
    return true;
}

void D2DContext::EndDraw() {
    if (!IsReady() || !m_hwndRenderTarget) {
        return;
    }
    // D2DERR_RECREATE_TARGET means the underlying device was lost (driver
    // reset, etc.) -- the documented recovery is to drop the render target
    // so the next CreateForHwnd() call rebuilds it from scratch.
    if (m_hwndRenderTarget->EndDraw() == D2DERR_RECREATE_TARGET) {
        m_hwndRenderTarget.Reset();
        m_ready = false;
    }
}

void D2DContext::FillRoundedRect(const D2D1_RECT_F& rect, float radius, const D2D1_COLOR_F& color) {
    ComPtr<ID2D1SolidColorBrush> brush;
    Target()->CreateSolidColorBrush(color, brush.GetAddressOf());
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(rect, radius, radius);
    Target()->FillRoundedRectangle(rr, brush.Get());
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
    Target()->CreateSolidColorBrush(color, brush.GetAddressOf());
    Target()->DrawText(text, static_cast<UINT32>(wcslen(text)), format, layoutRect, brush.Get());
}

ComPtr<IDWriteTextFormat> D2DContext::CreateTextFormat(const wchar_t* fontFamily, float sizePt) {
    ComPtr<IDWriteTextFormat> format;
    if (!m_dwriteFactory) {
        return format;
    }
    if (FAILED(m_dwriteFactory->CreateTextFormat(fontFamily, nullptr, DWRITE_FONT_WEIGHT_NORMAL,
                                                  DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                                  sizePt, L"en-us", format.GetAddressOf()))) {
        format.Reset();
    }
    return format;
}
