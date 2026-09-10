#include "input/InputManager.h"

#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <exception>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace
{
    std::string NormalizeName(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char character) { return static_cast<char>(std::toupper(character)); });
        return value;
    }

    int KeyCodeFromName(const std::string& keyName)
    {
        const std::string name = NormalizeName(keyName);

        if (name.size() == 1 && name[0] >= 'A' && name[0] <= 'Z')
        {
            return GLFW_KEY_A + (name[0] - 'A');
        }
        if (name.size() == 1 && name[0] >= '0' && name[0] <= '9')
        {
            return GLFW_KEY_0 + (name[0] - '0');
        }

        static const std::unordered_map<std::string, int> keys = {
            { "SPACE", GLFW_KEY_SPACE },
            { "ESCAPE", GLFW_KEY_ESCAPE },
            { "ENTER", GLFW_KEY_ENTER },
            { "TAB", GLFW_KEY_TAB },
            { "BACKSPACE", GLFW_KEY_BACKSPACE },
            { "LEFT", GLFW_KEY_LEFT },
            { "RIGHT", GLFW_KEY_RIGHT },
            { "UP", GLFW_KEY_UP },
            { "DOWN", GLFW_KEY_DOWN },
            { "LEFT_SHIFT", GLFW_KEY_LEFT_SHIFT },
            { "RIGHT_SHIFT", GLFW_KEY_RIGHT_SHIFT },
            { "LEFT_CONTROL", GLFW_KEY_LEFT_CONTROL },
            { "RIGHT_CONTROL", GLFW_KEY_RIGHT_CONTROL },
            { "LEFT_ALT", GLFW_KEY_LEFT_ALT },
            { "RIGHT_ALT", GLFW_KEY_RIGHT_ALT }
        };

        const auto iterator = keys.find(name);
        return iterator == keys.end() ? -1 : iterator->second;
    }

    int MouseButtonCodeFromName(const std::string& buttonName)
    {
        static const std::unordered_map<std::string, int> buttons = {
            { "LEFT", GLFW_MOUSE_BUTTON_LEFT },
            { "RIGHT", GLFW_MOUSE_BUTTON_RIGHT },
            { "MIDDLE", GLFW_MOUSE_BUTTON_MIDDLE },
            { "BUTTON_4", GLFW_MOUSE_BUTTON_4 },
            { "BUTTON_5", GLFW_MOUSE_BUTTON_5 },
            { "BUTTON_6", GLFW_MOUSE_BUTTON_6 },
            { "BUTTON_7", GLFW_MOUSE_BUTTON_7 },
            { "BUTTON_8", GLFW_MOUSE_BUTTON_8 }
        };

        const auto iterator = buttons.find(NormalizeName(buttonName));
        return iterator == buttons.end() ? -1 : iterator->second;
    }
}

namespace eng
{
    void InputManager::SetKeyPressed(int key, bool pressed)
    {
        if (key < 0 || key >= static_cast<int>(m_keys.size()))
        {
            return;
        }

        if (m_keys[key] == pressed)
        {
            return;
        }

        m_keys[key] = pressed;
        m_keyPressed[key] = pressed;
        m_keyReleased[key] = !pressed;
    }

    bool InputManager::IsKeyPressed(int key) const
    {
        if (m_suppressed || key < 0 || key >= static_cast<int>(m_keys.size()))
        {
            return false;
        }

        return m_keys[key];
    }

    bool InputManager::WasKeyPressed(int key) const
    {
        if (m_suppressed || key < 0 || key >= static_cast<int>(m_keyPressed.size()))
        {
            return false;
        }
        return m_keyPressed[key];
    }

    bool InputManager::WasKeyReleased(int key) const
    {
        if (m_suppressed || key < 0 || key >= static_cast<int>(m_keyReleased.size()))
        {
            return false;
        }
        return m_keyReleased[key];
    }

    void InputManager::SetMouseButtonPressed(int button, bool pressed)
    {
        if (button < 0 || button >= static_cast<int>(m_mouseKeys.size()))
        {
            return;
        }

        if (m_mouseKeys[button] == pressed)
        {
            return;
        }

        m_mouseKeys[button] = pressed;
        m_mouseKeyPressed[button] = pressed;
        m_mouseKeyReleased[button] = !pressed;
    }

    bool InputManager::IsMouseButtonPressed(int button) const
    {
        if (m_suppressed || button < 0 || button >= static_cast<int>(m_mouseKeys.size()))
        {
            return false;
        }
        return m_mouseKeys[button];
    }

    void InputManager::SetMouseButtonWasPressed(int button, bool pressed)
    {
        if (button < 0 || button >= static_cast<int>(m_mouseKeyPressed.size()))
        {
            return;
        }
        m_mouseKeyPressed[button] = pressed;
    }

    bool InputManager::WasMouseButtonPressed(int button) const
    {
        if (m_suppressed || button < 0 || button >= static_cast<int>(m_mouseKeyPressed.size()))
        {
            return false;
        }
        return m_mouseKeyPressed[button];
    }

    void InputManager::SetMouseButtonWasReleased(int button, bool pressed)
    {
        if (button < 0 || button >= static_cast<int>(m_mouseKeyReleased.size()))
        {
            return;
        }
        m_mouseKeyReleased[button] = pressed;
    }

    bool InputManager::WasMouseButtonReleased(int button) const
    {
        if (m_suppressed || button < 0 || button >= static_cast<int>(m_mouseKeyReleased.size()))
        {
            return false;
        }
        return m_mouseKeyReleased[button];
    }

    void InputManager::SetMousePositionOld(const glm::vec2& pos)
    {
        m_mousePositionOld = pos;
    }

    const glm::vec2& InputManager::GetMousePositionOld() const
    {
        return m_mousePositionOld;
    }

    void InputManager::SetMousePositionCurrent(const glm::vec2& pos)
    {
        m_mousePositionCurrent = pos;
    }

    const glm::vec2& InputManager::GetMousePositionCurrent() const
    {
        return m_mousePositionCurrent;
    }

    void InputManager::SetMousePositionChanged(bool changed)
    {
        m_mousePositionChanged = changed;
    }

    bool InputManager::IsMousePositionChanged() const
    {
        return !m_suppressed && m_mousePositionChanged;
    }

    bool InputManager::LoadActionMap(const std::string& jsonText, std::string* errorMessage)
    {
        try
        {
            const auto root = nlohmann::json::parse(jsonText);
            if (!root.contains("actions") || !root["actions"].is_object())
            {
                throw std::runtime_error("Input map must contain an 'actions' object.");
            }

            std::unordered_map<std::string, std::vector<ActionBinding>> loadedActions;
            for (const auto& actionEntry : root["actions"].items())
            {
                if (!actionEntry.value().is_array() || actionEntry.value().empty())
                {
                    throw std::runtime_error("Action '" + actionEntry.key() + "' must have at least one binding.");
                }

                auto& bindings = loadedActions[actionEntry.key()];
                for (const auto& bindingJson : actionEntry.value())
                {
                    const std::string device = NormalizeName(bindingJson.value("device", ""));
                    const std::string codeName = bindingJson.value("code", "");

                    ActionBinding binding;
                    if (device == "KEYBOARD")
                    {
                        binding.type = BindingType::Keyboard;
                        binding.code = KeyCodeFromName(codeName);
                    }
                    else if (device == "MOUSE")
                    {
                        binding.type = BindingType::MouseButton;
                        binding.code = MouseButtonCodeFromName(codeName);
                    }
                    else
                    {
                        throw std::runtime_error("Action '" + actionEntry.key() + "' uses unknown device '" + device + "'.");
                    }

                    if (binding.code < 0)
                    {
                        throw std::runtime_error("Action '" + actionEntry.key() + "' uses unknown code '" + codeName + "'.");
                    }
                    bindings.push_back(binding);
                }
            }

            m_actions = std::move(loadedActions);
            return true;
        }
        catch (const std::exception& exception)
        {
            if (errorMessage)
            {
                *errorMessage = exception.what();
            }
            return false;
        }
    }

    void InputManager::ClearActionMap()
    {
        m_actions.clear();
    }

    bool InputManager::HasAction(const std::string& action) const
    {
        return m_actions.find(action) != m_actions.end();
    }

    bool InputManager::IsActionDown(const std::string& action) const
    {
        const auto iterator = m_actions.find(action);
        if (iterator == m_actions.end())
        {
            return false;
        }

        return std::any_of(iterator->second.begin(), iterator->second.end(),
            [this](const ActionBinding& binding)
            {
                return binding.type == BindingType::Keyboard
                    ? IsKeyPressed(binding.code)
                    : IsMouseButtonPressed(binding.code);
            });
    }

    bool InputManager::WasActionPressed(const std::string& action) const
    {
        const auto iterator = m_actions.find(action);
        if (iterator == m_actions.end())
        {
            return false;
        }

        return std::any_of(iterator->second.begin(), iterator->second.end(),
            [this](const ActionBinding& binding)
            {
                return binding.type == BindingType::Keyboard
                    ? WasKeyPressed(binding.code)
                    : WasMouseButtonPressed(binding.code);
            });
    }

    bool InputManager::WasActionReleased(const std::string& action) const
    {
        const auto iterator = m_actions.find(action);
        if (iterator == m_actions.end())
        {
            return false;
        }

        return std::any_of(iterator->second.begin(), iterator->second.end(),
            [this](const ActionBinding& binding)
            {
                return binding.type == BindingType::Keyboard
                    ? WasKeyReleased(binding.code)
                    : WasMouseButtonReleased(binding.code);
            });
    }

    void InputManager::ClearStates()
    {
        SetMousePositionChanged(false);
        m_keyPressed.fill(false);
        m_keyReleased.fill(false);
        m_mouseKeyPressed.fill(false);
        m_mouseKeyReleased.fill(false);
    }
}
