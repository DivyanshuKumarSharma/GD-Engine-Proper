#pragma once
#include <eng.h>
#include <memory>
#include <string>

namespace game
{
    class CourseDirector;
    class SelfTest;
}

class Game : public eng::Application
{
public:
    Game();
    ~Game() override;

    void SetSelfTest(bool enabled) { m_selfTest = enabled; }
    void SetScreenshotDir(std::string dir) { m_screenshotDir = std::move(dir); }
    // Play a CourseMaker project folder instead of the bundled example.
    void SetProjectRoot(std::string root) { m_projectRoot = std::move(root); }

    void RegisterTypes() override;
    bool Init() override;
    void Update(float deltaTime) override;
    void Destroy() override;

private:
    std::shared_ptr<eng::Scene> m_scene;
    std::unique_ptr<game::CourseDirector> m_director;
    std::unique_ptr<game::SelfTest> m_selfTestRunner;
    bool m_selfTest = false;
    std::string m_screenshotDir;
    std::string m_projectRoot;
};
