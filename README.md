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

The engine is functional and the editor is a working prototype covered by three
automated suites (`ctest`): pure-logic course tests, project/persistence/movement
tests, and an end-to-end workflow that drives the real editor — open a project,
place targets, undo/redo, save, **enter Play, let a real `CourseDirector` run
the intro → countdown → drill → completion → results screen**, verify a hitscan
shot fired from the spawned, moving player actually registers on a live target,
stop, and confirm the authored scene came back untouched.

Working end to end today:
- Author an environment + any number of drills, each with its own targets,
  objectives, movement override and allowed weapons.
- Playtest one drill or the whole course from inside the editor: the player
  spawns with real frame-rate-independent movement, a weapon model equipped
  from the project's `WeaponDef`s (fire mode/rate/spread/recoil/sniper zoom, a
  distinct sound if one's authored), the HUD/crosshair/objectives/timer, pause
  (Esc) and resume, restart, and the drill-complete → results screen with score
  and high-score tracking.
- New projects are playable immediately — a spawned player, default key
  bindings, and the default checker/audio/font assets are seeded automatically,
  not just the bundled example.
- Save, close, reopen, and move the whole project folder elsewhere.

Still in progress (tracked in `docs/SHOOTING_COURSE_MAKER.md`): a weapon
inventory with slot switching/rebinding/reload (today each drill equips one
weapon automatically), the asset **import** pipeline (drag-drop placement works;
copying an external file into the project with format validation doesn't yet),
moving-target playback modes beyond ping-pong, per-drill music playback, and
Blender import.
