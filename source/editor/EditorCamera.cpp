#include "editor/EditorCamera.h"

#include "imgui.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/trigonometric.hpp>

#include <algorithm>
#include <cmath>

namespace editor
{
    EditorCamera::EditorCamera()
    {
        Reset();
    }

    void EditorCamera::Reset()
    {
        m_position = glm::vec3(6.0f, 5.0f, 14.0f);
        m_yaw = -25.0f;
        m_pitch = -18.0f;
    }

    void EditorCamera::SetPose(const glm::vec3& position, float yawDeg, float pitchDeg)
    {
        m_position = position;
        m_yaw = yawDeg;
        m_pitch = std::clamp(pitchDeg, -89.0f, 89.0f);
    }

    glm::vec3 EditorCamera::Forward() const
    {
        const float cy = std::cos(glm::radians(m_yaw));
        const float sy = std::sin(glm::radians(m_yaw));
        const float cp = std::cos(glm::radians(m_pitch));
        const float sp = std::sin(glm::radians(m_pitch));
        return glm::normalize(glm::vec3(sy * cp, sp, -cy * cp));
    }

    void EditorCamera::Focus(const glm::vec3& target, float distance)
    {
        m_position = target - Forward() * distance;
    }

    void EditorCamera::Update(float deltaSeconds, bool interacting)
    {
        if (!interacting)
        {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        const glm::vec3 forward = Forward();
        const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        const glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

        // Wheel: dolly along the view direction (also nudges the fly speed).
        if (std::fabs(io.MouseWheel) > 0.0f && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            m_position += forward * io.MouseWheel * (moveSpeed * 0.15f);
        }

        const bool looking = ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f);
        const bool panning = ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f);

        if (looking)
        {
            const float sens = 0.15f;
            m_yaw += io.MouseDelta.x * sens;
            m_pitch = std::clamp(m_pitch - io.MouseDelta.y * sens, -89.0f, 89.0f);

            float speed = moveSpeed;
            if (io.KeyShift) speed *= 4.0f;
            if (io.KeyCtrl) speed *= 0.25f;

            glm::vec3 move(0.0f);
            if (ImGui::IsKeyDown(ImGuiKey_W)) move += forward;
            if (ImGui::IsKeyDown(ImGuiKey_S)) move -= forward;
            if (ImGui::IsKeyDown(ImGuiKey_D)) move += right;
            if (ImGui::IsKeyDown(ImGuiKey_A)) move -= right;
            if (ImGui::IsKeyDown(ImGuiKey_E)) move += up;
            if (ImGui::IsKeyDown(ImGuiKey_Q)) move -= up;
            if (glm::dot(move, move) > 0.0f)
            {
                m_position += glm::normalize(move) * speed * deltaSeconds;
            }
        }
        else if (panning)
        {
            const float scale = 0.02f;
            m_position += (-right * io.MouseDelta.x + up * io.MouseDelta.y) * scale * std::max(moveSpeed, 4.0f);
        }
    }

    const eng::CameraData& EditorCamera::BuildCameraData(float aspect)
    {
        const glm::vec3 forward = Forward();
        m_cameraData.viewMatrix = glm::lookAt(m_position, m_position + forward, glm::vec3(0.0f, 1.0f, 0.0f));
        m_cameraData.projectionMatrix = glm::perspective(glm::radians(fov),
            aspect > 0.0001f ? aspect : 1.0f, nearPlane, farPlane);
        m_cameraData.position = m_position;
        return m_cameraData;
    }

    void EditorCamera::Ray(const glm::vec2& ndc, float aspect, glm::vec3& outOrigin, glm::vec3& outDir) const
    {
        const glm::mat4 proj = glm::perspective(glm::radians(fov),
            aspect > 0.0001f ? aspect : 1.0f, nearPlane, farPlane);
        const glm::mat4 view = glm::lookAt(m_position, m_position + Forward(), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 invVP = glm::inverse(proj * view);

        glm::vec4 nearPoint = invVP * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
        glm::vec4 farPoint = invVP * glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);
        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;

        outOrigin = glm::vec3(nearPoint);
        outDir = glm::normalize(glm::vec3(farPoint - nearPoint));
    }
}
