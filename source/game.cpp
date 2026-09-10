#include "Game.h"

#include "Bullet.h"
#include "JumpPlatform.h"
#include "Player.h"

#include "course/CourseDirector.h"
#include "hud/CrosshairComponent.h"
#include "hud/HudPanelComponent.h"
#include "selftest/SelfTest.h"
#include "targets/KnockTargetComponent.h"
#include "targets/MovingTargetComponent.h"
#include "targets/ReachZoneComponent.h"
#include "targets/TargetComponent.h"

#include <exception>
#include <iostream>
#include <string>

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

    const std::string inputMap = engine.GetFileSystem().LoadAssetFileText("config/input.json");
    std::string inputError;
    if (inputMap.empty() || !input.LoadActionMap(inputMap, &inputError))
    {
        std::cerr << "Failed to load input configuration: "
                  << (inputError.empty() ? "asset is empty or missing" : inputError)
                  << std::endl;
        return false;
    }

    auto scene = eng::Scene::Load("scenes/scene.sc");
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
    if (!menuCanvas)
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
    m_director->Init(m_scene.get(), player, menuCanvas);

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
