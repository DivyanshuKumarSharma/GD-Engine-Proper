#pragma once

#include <eng.h>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace game
{
    // A single flat rectangle drawn into the HUD canvas, positioned in screen
    // pixels each frame by HudView. Used to darken the world behind the
    // countdown / drill-complete / results text so it stays readable.
    class HudPanelComponent : public eng::UIElementComponent
    {
        COMPONENT_2(HudPanelComponent, eng::UIElementComponent)
    public:
        void Render(eng::CanvasComponent* canvas) override;

        void SetRect(const glm::vec2& min, const glm::vec2& max) { m_min = min; m_max = max; }
        void SetColor(const glm::vec4& color) { m_color = color; }
        void SetVisible(bool visible) { m_visible = visible; }

    private:
        glm::vec2 m_min = glm::vec2(0.0f);
        glm::vec2 m_max = glm::vec2(0.0f);
        glm::vec4 m_color = glm::vec4(0.0f, 0.0f, 0.0f, 0.5f);
        bool m_visible = false;
    };
}
