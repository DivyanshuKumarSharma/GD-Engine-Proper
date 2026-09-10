#include "hud/CrosshairComponent.h"

#include <algorithm>

namespace game
{
    void CrosshairComponent::Update(float deltaTime)
    {
        m_hitFlash = std::max(0.0f, m_hitFlash - deltaTime);
    }

    void CrosshairComponent::Render(eng::CanvasComponent* canvas)
    {
        if (!m_visible || !canvas)
        {
            return;
        }

        const auto& viewport = eng::Engine::GetInstance().GetGraphicsAPI().GetViewport();
        const glm::vec2 center(viewport.width * 0.5f, viewport.height * 0.5f);

        // On a hit the cross briefly expands outward and turns to the hit colour -
        // a clean, readable confirmation that works with axis-aligned quads.
        const float t = m_hitFlashDuration > 0.0f ? m_hitFlash / m_hitFlashDuration : 0.0f;
        const bool hit = m_hitFlash > 0.0f;
        const glm::vec4 color = hit ? m_hitColor : m_color;
        const float gap = m_gap + (hit ? t * 7.0f : 0.0f);
        const float length = m_length + (hit ? t * 4.0f : 0.0f);
        const float half = (m_thickness + (hit ? t * 1.5f : 0.0f)) * 0.5f;

        auto bar = [&](const glm::vec2& a, const glm::vec2& b)
        {
            canvas->DrawRect(center + a, center + b, color);
        };

        bar({ -half, gap }, { half, gap + length });            // up
        bar({ -half, -gap - length }, { half, -gap });          // down
        bar({ gap, -half }, { gap + length, half });            // right
        bar({ -gap - length, -half }, { -gap, half });          // left

        // Centre dot when idle.
        if (!hit)
        {
            canvas->DrawRect(center + glm::vec2(-1.0f, -1.0f),
                             center + glm::vec2(1.0f, 1.0f), color);
        }
    }
}
