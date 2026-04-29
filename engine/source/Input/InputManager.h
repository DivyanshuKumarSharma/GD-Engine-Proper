#pragma once
#include <array>

namespace eng{
    class InputManager
    {
    private:
        // Private members for input management
        InputManager() = default; // Private constructor for singleton pattern
        InputManager(const InputManager&) = delete; // Delete copy constructor
        InputManager(InputManager&&) = delete; // Delete move constructor
        InputManager& operator=(const InputManager&) = delete; // Delete copy assignment operator
        InputManager& operator=(InputManager&&) = delete; // Delete move assignment operator

    public:
        void setKeyPressed(int key, bool pressed);

        bool isKeyPressed(int key) const;

         static InputManager& getInstance()
        {
            static InputManager instance; // Guaranteed to be destroyed and instantiated on first use
            return instance;
        }

    private:
        std::array<bool, 1024> m_keys = {false}; // Array to track key states
        friend class Engine; // Allow Engine to access private members of InputManager
    };
}