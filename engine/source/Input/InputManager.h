#pragma once
#include <array>
#include <glm/vec2.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace eng
{
    class InputManager
    {
    public:
        enum class BindingType
        {
            Keyboard,
            MouseButton,
            MouseWheel   // code +1 = wheel up, -1 = wheel down; "pressed" for one frame per notch
        };

        struct ActionBinding
        {
            BindingType type = BindingType::Keyboard;
            int code = -1;
        };

    private:
        InputManager() = default;
        InputManager(const InputManager&) = delete;
        InputManager(InputManager&&) = delete;
        InputManager& operator=(const InputManager&) = delete;
        InputManager& operator=(InputManager&&) = delete;

    public:
        void SetKeyPressed(int key, bool pressed);
        bool IsKeyPressed(int key) const;
        bool WasKeyPressed(int key) const;
        bool WasKeyReleased(int key) const;

        void SetMouseButtonPressed(int button, bool pressed);
        bool IsMouseButtonPressed(int button) const;

        void SetMouseButtonWasPressed(int button, bool pressed);
        bool WasMouseButtonPressed(int button) const;

        void SetMouseButtonWasReleased(int button, bool pressed);
        bool WasMouseButtonReleased(int button) const;

        void SetMousePositionOld(const glm::vec2& pos);
        const glm::vec2& GetMousePositionOld() const;

        void SetMousePositionCurrent(const glm::vec2& pos);
        const glm::vec2& GetMousePositionCurrent() const;

        void SetMousePositionChanged(bool changed);
        bool IsMousePositionChanged() const;

        // Accumulates scroll this frame (positive = up). Cleared by ClearStates.
        void AddScroll(float delta);
        float GetScrollDelta() const;

        // While suppressed, every query below reports "not pressed" and the mouse
        // is reported as not moved. The editor sets this so panel interaction and
        // the free-fly camera never reach gameplay input.
        void SetSuppressed(bool suppressed) { m_suppressed = suppressed; }
        bool IsSuppressed() const { return m_suppressed; }

        bool LoadActionMap(const std::string& jsonText, std::string* errorMessage = nullptr);
        void ClearActionMap();
        bool HasAction(const std::string& action) const;
        bool IsActionDown(const std::string& action) const;
        bool WasActionPressed(const std::string& action) const;
        bool WasActionReleased(const std::string& action) const;

        void ClearStates();

    private:
        bool WheelNotch(int upCode) const;

        std::array<bool, 512> m_keys = { false };
        std::array<bool, 512> m_keyPressed = { false };
        std::array<bool, 512> m_keyReleased = { false };
        std::array<bool, 16> m_mouseKeys = { false };
        std::array<bool, 16> m_mouseKeyPressed = { false };
        std::array<bool, 16> m_mouseKeyReleased = { false };
        std::unordered_map<std::string, std::vector<ActionBinding>> m_actions;
        bool m_suppressed = false;
        glm::vec2 m_mousePositionOld = glm::vec2(0.0f);
        glm::vec2 m_mousePositionCurrent = glm::vec2(0.0f);
        bool m_mousePositionChanged = false;
        float m_scrollDelta = 0.0f;

        friend class Engine;
    };
}
