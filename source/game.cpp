#include "game.h"

#include "Bullet.h"
#include "JumpPlatform.h"
#include "Player.h"

#include "course/CourseDirector.h"
#include "project/CourseProject.h"
#include "project/PlaySetup.h"
#include "hud/CrosshairComponent.h"
#include "hud/HudPanelComponent.h"
#include "selftest/SelfTest.h"
#include "targets/KnockTargetComponent.h"
#include "targets/MovingTargetComponent.h"
#include "targets/ReachZoneComponent.h"
#include "targets/TargetComponent.h"

#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

Game::Game() = default;
Game::~Game() = default;

void Game::RegisterTypes()
{
    Player::Register();
    Bullet::Register();
    JumpPlatform::Register();

    game::TargetComponent::Register();
    game::MovingTargetComponent::Register();
    game::KnockTargetComponent::Register();
    game::ReachZoneComponent::Register();
    game::CrosshairComponent::Register();
    game::HudPanelComponent::Register();
}

bool Game::Init()
{
    auto& engine = eng::Engine::GetInstance();
    auto& input = engine.GetInputManager();
    auto& fs = engine.GetFileSystem();

    // A course project (project.json in the asset root, or a folder passed on
    // the command line) drives everything the editor authors: drill list and
    // order, music, audio mix, key bindings, weapon loadout, movement feel.
    // Without one, fall back to the legacy drills/course.json course.
    if (!m_projectRoot.empty())
    {
        fs.SetAssetsRootOverride(m_projectRoot);
    }
    game::CourseProject project;
    const auto projectRoot = fs.GetAssetsFolder();
    bool hasProject = false;
    if (std::filesystem::exists(projectRoot / "project.json"))
    {
        const auto loaded = game::CourseProject::Load(projectRoot, project);
        hasProject = loaded.ok && !project.drills.empty();
        if (!loaded.ok)
        {
            std::cerr << "Course project could not be opened (" << loaded.error
                      << ") - falling back to the built-in course." << std::endl;
        }
    }

    const std::string inputMap = fs.LoadAssetFileText("config/input.json");
    std::string inputError;
    bool inputOk = false;
    try
    {
        if (!inputMap.empty())
        {
            nlohmann::json map = nlohmann::json::parse(inputMap);
            if (hasProject)
            {
                std::vector<std::string> unsupported;
                map = game::PlaySetup::MergeInputMap(map, project.bindings, &unsupported);
                for (const auto& u : unsupported)
                {
                    std::cerr << "Ignoring unrecognised key binding " << u << std::endl;
                }
            }
            inputOk = input.LoadActionMap(map.dump(), &inputError);
        }
    }
    catch (const std::exception& exception)
    {
        inputError = exception.what();
    }
    if (!inputOk)
    {
        std::cerr << "Failed to load input configuration: "
                  << (inputError.empty() ? "asset is empty or missing" : inputError)
                  << std::endl;
        return false;
    }

    std::shared_ptr<eng::Scene> scene;
    if (hasProject)
    {
        auto& audio = engine.GetAudioManager();
        audio.SetMasterVolume(project.masterVolume);
        audio.SetMusicVolume(project.musicVolume);
        audio.SetEffectsVolume(project.effectsVolume);

        // Same setup the editor's playtest applies (PlaySetup), on the menu
        // scene: authored movement feel + the project's weapons.
        // The bundled example has a menu scene; a project made in CourseMaker
        // plays its environment directly (no menu - the run starts at once).
        const std::string scenePath = std::filesystem::exists(projectRoot / "scenes" / "scene.sc")
            ? std::string("scenes/scene.sc") : project.environmentScene;
        try
        {
            nlohmann::json doc = nlohmann::json::parse(fs.LoadAssetFileText(scenePath));
            game::PlaySetup::StripDrillRoot(doc);
            game::PlaySetup::ApplyMovement(doc, project.movementDefaults);
            game::PlaySetup::ApplyLoadout(doc, game::PlaySetup::ResolveLoadout(project, nullptr));
            scene = eng::Scene::CreateFromDocument(doc);
        }
        catch (const std::exception& exception)
        {
            std::cerr << "Failed to parse the main scene: " << exception.what() << std::endl;
        }
    }
    else
    {
        scene = eng::Scene::Load("scenes/scene.sc");
    }
    if (!scene)
    {
        std::cerr << "Failed to load the main scene." << std::endl;
        return false;
    }

    m_scene = scene;
    engine.SetScene(m_scene);

#ifndef NDEBUG
    try
    {
        const std::string serializedScene = m_scene->Serialize().dump();
        const auto roundTripDocument = nlohmann::json::parse(serializedScene);
        if (!roundTripDocument.contains("objects") || !roundTripDocument["objects"].is_array())
        {
            std::cerr << "Scene serialization validation failed." << std::endl;
            return false;
        }
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Scene serialization validation failed: " << exception.what() << std::endl;
        return false;
    }
#endif

    eng::GameObject* playerObject = m_scene->FindObjectByName("MainPlayer");
    if (!playerObject || std::string(playerObject->GetTypeName()) != "Player")
    {
        std::cerr << "Scene is missing a 'MainPlayer' Player object." << std::endl;
        return false;
    }
    auto* player = static_cast<Player*>(playerObject);

    eng::CanvasComponent* menuCanvas = engine.GetUIInputSystem().GetCanvas();
    if (!menuCanvas && !hasProject)
    {
        std::cerr << "Scene has no active canvas." << std::endl;
        return false;
    }

    m_director = std::make_unique<game::CourseDirector>();
    m_director->onQuitRequested = [this]() { SetNeedsToBeClosed(true); };
    if (m_selfTest && m_screenshotDir.empty())
    {
        m_director->SetFastTransitions(true);
    }
    if (hasProject)
    {
        std::vector<game::DrillDefinition> drills;
        for (const auto& drill : project.drills)
        {
            drills.push_back(drill.ToRuntime());
        }
        m_director->Init(m_scene.get(), player, menuCanvas, std::move(drills));
        std::cout << "Playing course '" << project.name << "' (" << project.drills.size()
                  << " drills) from " << projectRoot.string() << std::endl;
        if (!menuCanvas && !m_selfTest)
        {
            m_director->StartRun(); // no menu scene: go straight into the course
        }
    }
    else
    {
        m_director->Init(m_scene.get(), player, menuCanvas);
    }

    if (m_selfTest)
    {
        m_selfTestRunner = std::make_unique<game::SelfTest>(m_scene.get(), m_director.get(), player);
        if (!m_screenshotDir.empty())
        {
            m_selfTestRunner->SetScreenshotDir(m_screenshotDir);
        }
    }

    engine.GetGraphicsAPI().SetClearColor(
        117.0f / 256.0f, 187.0f / 256.0f, 253.0f / 256.0f, 1.0f);
    return true;
}

void Game::Update(float deltaTime)
{
    if (!m_selfTest)
    {
        auto& input = eng::Engine::GetInstance().GetInputManager();
        if (input.WasActionPressed("Pause"))
        {
            m_director->OnPausePressed();
        }
        if (input.WasActionPressed("Confirm"))
        {
            m_director->OnConfirmPressed();
        }
    }

    // The director runs before the scene so anything it spawns, destroys or
    // resets happens while the scene is not mid-iteration.
    m_director->Update(deltaTime);

    m_scene->Update(deltaTime);

    if (m_selfTestRunner && m_selfTestRunner->Update(deltaTime))
    {
        std::cout << (m_selfTestRunner->Passed() ? "SELF-TEST PASSED" : "SELF-TEST FAILED")
                  << "  (" << m_selfTestRunner->Failures() << " failures / "
                  << m_selfTestRunner->Checks() << " checks)" << std::endl;
        SetNeedsToBeClosed(true);
    }
}

void Game::Destroy()
{
    m_selfTestRunner.reset();
    m_director.reset();
}
