#pragma once

#include <eng.h>

#include <glm/vec4.hpp>

#include <algorithm>

namespace game
{
    // A minimal FPS crosshair drawn straight into the HUD canvas: a centre gap
    // cross that briefly turns into an X "hit marker" (and changes colour) when
    // FlashHit() is called. Add it to a child object of the HUD canvas.
    class CrosshairComponent : public eng::UIElementComponent
    {
        COMPONENT_2(CrosshairComponent, eng::UIElementComponent)
    public:
        void Update(float deltaTime) override;
        void Render(eng::CanvasComponent* canvas) override;

        void SetVisible(bool visible) { m_visible = visible; }
        void FlashHit() { m_hitFlash = m_hitFlashDuration; }
        // Extra gap (pixels) on top of the resting gap: the HUD feeds the active
        // weapon's live spread in, so the cross honestly shows the cone.
        void SetSpreadGap(float pixels) { m_spreadGap = std::max(0.0f, pixels); }

    private:
        bool m_visible = true;
        float m_hitFlash = 0.0f;
        float m_hitFlashDuration = 0.16f;

        glm::vec4 m_color = glm::vec4(1.0f, 1.0f, 1.0f, 0.85f);
        glm::vec4 m_hitColor = glm::vec4(1.0f, 0.32f, 0.2f, 1.0f);
        float m_thickness = 2.0f;
        float m_length = 10.0f;
        float m_gap = 5.0f;
        float m_spreadGap = 0.0f;
    };
}
