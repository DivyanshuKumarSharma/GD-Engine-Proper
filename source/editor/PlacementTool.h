#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>

namespace eng
{
    class Scene;
}

namespace editor
{
    class EditorCamera;

    // First-person crosshair placement. Distinct from firing and from scored
    // playtesting: it only ever moves a preview "ghost" object and, on confirm,
    // hands back a doc + transform for the caller to wrap in an undoable command.
    class PlacementTool
    {
    public:
        struct Frame
        {
            glm::vec2 ndcCursor = glm::vec2(0.0f); // viewport cursor, NDC, y up
            float aspect = 1.0f;
            bool confirm = false;                  // left click in viewport
            bool cancel = false;                   // Esc / right click
            float wheel = 0.0f;                    // adjusts distance
            bool rotateStep = false;               // tap to yaw the ghost 15 deg
            bool gridSnap = false;
            float gridSize = 0.5f;
            bool alignToSurface = false;
        };

        struct Outcome
        {
            enum class Kind { None, Confirmed, Cancelled };
            Kind kind = Kind::None;
            nlohmann::json doc;      // final doc, transform baked in
            uint64_t ghostId = 0;    // caller must destroy this before creating the real one
        };

        bool Active() const { return m_active; }
        const std::string& Label() const { return m_label; }
        float Distance() const { return m_distance; }
        bool HasSurface() const { return m_hasSurface; }

        // `doc` is a scene-schema object (from PrimitiveFactory or the asset browser).
        void Begin(eng::Scene* scene, nlohmann::json doc, std::string label);
        // Update the ghost. Returns Confirmed/Cancelled once, then goes inactive.
        Outcome Update(eng::Scene* scene, EditorCamera& camera, const Frame& frame);
        void Abort(eng::Scene* scene);

    private:
        bool m_active = false;
        std::string m_label;
        nlohmann::json m_doc;
        uint64_t m_ghostId = 0;
        float m_distance = 8.0f;
        float m_yawDegrees = 0.0f;
        bool m_hasSurface = false;
        glm::vec3 m_lastPosition = glm::vec3(0.0f);
    };
}
