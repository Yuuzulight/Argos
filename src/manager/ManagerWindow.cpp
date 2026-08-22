#include "manager/ManagerWindow.h"

using namespace argos;

namespace {
constexpr float kRefreshButtonX = 12.0f;
constexpr float kRefreshButtonY = 12.0f;
constexpr float kRefreshButtonW = 88.0f;
constexpr float kRefreshButtonH = 28.0f;
constexpr float kListTop = 52.0f;
constexpr float kRowHeight = 36.0f;
constexpr float kRowLeft = 12.0f;
constexpr float kRowWidth = 296.0f;
constexpr float kToggleSize = 16.0f;
}

void ManagerWindow::SetEntries(std::vector<SkinEntry> entries) {
    m_entries = std::move(entries);
    Render();
}

void ManagerWindow::OnPaint(D2DContext& ctx, float dipWidth, float dipHeight) {
    auto format = ctx.CreateTextFormat(L"Segoe UI", 14.0f);

    D2D1_RECT_F refreshRect{ kRefreshButtonX, kRefreshButtonY,
                             kRefreshButtonX + kRefreshButtonW, kRefreshButtonY + kRefreshButtonH };
    ctx.FillRoundedRect(refreshRect, 4.0f, D2D1::ColorF(0.30f, 0.30f, 0.32f));
    if (format) {
        ctx.DrawText(refreshRect, L"Refresh", format.Get(), D2D1::ColorF(D2D1::ColorF::White));
    }

    for (size_t i = 0; i < m_entries.size(); ++i) {
        const SkinEntry& entry = m_entries[i];
        float rowTop = kListTop + static_cast<float>(i) * kRowHeight;
        if (rowTop + kRowHeight > dipHeight) break; // v1 has no scrolling -- see class comment

        D2D1_RECT_F toggleRect{ kRowLeft, rowTop + (kRowHeight - kToggleSize) / 2.0f,
                                 kRowLeft + kToggleSize, rowTop + (kRowHeight - kToggleSize) / 2.0f + kToggleSize };
        D2D1_COLOR_F toggleColor = entry.loadFailed ? D2D1::ColorF(0.80f, 0.25f, 0.25f)
                                  : entry.enabled   ? D2D1::ColorF(0.35f, 0.70f, 0.40f)
                                                     : D2D1::ColorF(0.45f, 0.45f, 0.47f);
        ctx.FillRoundedRect(toggleRect, 3.0f, toggleColor);

        std::wstring label = entry.name;
        if (entry.loadFailed) {
            label += L" (failed to load)";
        } else if (entry.enabled) {
            label += L" (enabled)";
        }
        if (format) {
            D2D1_RECT_F labelRect{ kRowLeft + kToggleSize + 10.0f, rowTop,
                                    kRowLeft + kRowWidth, rowTop + kRowHeight };
            ctx.DrawText(labelRect, label.c_str(), format.Get(), D2D1::ColorF(D2D1::ColorF::Black));
        }
    }
}

void ManagerWindow::OnLButtonUp(float dipX, float dipY) {
    if (dipX >= kRefreshButtonX && dipX <= kRefreshButtonX + kRefreshButtonW &&
        dipY >= kRefreshButtonY && dipY <= kRefreshButtonY + kRefreshButtonH) {
        if (onRefreshRequested) onRefreshRequested();
        return;
    }
    for (size_t i = 0; i < m_entries.size(); ++i) {
        float rowTop = kListTop + static_cast<float>(i) * kRowHeight;
        if (dipY >= rowTop && dipY < rowTop + kRowHeight &&
            dipX >= kRowLeft && dipX <= kRowLeft + kRowWidth) {
            if (onToggleRequested) onToggleRequested(i);
            return;
        }
    }
}

void ManagerWindow::OnDestroy() {
    PostQuitMessage(0);
}
