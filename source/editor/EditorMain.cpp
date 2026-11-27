#include "editor/EditorApp.h"

#include <eng.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

// CourseMaker - the visual authoring tool. Usage:
//   CourseMaker [<project-folder>]
// With no argument it opens the bundled example project (if present) or starts
// with no project.
int main(int argc, char** argv)
{
    auto* app = new editor::EditorApp();

    int headlessFrames = -1;
    std::string shotsDir;
    bool demo = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--frames" && i + 1 < argc)
        {
            headlessFrames = std::atoi(argv[++i]);
        }
        else if (arg == "--shots" && i + 1 < argc)
        {
            shotsDir = argv[++i];
            if (headlessFrames < 0) headlessFrames = 40;
        }
        else if (arg == "--demo")
        {
            demo = true;
            if (headlessFrames < 0) headlessFrames = 1000;
        }
        else if (!arg.empty() && arg[0] != '-')
        {
            std::error_code ec;
            if (std::filesystem::exists(arg, ec))
            {
                app->SetStartupProject(std::filesystem::path(arg));
            }
            else
            {
                std::cerr << "CourseMaker: project folder not found: " << arg << std::endl;
            }
        }
    }
    if (headlessFrames > 0)
    {
        app->SetHeadless(headlessFrames, shotsDir);
    }
    if (demo)
    {
        app->SetDemo(true);
    }

    eng::Engine& engine = eng::Engine::GetInstance();
    engine.SetApplication(app);

    const bool initialized = engine.Init(1600, 900);
    if (initialized)
    {
        engine.Run();
    }
    engine.Destroy();
    return initialized ? 0 : 1;
}
