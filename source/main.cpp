#include "Game.h"
#include <eng.h>

#include <cstring>
#include <string>

int main(int argc, char** argv)
{
    bool selfTest = false;
    std::string screenshotDir;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--selftest") == 0)
        {
            selfTest = true;
        }
        else if (std::strcmp(argv[i], "--shots") == 0 && i + 1 < argc)
        {
            screenshotDir = argv[++i];
            selfTest = true; // screenshots are driven by the self-test walking the states
        }
    }

    Game* game = new Game();
    game->SetSelfTest(selfTest);
    if (!screenshotDir.empty())
    {
        game->SetScreenshotDir(screenshotDir);
    }

    eng::Engine& engine = eng::Engine::GetInstance();
    engine.SetApplication(game);

    if (engine.Init(1280, 720))
    {
        engine.Run();
    }

    engine.Destroy();
    return 0;
}
