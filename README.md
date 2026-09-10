# GD-Engine

A game engine I'm building from scratch in modern C++, and **CourseMaker** — a
visual shooting-course authoring tool built on top of it.

The idea: a custom engine plus a no-code editor where you create, configure,
save, share and play your own first-person shooting courses.

---

## Two applications

| | |
|---|---|
| **CourseMaker** | The editor. A dockable desktop tool — 3D viewport, scene hierarchy, inspector, asset browser, course/drill organizer — for building courses without writing code: place targets, configure movement and weapons, playtest, save, reopen. |
| **Course player** (`GameDevelopmentProject`) | The standalone runtime that plays a course project through the same simulation code the editor authors. Ships with an example course (Target Practice → Speed Course → Final Test). |

See [`docs/CREATOR_GUIDE.md`](docs/CREATOR_GUIDE.md) for a walkthrough and
[`docs/SHOOTING_COURSE_MAKER.md`](docs/SHOOTING_COURSE_MAKER.md) for the full
design, current status and roadmap.

---

## The engine

Written from the ground up — no engine framework underneath.

- **Rendering** — OpenGL 3.3 core, a render queue, custom shaders, an offscreen
  render target the editor draws the scene into.
- **Scene** — component-based game objects, JSON serialization, prefabs, stable
  object ids.
- **Physics** — Bullet: kinematic character controller, rigid bodies, raycasts,
  trigger volumes.
- **Audio** — miniaudio.
- **Text / UI** — FreeType glyph atlas, an in-engine canvas/button system.
- **Assets** — glTF/GLB model loading (cgltf), PNG/JPEG images (stb_image),
  portable project folders that resolve every reference relative to the project.
- **Editor layer** — Dear ImGui (docking) + ImGuizmo, compiled separately and
  linked only into CourseMaker; the runtime has no dependency on it.

---

## Tech stack

| | |
|---|---|
| Language | C++17 |
| Build | CMake (≥ 3.10) |
| Graphics | OpenGL, GLFW, GLEW, GLM |
| Physics | Bullet 3.25 |
| Audio | miniaudio |
| Text | FreeType |
| Data | nlohmann/json |
| Editor UI | Dear ImGui (docking) + ImGuizmo |

All third-party libraries are vendored under `engine/thirdparty/` — no package
manager required. Full matrix in
[`docs/FORMATS_AND_DEPENDENCIES.md`](docs/FORMATS_AND_DEPENDENCIES.md).

---

## Build

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Then:

```bash
build/Debug/CourseMaker.exe                  # the editor (opens the example project)
build/Debug/CourseMaker.exe path/to/project  # open a specific project folder
build/Debug/GameDevelopmentProject.exe       # play the example course
```

On Windows, `gen_proj_vs2022.bat` generates a Visual Studio 2022 solution.
Details and headless/CI flags in [`docs/BUILD.md`](docs/BUILD.md).

**Platforms:** developed and verified on Windows 11. macOS / Linux code paths
exist but are currently unverified.

---

## Repository layout

| Path | |
|---|---|
| `engine/source/` | engine — rendering, scene, physics, audio, text, IO |
| `engine/thirdparty/` | vendored dependencies |
| `source/` | game runtime — course lifecycle, drills, objectives, targets, weapons, player, HUD |
| `source/editor/` | CourseMaker — the editor application |
| `source/project/` | project / course document model (shared by editor and runtime) |
| `assets/` | the bundled example project |
| `tests/` | test suites (`course_tests`, `editor_tests`) |
| `docs/` | design, creator guide, build, format matrix |

---

## Status

The engine is functional and the editor is a working prototype: the full
create → build → place → configure → playtest → save → reopen loop runs, covered
by an automated end-to-end test. Weapon switching, the asset import pipeline,
moving-target playback modes, in-editor scored playtesting and Blender import are
in progress — tracked in `docs/SHOOTING_COURSE_MAKER.md`.
