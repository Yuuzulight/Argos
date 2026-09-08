#pragma once
#include <memory>
#include "engine/LayeredWindow.h"
#include "skin/Skin.h"

namespace argos {

// Hosts one loaded skin as its own on-screen widget window. Identical in
// spirit to skin_demo.cpp's single hardcoded widget, but reusable: the
// manager creates and destroys any number of these as skins are
// enabled/disabled or reloaded. Reuses LayeredWindow's own built-in
// drag-to-reposition (design spec section 8's "reposition: dragging the
// widget itself") -- no drag code needed here.
class SkinWidgetWindow : public LayeredWindow {
public:
    void SetSkin(std::unique_ptr<Skin> skin) { m_skin = std::move(skin); }

    void OnPaint(D2DContext& ctx, float, float) override {
        if (m_skin) m_skin->RenderMeters(ctx);
    }
    void OnTimer() override {
        if (m_skin) {
            m_skin->UpdateMeasures();
            Render();
        }
    }
    // Deliberately no OnDestroy() override: LayeredWindow's default is a
    // no-op, which is correct here -- a widget closing (because the
    // manager disabled it) must never quit the whole manager process,
    // only the manager's own window closing does that (see
    // ManagerWindow::OnDestroy).

private:
    std::unique_ptr<Skin> m_skin;
};

}
