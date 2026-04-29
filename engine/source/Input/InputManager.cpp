#include "InputManager.h"
#include "Engine.h"

namespace eng
{
    // InputManager implementation can be added here if needed
    void InputManager::setKeyPressed(int key, bool pressed)
    {
        if (key < 0 || key >= static_cast<int>(m_keys.size()))
        {
            return; // Out of bounds, do nothing
        }
        m_keys[key] = pressed;
    }

    bool InputManager::isKeyPressed(int key) const
    {
        if (key < 0 || key >= static_cast<int>(m_keys.size()))
        {
            return false; // Out of bounds, do nothing
        }
        return m_keys[key]; 
    }
}