#pragma once

namespace game
{
    // High-level flow of a course run. Owned and advanced by CourseDirector.
    //
    //   MainMenu --Play--> DrillIntro --> Countdown --GO--> Playing
    //   Playing --all objectives done--> DrillComplete --> (next) DrillIntro ...
    //   last DrillComplete --> RunComplete --Retry--> DrillIntro(0)
    //   RunComplete --Menu--> MainMenu
    //   Playing <--Esc--> Paused
    enum class GameState
    {
        MainMenu,
        DrillIntro,     // drill title card; content is being spawned
        Countdown,      // 3..2..1..GO; look allowed, firing and timers frozen
        Playing,        // objectives tracked, timers running
        DrillComplete,  // per-drill results card
        RunComplete,    // final results screen
        Paused
    };

    inline const char* ToString(GameState state)
    {
        switch (state)
        {
        case GameState::MainMenu:      return "MainMenu";
        case GameState::DrillIntro:    return "DrillIntro";
        case GameState::Countdown:     return "Countdown";
        case GameState::Playing:       return "Playing";
        case GameState::DrillComplete: return "DrillComplete";
        case GameState::RunComplete:   return "RunComplete";
        case GameState::Paused:        return "Paused";
        }
        return "Unknown";
    }
}
