// Pure-logic tests for the CourseMaker editor: project persistence, the drill
// collection, validation, undo/redo and dirty tracking. No window / OpenGL.
// Built as `editor_tests`; run it from the build output.

#include "editor/CommandStack.h"
#include "project/CourseProject.h"
#include "project/WeaponCatalog.h"
#include "scene/components/PlayerControllerComponent.h"

#include <cmath>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <iostream>
#include <string>

namespace
{
    int g_checks = 0;
    int g_failures = 0;

    void Check(bool condition, const char* expr, const char* file, int line)
    {
        ++g_checks;
        if (!condition)
        {
            ++g_failures;
            std::cerr << "FAIL " << file << ':' << line << "  " << expr << '\n';
        }
    }
#define CHECK(cond) Check((cond), #cond, __FILE__, __LINE__)

    std::filesystem::path MakeTempDir(const std::string& tag)
    {
        static long counter = 0;
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        auto base = std::filesystem::temp_directory_path() /
            ("coursemaker_test_" + tag + "_" + std::to_string(stamp) + "_" + std::to_string(++counter));
        std::error_code ec;
        std::filesystem::remove_all(base, ec);
        std::filesystem::create_directories(base, ec);
        return base;
    }

    void TestCreateSaveReload()
    {
        auto root = MakeTempDir("create");
        game::CourseProject project;
        auto created = game::CourseProject::CreateNew(root, "Marksman Fundamentals", project);
        CHECK(created.ok);
        CHECK(std::filesystem::exists(root / "project.json"));
        CHECK(std::filesystem::exists(root / "scenes" / "environment.sc"));
        CHECK(project.drills.size() == 1);
        CHECK(project.weapons.size() == 4);
        CHECK(std::filesystem::exists(root / "drills" / (project.drills[0].id + ".json")));

        // Reload from disk and compare.
        game::CourseProject reloaded;
        auto load = game::CourseProject::Load(root, reloaded);
        CHECK(load.ok);
        CHECK(reloaded.name == "Marksman Fundamentals");
        CHECK(reloaded.drills.size() == 1);
        CHECK(reloaded.weapons.size() == 4);
        CHECK(reloaded.movementDefaults.feelPreset == "responsive");

        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    void TestDrillCollectionOps()
    {
        auto root = MakeTempDir("drills");
        game::CourseProject project;
        game::CourseProject::CreateNew(root, "Course", project);

        project.AddDrill("Speed Course");
        project.AddDrill("Final Test");
        CHECK(project.drills.size() == 3);

        // ids are unique even with similar names
        project.AddDrill("Speed Course");
        CHECK(project.drills[3].id != project.drills[1].id);

        const std::string firstId = project.drills[0].id;
        CHECK(project.MoveDrill(0, 1));
        CHECK(project.drills[1].id == firstId);
        CHECK(!project.MoveDrill(0, -1)); // already at top

        auto* dup = project.DuplicateDrill(0);
        CHECK(dup != nullptr);
        CHECK(project.drills.size() == 5);
        CHECK(project.drills[1].name == project.drills[0].name + " Copy");

        const std::size_t before = project.drills.size();
        CHECK(project.RemoveDrill(1));
        CHECK(project.drills.size() == before - 1);

        std::string err;
        CHECK(project.Save(&err));

        game::CourseProject reloaded;
        CHECK(game::CourseProject::Load(root, reloaded).ok);
        CHECK(reloaded.drills.size() == project.drills.size());
        for (std::size_t i = 0; i < reloaded.drills.size(); ++i)
        {
            CHECK(reloaded.drills[i].id == project.drills[i].id); // order preserved
        }

        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    void TestValidation()
    {
        auto root = MakeTempDir("validate");
        game::CourseProject project;
        game::CourseProject::CreateNew(root, "Course", project);

        // Fresh starter drill: a lone "finish" objective, no targets -> valid.
        auto v0 = project.Validate();
        CHECK(!v0.HasErrors());

        // Ask for target hits with nothing placed -> error.
        game::DrillDoc& d = project.drills[0];
        game::ObjectiveDefinition shoot;
        shoot.type = "shoot"; shoot.description = "Shoot plates"; shoot.amount = 3;
        d.objectives.push_back(shoot);
        auto v1 = project.Validate();
        CHECK(v1.HasErrors());

        // Place three targets -> clears.
        for (int i = 0; i < 3; ++i)
        {
            d.content.push_back({
                { "name", "Plate" + std::to_string(i) },
                { "components", nlohmann::json::array({ { { "type", "TargetComponent" }, { "kind", "red" } } }) },
            });
        }
        auto v2 = project.Validate();
        CHECK(!v2.HasErrors());

        // Unknown objective type -> error.
        game::ObjectiveDefinition bad;
        bad.type = "explode";
        d.objectives.push_back(bad);
        CHECK(project.Validate().HasErrors());

        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    void TestPortableMove()
    {
        auto root = MakeTempDir("port_a");
        auto moved = MakeTempDir("port_b");
        std::error_code ec;
        std::filesystem::remove_all(moved, ec); // SaveAs re-creates

        game::CourseProject project;
        game::CourseProject::CreateNew(root, "Portable", project);
        project.AddDrill("Second");
        std::string err;
        CHECK(project.Save(&err));

        // Simulate "move the folder": copy tree, delete original, load from new.
        std::filesystem::copy(root, moved,
            std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
        CHECK(!ec);
        std::filesystem::remove_all(root, ec);

        game::CourseProject reopened;
        auto load = game::CourseProject::Load(moved, reopened);
        CHECK(load.ok);
        CHECK(reopened.drills.size() == 2);
        CHECK(reopened.Paths().Root() == std::filesystem::weakly_canonical(moved));

        std::filesystem::remove_all(moved, ec);
    }

    void TestMalformedProject()
    {
        auto root = MakeTempDir("malformed");
        std::filesystem::create_directories(root);
        std::ofstream(root / "project.json") << "{ not valid json ";
        game::CourseProject project;
        auto load = game::CourseProject::Load(root, project);
        CHECK(!load.ok);
        CHECK(!load.error.empty());

        // Newer schema version is refused with a clear message.
        std::ofstream(root / "project.json") << R"({ "schemaVersion": 999, "name": "x" })";
        auto load2 = game::CourseProject::Load(root, project);
        CHECK(!load2.ok);
        CHECK(load2.error.find("newer") != std::string::npos);

        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    void TestMovementAndWeaponJson()
    {
        auto m = game::MovementSettings::Preset("responsive");
        auto j = m.ToJson();
        auto back = game::MovementSettings::FromJson(j);
        CHECK(back.moveSpeed == m.moveSpeed);
        CHECK(back.airControl == m.airControl);
        CHECK(back.feelPreset == "responsive");

        auto weapons = game::WeaponDef::Defaults();
        CHECK(weapons.size() == 4);
        bool sawSniper = false;
        for (const auto& w : weapons)
        {
            CHECK(game::WeaponCatalog::IsCategory(w.category));
            CHECK(game::WeaponCatalog::IsFireMode(w.fireMode));
            CHECK(w.FireInterval() > 0.0f);
            auto rj = game::WeaponDef::FromJson(w.ToJson());
            CHECK(rj.id == w.id);
            CHECK(rj.fireRateRpm == w.fireRateRpm);
            CHECK(rj.magazine == w.magazine);
            if (w.category == "sniper") { sawSniper = true; CHECK(w.aimZoom > 1.0f); }
        }
        CHECK(sawSniper);

        // Blockout model is well-formed scene JSON: body + barrel + a "Muzzle"
        // transform marker Player resolves for projectile spawn position.
        auto model = game::WeaponCatalog::BlockoutModel(weapons[0]);
        CHECK(model.contains("children"));
        CHECK(model["children"].is_array());
        CHECK(model["children"].size() == 3);
        bool sawMuzzle = false;
        for (const auto& child : model["children"])
        {
            if (child.value("name", "") == "Muzzle") sawMuzzle = true;
        }
        CHECK(sawMuzzle);

        // PlayerOverrideDoc carries the authored fire mode/rate/zoom through to
        // the schema Player::LoadProperties / WeaponConfig::ApplyOverrides expect.
        auto override1 = game::WeaponCatalog::PlayerOverrideDoc(weapons[0]);
        CHECK(override1["weaponObject"] == "Gun");
        CHECK(override1["muzzleObject"] == "Muzzle");
        CHECK(std::fabs(override1["fireInterval"].get<float>() - weapons[0].FireInterval()) < 1e-5f);
        CHECK(override1["semiAuto"] == (weapons[0].fireMode != "auto"));

        // EquipModel falls back to the blockout when no model is imported, and
        // switches to a gltf reference once one is set.
        auto equip1 = game::WeaponCatalog::EquipModel(weapons[0]);
        CHECK(equip1.contains("children")); // blockout shape
        game::WeaponDef withModel = weapons[0];
        withModel.modelPath = "models/imported_smg.glb";
        auto equip2 = game::WeaponCatalog::EquipModel(withModel);
        CHECK(equip2.value("type", "") == "gltf");
        CHECK(equip2.value("path", "") == "models/imported_smg.glb");
    }

    void TestNewProjectHasPlayableDefaults()
    {
        // A freshly created project must be playable out of the box: a spawned
        // Player with a camera + controller, a main-camera reference, and a
        // default input action map - not just the bundled example project.
        auto root = MakeTempDir("new_playable");
        game::CourseProject project;
        auto created = game::CourseProject::CreateNew(root, "Fresh Course", project);
        CHECK(created.ok);

        const auto scenePath = root / "scenes" / "environment.sc";
        CHECK(std::filesystem::exists(scenePath));
        std::ifstream sceneFile(scenePath);
        std::stringstream sceneSs;
        sceneSs << sceneFile.rdbuf();
        auto sceneDoc = nlohmann::json::parse(sceneSs.str());
        CHECK(sceneDoc.value("camera", "") == "MainPlayer");

        // Find MainPlayer anywhere in the tree (it's nested under "Environment").
        std::function<const nlohmann::json*(const nlohmann::json&)> findPlayer =
            [&](const nlohmann::json& node) -> const nlohmann::json*
        {
            if (node.is_object())
            {
                if (node.value("name", "") == "MainPlayer" && node.value("type", "") == "Player")
                {
                    return &node;
                }
                for (auto it = node.begin(); it != node.end(); ++it)
                {
                    if (it->is_structured())
                    {
                        if (auto* found = findPlayer(*it)) return found;
                    }
                }
            }
            else if (node.is_array())
            {
                for (const auto& item : node)
                {
                    if (auto* found = findPlayer(item)) return found;
                }
            }
            return nullptr;
        };
        const nlohmann::json* player = findPlayer(sceneDoc);
        CHECK(player != nullptr);
        if (player)
        {
            bool hasCamera = false, hasController = false;
            for (const auto& c : player->value("components", nlohmann::json::array()))
            {
                if (c.value("type", "") == "CameraComponent") hasCamera = true;
                if (c.value("type", "") == "PlayerControllerComponent") hasController = true;
            }
            CHECK(hasCamera);
            CHECK(hasController);
        }

        CHECK(std::filesystem::exists(root / "config" / "input.json"));
        std::ifstream inputFile(root / "config" / "input.json");
        std::stringstream inputSs;
        inputSs << inputFile.rdbuf();
        auto inputDoc = nlohmann::json::parse(inputSs.str());
        for (const char* action : { "MoveForward", "MoveBackward", "MoveLeft", "MoveRight",
                                    "Jump", "Fire", "AimDownSights", "Pause", "Confirm" })
        {
            CHECK(inputDoc["actions"].contains(action));
        }

        CHECK(std::filesystem::exists(root / "config" / "player.json"));

        std::error_code ec;
        std::filesystem::remove_all(root, ec);
    }

    void TestMovementModel()
    {
        using PCC = eng::PlayerControllerComponent;

        // Jump-height -> jump-speed: v = sqrt(2 g h).
        const float v = PCC::JumpSpeedForHeight(1.1f, 9.81f);
        CHECK(std::fabs(v - std::sqrt(2.0f * 9.81f * 1.1f)) < 1e-4f);
        CHECK(PCC::JumpSpeedForHeight(0.0f, 9.81f) == 0.0f);

        // Accelerating from rest to an 8 m/s target must converge to ~8 m/s and
        // land at nearly the same speed regardless of frame rate.
        auto runToSteady = [](float dt)
        {
            glm::vec2 vel(0.0f);
            const glm::vec2 target(8.0f, 0.0f);
            for (float t = 0.0f; t < 3.0f; t += dt)
            {
                vel = PCC::IntegrateGroundVelocity(vel, target, true, true, 110.0f, 90.0f, 7.0f, 0.3f, dt);
            }
            return glm::length(vel);
        };
        const float s30 = runToSteady(1.0f / 30.0f);
        const float s60 = runToSteady(1.0f / 60.0f);
        const float s144 = runToSteady(1.0f / 144.0f);
        CHECK(std::fabs(s60 - 8.0f) < 0.05f);
        CHECK(std::fabs(s30 - s60) < 0.05f);   // frame-rate independence
        CHECK(std::fabs(s144 - s60) < 0.05f);

        // Releasing input on the ground decays speed toward zero (friction).
        glm::vec2 coast(8.0f, 0.0f);
        for (int i = 0; i < 240; ++i)
        {
            coast = PCC::IntegrateGroundVelocity(coast, glm::vec2(0.0f), false, true, 110.0f, 90.0f, 7.0f, 0.3f, 1.0f / 60.0f);
        }
        CHECK(glm::length(coast) < 0.2f);

        // A huge frame time is clamped, not exploded.
        glm::vec2 spike(0.0f);
        spike = PCC::IntegrateGroundVelocity(spike, glm::vec2(8.0f, 0.0f), true, true, 110.0f, 90.0f, 7.0f, 0.3f, 5.0f);
        CHECK(glm::length(spike) <= 8.01f);
    }

    void TestUndoRedoAndDirty()
    {
        editor::CommandStack stack;
        int value = 0;

        auto set = [&](int from, int to)
        {
            return std::make_unique<editor::FunctionCommand>(
                "Set value",
                [&value, to]() { value = to; },
                [&value, from]() { value = from; });
        };

        CHECK(!stack.IsDirty());
        stack.Execute(set(0, 5));
        CHECK(value == 5);
        CHECK(stack.IsDirty());
        CHECK(stack.CanUndo());

        stack.Execute(set(5, 9));
        CHECK(value == 9);
        CHECK(stack.UndoDepth() == 2);

        stack.Undo();
        CHECK(value == 5);
        stack.Undo();
        CHECK(value == 0);
        CHECK(!stack.CanUndo());
        CHECK(stack.CanRedo());

        stack.Redo();
        CHECK(value == 5);

        stack.MarkSaved();
        CHECK(!stack.IsDirty());
        stack.Redo();
        CHECK(value == 9);
        CHECK(stack.IsDirty());
        stack.Undo();
        CHECK(!stack.IsDirty()); // back at the save point

        // Redo branch discarded past the save point -> permanently dirty.
        stack.MarkSaved();
        stack.Undo();               // value 5, save point ahead at depth 1
        stack.Execute(set(5, 42));  // cuts the redo branch
        CHECK(value == 42);
        CHECK(stack.IsDirty());

        // Merge: rapid same-key edits collapse to one undo step.
        editor::CommandStack drag;
        int v = 0;
        for (int i = 1; i <= 5; ++i)
        {
            drag.Execute(std::make_unique<editor::FunctionCommand>(
                "Drag", [&v, i]() { v = i; }, [&v]() { v = 0; }, "drag:v"));
        }
        CHECK(v == 5);
        CHECK(drag.UndoDepth() == 1);
        drag.Undo();
        CHECK(v == 0);
    }
}

int main()
{
    TestCreateSaveReload();
    TestNewProjectHasPlayableDefaults();
    TestDrillCollectionOps();
    TestValidation();
    TestPortableMove();
    TestMalformedProject();
    TestMovementAndWeaponJson();
    TestMovementModel();
    TestUndoRedoAndDirty();

    std::printf("\neditor_tests: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
