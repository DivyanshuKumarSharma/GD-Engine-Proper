#pragma once

#include "Common.h"          // eng::CameraData

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace editor
{
    // Free-fly viewport camera. Driven entirely from ImGui input so it can never
    // reach gameplay code: hold the right mouse button to look + WASDQE to move,
    // middle-drag to pan, wheel to dolly. Shift = faster, Ctrl = slower.
    class EditorCamera
    {
    public:
        EditorCamera();

        // `interacting` gates all input (true only when the viewport is hovered
        // or the user is mid-drag started in it).
        void Update(float deltaSeconds, bool interacting);

        void Reset();
        void Focus(const glm::vec3& target, float distance = 6.0f);
        void SetPose(const glm::vec3& position, float yawDeg, float pitchDeg);

        const eng::CameraData& BuildCameraData(float aspect);

        glm::vec3 Position() const { return m_position; }
        glm::vec3 Forward() const;

        // World-space ray through a viewport point given in normalised device
        // coords (x,y in [-1,1], y up). Used for picking and first-person placement.
        void Ray(const glm::vec2& ndc, float aspect, glm::vec3& outOrigin, glm::vec3& outDir) const;

        float fov = 60.0f;
        float nearPlane = 0.05f;
        float farPlane = 3000.0f;
        float moveSpeed = 12.0f;   // m/s baseline

    private:
        glm::vec3 m_position;
        float m_yaw = 0.0f;    // degrees, around world +Y
        float m_pitch = 0.0f;  // degrees
        eng::CameraData m_cameraData;
    };
}
