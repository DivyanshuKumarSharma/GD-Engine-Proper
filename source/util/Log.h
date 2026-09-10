#pragma once

#include <iostream>
#include <string>

namespace game::log
{
    // Lightweight tagged logging for development. Keep these out of per-frame code.
    //   [Drill] Started 'DRILL 01 - TARGET PRACTICE'
    //   [Objective] Shoot Cubes 2/3
    //   [Score] Final score 13080

    inline void Info(const char* category, const std::string& message)
    {
        std::cout << '[' << category << "] " << message << std::endl;
    }

    inline void Warn(const char* category, const std::string& message)
    {
        std::cerr << '[' << category << "] WARNING: " << message << std::endl;
    }
}
