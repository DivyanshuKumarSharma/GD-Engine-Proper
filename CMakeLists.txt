cmake_minimum_required(VERSION 3.10)

# Some vendored dependencies still declare pre-3.5 CMake compatibility.
# CMake 4 removed that compatibility unless a policy floor is provided.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

project (GameDevelopmentProject)

# ---------------------------------------------------------------------------
# Shared runtime: the course/drill/target/weapon/player simulation. Both the
# standalone player (GameDevelopmentProject) and the editor (CourseMaker)
# compile this so a course plays back through exactly the same code that the
# editor authors.
# ---------------------------------------------------------------------------
set(COURSE_RUNTIME_SOURCES
  source/Player.h
  source/Player.cpp
  source/PlayerConfig.h
  source/PlayerConfig.cpp
  source/Bullet.h
  source/Bullet.cpp
  source/JumpPlatform.h
  source/JumpPlatform.cpp
  source/TestObject.h
  source/TestObject.cpp

  source/util/Signal.h
  source/util/Log.h
  source/util/Format.h
  source/util/Stopwatch.h
  source/util/Stopwatch.cpp

  source/stats/PlayerStats.h
  source/stats/ScoreCalculator.h
  source/stats/ScoreCalculator.cpp
  source/stats/HighScoreStore.h
  source/stats/HighScoreStore.cpp

  source/course/GameState.h
  source/course/GameEvent.h
  source/course/GameEvent.cpp
  source/course/DrillResult.h
  source/course/CourseDirector.h
  source/course/CourseDirector.cpp

  source/objectives/Objective.h
  source/objectives/Objective.cpp
  source/objectives/ShootObjective.h
  source/objectives/ShootObjective.cpp
  source/objectives/KnockObjective.h
  source/objectives/KnockObjective.cpp
  source/objectives/ReachZoneObjective.h
  source/objectives/ReachZoneObjective.cpp
  source/objectives/FinishDrillObjective.h
  source/objectives/FinishDrillObjective.cpp
  source/objectives/ObjectiveFactory.h
  source/objectives/ObjectiveFactory.cpp

  source/drills/DrillDefinition.h
  source/drills/DrillCatalog.h
  source/drills/DrillCatalog.cpp
  source/drills/DrillRunner.h
  source/drills/DrillRunner.cpp

  source/targets/TargetComponent.h
  source/targets/TargetComponent.cpp
  source/targets/MovingTargetComponent.h
  source/targets/MovingTargetComponent.cpp
  source/targets/KnockTargetComponent.h
  source/targets/KnockTargetComponent.cpp
  source/targets/ReachZoneComponent.h
  source/targets/ReachZoneComponent.cpp

  source/weapon/ShootingController.h
  source/weapon/ShootingController.cpp

  source/hud/CrosshairComponent.h
  source/hud/CrosshairComponent.cpp
  source/hud/HudPanelComponent.h
  source/hud/HudPanelComponent.cpp
  source/hud/HudView.h
  source/hud/HudView.cpp

  # --- Project / course document model (authored data, shared with the editor) ---
  source/project/SchemaVersion.h
  source/project/CourseProject.h
  source/project/CourseProject.cpp
  source/project/ProjectPaths.h
  source/project/ProjectPaths.cpp
  source/project/WeaponCatalog.h
  source/project/WeaponCatalog.cpp
)

# --- Editor-only sources (Dear ImGui tool shell) ---
set(EDITOR_SOURCES
  source/editor/EditorMain.cpp
  source/editor/EditorApp.h
  source/editor/EditorApp.cpp
  source/editor/EditorCamera.h
  source/editor/EditorCamera.cpp
  source/editor/EditorLog.h
  source/editor/EditorLog.cpp
  source/editor/CommandStack.h
  source/editor/CommandStack.cpp
  source/editor/EditorCommands.h
  source/editor/EditorCommands.cpp
  source/editor/PlayController.h
  source/editor/PlayController.cpp
  source/editor/AssetLibrary.h
  source/editor/AssetLibrary.cpp
  source/editor/PrimitiveFactory.h
  source/editor/PrimitiveFactory.cpp
  source/editor/PlacementTool.h
  source/editor/PlacementTool.cpp
)

include_directories(engine/thirdparty/glew/include)
include_directories(engine/thirdparty/glm-1.0.1)
include_directories(source)

add_compile_definitions(GLEW_NO_GLU)
add_subdirectory(engine "${CMAKE_CURRENT_BINARY_DIR}/engine_build")
include_directories(engine/source)
include_directories(engine/thirdparty/stb_image)
include_directories(engine/thirdparty/json)

# ---------------------------------------------------------------------------
# Standalone player: opens a course project (or the bundled example) and plays.
# ---------------------------------------------------------------------------
add_executable(${PROJECT_NAME}
  source/main.cpp
  source/Game.h
  source/Game.cpp
  source/selftest/SelfTest.h
  source/selftest/SelfTest.cpp
  ${COURSE_RUNTIME_SOURCES}
)
target_link_libraries(${PROJECT_NAME} Engine)

# ---------------------------------------------------------------------------
# CourseMaker: the visual editor / authoring tool.
# ---------------------------------------------------------------------------
add_executable(CourseMaker ${EDITOR_SOURCES} ${COURSE_RUNTIME_SOURCES})
target_include_directories(CourseMaker PRIVATE source engine/source engine/thirdparty/imgui engine/thirdparty/imgui/backends)
target_link_libraries(CourseMaker Engine imgui)
target_compile_definitions(CourseMaker PRIVATE COURSEMAKER_EDITOR=1)
if(WIN32)
  target_link_libraries(CourseMaker shell32 ole32)
endif()

add_custom_command(TARGET CourseMaker POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${CMAKE_SOURCE_DIR}/assets"
  "${CMAKE_BINARY_DIR}/assets"
  VERBATIM
)

# --- Pure-logic tests for the course framework (no window / OpenGL) ---
set(COURSE_TEST_SOURCES
  tests/course_tests.cpp
  source/util/Stopwatch.cpp
  source/stats/ScoreCalculator.cpp
  source/stats/HighScoreStore.cpp
  source/course/GameEvent.cpp
  source/objectives/Objective.cpp
  source/objectives/ShootObjective.cpp
  source/objectives/KnockObjective.cpp
  source/objectives/ReachZoneObjective.cpp
  source/objectives/FinishDrillObjective.cpp
  source/objectives/ObjectiveFactory.cpp
  source/drills/DrillCatalog.cpp
)
add_executable(course_tests ${COURSE_TEST_SOURCES})
target_include_directories(course_tests PRIVATE source engine/source engine/thirdparty/json engine/thirdparty/glm-1.0.1)
target_link_libraries(course_tests Engine)

# --- Editor / project-model tests (no window / OpenGL) ---
add_executable(editor_tests
  tests/editor_tests.cpp
  source/project/CourseProject.cpp
  source/project/ProjectPaths.cpp
  source/project/WeaponCatalog.cpp
  source/editor/CommandStack.cpp
)
target_include_directories(editor_tests PRIVATE source engine/source engine/thirdparty/json engine/thirdparty/glm-1.0.1)
target_link_libraries(editor_tests Engine)

enable_testing()
add_test(NAME course_tests COMMAND course_tests)
add_test(NAME editor_tests COMMAND editor_tests)
# End-to-end editor workflow: opens the example project, adds targets, undo/redo,
# save, play, stop, verifies the authored scene is restored. Needs a GL context.
add_test(NAME editor_workflow COMMAND CourseMaker --demo)
set_tests_properties(editor_workflow PROPERTIES
  PASS_REGULAR_EXPRESSION "DEMO WORKFLOW PASSED"
  FAIL_REGULAR_EXPRESSION "DEMO WORKFLOW FAILED"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
