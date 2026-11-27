#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <string>

namespace eng
{
    class Scene;
    class GameObject;
    class CanvasComponent;
    class TextComponent;
}

namespace game
{
    class CourseDirector;
    class CrosshairComponent;
    class HudPanelComponent;

    // The training HUD: a dedicated canvas built in code, refreshed once per frame
    // from the CourseDirector's public accessors. Pure presentation - it reads
    // course state, it never changes it.
    class HudView
    {
    public:
        void Init(eng::Scene* scene);
        void Refresh(const CourseDirector& director);
        void FlashHitMarker();

    private:
        eng::TextComponent* MakeText(const char* name, int fontSize,
            const glm::vec2& pivot, const glm::vec4& color);
        HudPanelComponent* MakePanel(const char* name);
        void SetText(eng::TextComponent* text, const std::string& value,
            const glm::vec2& screenPos);
        void Clear(eng::TextComponent* text);
        void RefreshCenter(const CourseDirector& director, float width, float height);
        void SetVisible(bool visible);

        eng::Scene* m_scene = nullptr;
        eng::GameObject* m_canvasObject = nullptr;
        eng::CanvasComponent* m_canvas = nullptr;
        CrosshairComponent* m_crosshair = nullptr;

        HudPanelComponent* m_panelTopLeft = nullptr;
        HudPanelComponent* m_panelTopCenter = nullptr;
        HudPanelComponent* m_panelCenter = nullptr;

        eng::TextComponent* m_drillTitle = nullptr;
        eng::TextComponent* m_objectiveHeader = nullptr;
        std::array<eng::TextComponent*, 6> m_objectiveLines{};
        eng::TextComponent* m_timerLabel = nullptr;
        eng::TextComponent* m_timerValue = nullptr;
        eng::TextComponent* m_statsLine = nullptr;

        eng::TextComponent* m_centerBig = nullptr;
        std::array<eng::TextComponent*, 5> m_centerLines{};
        eng::TextComponent* m_hintLine = nullptr;

        // bottom-left weapon block (loadout mode only)
        HudPanelComponent* m_panelWeapon = nullptr;
        eng::TextComponent* m_weaponName = nullptr;
        eng::TextComponent* m_weaponAmmo = nullptr;
        eng::TextComponent* m_weaponSlots = nullptr;

        void RefreshWeapon(const CourseDirector& director, bool show, float width, float height);
    };
}
