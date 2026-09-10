#include "hud/HudPanelComponent.h"

namespace game
{
    void HudPanelComponent::Render(eng::CanvasComponent* canvas)
    {
        if (!m_visible || !canvas || m_color.a <= 0.0f)
        {
            return;
        }
        canvas->DrawRect(m_min, m_max, m_color);
    }
}
