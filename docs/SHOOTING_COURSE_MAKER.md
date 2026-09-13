# Shooting Course Maker — status, architecture, and plan

This document is the map for turning the GD-Engine FPS-course sandbox into
**CourseMaker**, a visual authoring tool where a beginner builds, configures,
saves, shares and plays a shooting course without writing code.

It records: the gap analysis, the architecture that was chosen and why, exactly
what is implemented and verified today, and the sequence for the rest.

---

## 1. Baseline (before this work)

The repository was an early-but-real custom C++17/OpenGL engine plus a **complete,
uncommitted FPS training-course demo**:

* `engine/` — window, GL renderer + render queue, GLTF loader (cgltf), Bullet
  physics, miniaudio, FreeType text, a component scene graph with JSON
  serialization and prefabs, a custom in-game UI/canvas system.
* `source/` — `Game` (menu → 3 drills → results), `CourseDirector` state machine,
  `DrillRunner`, objectives (shoot/knock/reach/finish), `TargetComponent` +
  moving/knock/reach targets, `ShootingController` (hitscan), `Player` +
  `PlayerConfig`, HUD, `HighScoreStore`, `tests/course_tests.cpp`, `source/selftest/`.
* `assets/drills/drill0{1,2,3}.json` — the data-driven example course.

Build baseline: `GameDevelopmentProject` and `course_tests` compiled clean;
`course_tests` = 101 checks / 0 failures; `--selftest` = 32 checks / 0 failures.

## 2. Product decisions

| Decision | Choice | Why |
|---|---|---|
| Editor UI toolkit | **Dear ImGui (docking branch, 1.92.x) + ImGuizmo** | De-facto standard for C++/OpenGL tool UIs; native GLFW + GL3 backends match the engine's exact stack; docking branch gives resizable/persisted panels for free; MIT; **linked only into the editor**, never the runtime. |
| Editor vs runtime | Two executables from one code tree: `CourseMaker` (editor) and `GameDevelopmentProject` (player), sharing `COURSE_RUNTIME_SOURCES` | The runtime consumes the *same* course/drill/target/weapon code the editor authors — no separate demo behaviour. |
| Scene in a panel | Engine renders the scene into an **offscreen `Framebuffer`**; the editor shows that texture in a dock window and supplies its own fly-camera via `Engine::SetCameraOverride` | Minimal, additive engine change (`~40` lines in `Engine::Run`); the runtime path is byte-for-byte unchanged when no target/override is set. |
| Project format | Portable folder: `project.json` + `drills/*.json` + `scenes/*.sc` + copied `assets/`; every reference project-relative; `FileSystem::SetAssetsRootOverride` re-roots all asset loading at the open project | Move/zip/share the folder and it reopens (acceptance 12–13). The bundled example project *is* `assets/` + a `project.json`. |
| Play mode isolation | `PlayController` snapshots the authored scene to JSON, builds a **fresh `Scene`** to simulate, and on Stop rebuilds the authored scene from the snapshot | Stopping restores authored state exactly (positions, deleted objects, timers) because the authored scene was never simulated. |
| Stable identity | `GameObject` gets a process-unique `id`, serialized as `"id"`, used to address undo commands and selection so they survive rebuilds | Required for reliable undo/redo and reference validation (acceptance 8–14). |

## 3. What is implemented and verified

Everything below **builds, runs, and is exercised by an automated test** (`ctest`
targets: `course_tests`, `editor_tests`, `editor_workflow`).

### Engine (additive, backward-compatible)
* `graphics/Framebuffer` — offscreen RGBA8 + depth target.
* `Engine::SetSceneRenderTarget` / `SetCameraOverride` / `GetWindow`; new
  `Application::OnPostRender()` hook (default no-op).
* `FileSystem::SetAssetsRootOverride` — re-root asset loading at a project folder.
* `InputManager::SetSuppressed` — hard gate so panel/camera input never reaches gameplay.
* `GameObject` stable ids; `Scene::CreateFromDocument`, `RenderEditorPreview`
  (draw meshes without ticking gameplay), `GetRootObjects`, `FindObjectById`,
  `DestroyImmediate`, `SetSiblingIndex`, name accessors.
* `MeshComponent::Render()` / `GetMesh()`; `Mesh::CreateQuad/CreatePolygon/CreateCylinder`;
  new mesh types `quad`, `disc`/`circle`, `triangle`/`polygon`, `cylinder`.

### CourseMaker editor
* Dear ImGui docking shell, dark theme with cyan/violet/green/warm accents, a
  built-in default layout, per-install `coursemaker_layout.ini` persistence.
* **Panels**: Menu bar, Toolbar (Play/Pause/Stop/Restart, gizmo mode, grid snap),
  Viewport (FBO image, free-fly camera on RMB+WASDQE, ImGuizmo move/rotate/scale
  with grid snap, analytic click-to-select), Hierarchy (tree + context menu),
  Inspector (name, transform with drag + merged-undo, live editors for
  Mesh/Target/MovingTarget/Light components), Assets (recursive scan, search,
  type filter, drag-and-drop to viewport, missing-file flagging), Course (course
  name, **movement defaults** with feel presets + advanced params, **audio mix**,
  **weapons** — all fields per weapon, **key bindings** with conflict detection,
  **drills** — add / reorder / duplicate / delete with no hard cap, per-drill
  settings), Diagnostics (levelled log, captures engine `stderr`, "Revalidate").
* **Undo/redo**: `CommandStack` with merge (drag = one step) and save-point dirty
  tracking; `Transform/Create/Delete/ModifyObject` commands keyed by stable id;
  `Ctrl+Z/Y`, `Ctrl+S`, `Ctrl+D`, `Del`, `F`, `W/E/R` shortcuts.
* **First-person placement**: `PlacementTool` — ghost preview at the crosshair
  raycast surface or a chosen distance, wheel = distance, `R` = rotate, grid
  snap, Shift = align to surface, click = confirm / Esc = cancel, result is one
  undoable `CreateObjectCommand`. Distinct from firing and from scored play.
* **Add menu**: circle/square/triangle targets, box/sphere/cylinder/ramp,
  floor/wall/cover/light/player-spawn, knock cube, reach zone, moving target.
* **Project system** (`CourseProject`): create / open / save / save-as, portable
  folders, atomic writes (temp + rename), timestamped backups on save, schema
  version with a clear "saved by a newer build" migration error, validation
  (empty course, no objectives, target count vs. objective demand, unknown
  objective type, undefined allowed-weapon, binding conflicts, drill-count sanity).
* **Play mode**: snapshot → isolated `Scene` → Stop rebuilds authored scene;
  Pause/Resume freeze physics; Restart rebuilds from the snapshot; frame-time
  clamp so a stall can't fling bodies.
* **Weapons**: `WeaponDef` (pistol / SMG / automatic rifle / sniper; semi/auto/burst;
  fire rate, damage, magazine, reload, spread, recoil, sniper aim-zoom, model +
  grip + muzzle offsets, per-slot sounds) + `WeaponCatalog` (simple original
  blockout model per category, labels). Four polished defaults.

### Runtime gameplay bridge (playtesting inside the editor)
* **`CourseDirector` wired into editor Play** (`CourseDirector::Init` gained an
  overload that takes a drill list directly instead of reading
  `DrillCatalog::Load()`'s hard-coded `drills/course.json`, and `BeginDrill` is
  now public for "play just this one drill"). Play Drill / Play Course toolbar
  buttons; the drill's own `DrillRoot` layer is stripped from the play snapshot
  first so `DrillRunner` is the sole spawner (no duplicate targets). Real
  intro → countdown → **Playing** → objectives → **DrillComplete** →
  **RunComplete** (results, high-score tracking) flow, with the existing HUD
  (crosshair, objective list, timer, results card) built by `HudView`. Esc
  routes to the director's own Pause/Resume (which also frees the cursor so the
  toolbar's Stop/Restart are reachable); Enter confirms/retries; the toolbar
  shows the live state (`Countdown 3…`, `PLAYING`, `PAUSED`, `RESULTS`, …).
* **Weapon bridge**: `WeaponCatalog::EquipModel`/`PlayerOverrideDoc` attach a
  weapon child (the project's imported model, or a category-shaped blockout)
  to the player and bake a `Player::LoadProperties` "weaponOverride" so the
  drill's resolved `WeaponDef` (first of the drill's `allowedWeapons`, else the
  project's first weapon) actually drives fire mode/rate/muzzle/zoom at
  runtime — not just the authoring UI. A weapon's own `fireSound` is registered
  on the player's `AudioComponent` under a stable name and used in place of the
  default "shoot" clip.
* **Sniper aim zoom**: holding `AimDownSights` (right mouse, new fixed action)
  eases the camera FOV toward `1/aimZoom` — frame-rate-independent, ~90ms.
* **New projects are playable, not just the bundled example**:
  `CourseProject::CreateNew` now seeds a `MainPlayer` (camera + controller +
  audio) into the generated environment (it previously had no player at all),
  plus default `config/input.json` / `config/player.json`; `EditorApp::
  SeedDefaultAssets` copies the checker material/texture/shaders, stock
  jump/shoot/step sounds and a UI font from CourseMaker's own bundled assets
  into the new project.
* Additive engine/runtime support for the above: `CameraComponent::SetFov/GetFov`,
  `FileSystem::GetDefaultAssetsFolder()` (the true bundled root, ignoring the
  open project's asset-root override — used only for seeding), `WeaponConfig::
  ApplyOverrides()` (the file-load parser refactored into a reusable overlay so
  the same code loads `config/player.json` *and* applies the inline override).

### Verification
| Test | What it proves | Result |
|---|---|---|
| `course_tests` | existing course/objective/score/drill-catalog logic still correct | **101 checks, 0 failures** |
| `editor_tests` | project create/save/reload round-trip; drill add/reorder/duplicate/delete with order preserved; validation catches impossible configs & schema-too-new; **portable move**; malformed-JSON handling; movement/weapon JSON round-trip incl. `WeaponCatalog` equip/override docs; undo/redo incl. merge, redo-branch discard, dirty tracking; **a brand-new project has a spawned Player, `"camera":"MainPlayer"`, and a complete input action map** | **126 checks, 0 failures** |
| `editor_workflow` (`CourseMaker --demo`) | end-to-end in the real editor: open example → add circle/square/triangle targets → undo/redo → move + undo → **Save** → **Play Drill** → director reaches **Playing** after a real intro+countdown → drill content spawned exactly once → **a weapon model is equipped** → a hitscan shot fired from the spawned, moving player **registers a hit on a real drill target** → completing objectives reaches **DrillComplete** → the dwell elapses into **RunComplete** with a recorded `DrillResult` and computed score → **Stop** → `CourseDirector` torn down → authored scene object count restored | **DEMO WORKFLOW PASSED (26 checks)** |
| `GameDevelopmentProject --selftest` | the original standalone course still plays through all 3 drills via real mechanics (unaffected — the new movement/weapon bridges are opt-in) | **32 checks, 0 failures** |

Measured: `CourseMaker --demo` runs the whole workflow — including a full real
intro/countdown/drill/results cycle — in **~8 s** on the dev machine (Windows 11,
MSVC Debug build, screenshot at 5 checkpoints). The editor renders the 7-object
environment scene (or ~16 objects mid-playtest, including HUD and drill content)
into the viewport panel at 1600×900 with no perceptible frame cost; a formal FPS
capture on a large scene has not been done yet.

## 4. Acceptance criteria — current state

| # | Criterion | State |
|---|---|---|
| 1 | Create & save a project through the UI | **Done** — File ▸ New Project modal, File ▸ Save; `editor_workflow` + `editor_tests` cover it |
| 2 | Add & reorder ≥3 drills, no hard cap | **Done** — Course panel add/reorder/duplicate/delete; each drill's targets load as an editable `DrillRoot` layer in the viewport; `editor_tests::TestDrillCollectionOps` |
| 3 | Place circle / square / triangle targets | **Done** — Add menu; `editor_workflow` |
| 4 | Place a target through the crosshair and undo it | **Done** — `PlacementTool` → undoable `CreateObjectCommand` (interactive; not yet in the headless demo) |
| 5 | Configure & preview a diagonal moving target | **Partial** — moving-target object + inspector fields exist; the runtime component is still 2-point ping-pong, path-preview lines and playback modes pending |
| 6 | Import an image target and a supported 3D model | **Partial** — browser + drag-drop place image/model *targets*; the copy-into-project importer with format validation is pending |
| 7 | Configure movement speed & jump height | **Done** — Course ▸ Movement defaults (+ per-drill override) persists and is baked into the play snapshot; `PlayerControllerComponent` gained a frame-rate-independent accel model + `jumpHeight` (v=√(2gh)); `editor_tests::TestMovementModel` proves frame-rate consistency; exercised live end-to-end by `editor_workflow` |
| 8 | Use all four weapon categories & rebind switching | **Partial** — a drill's resolved `WeaponDef` (fire mode/rate/spread/recoil/sniper zoom/model/sound) now drives real Play-mode gameplay (`editor_workflow` fires it and confirms a hit); still missing: a multi-weapon inventory with slot switching and rebinding at runtime (today one weapon is auto-equipped per drill, and `KeyBindings`' weapon-slot entries aren't yet consumed by anything) |
| 9 | Assign custom music & distinct weapon sounds | **Partial** — a weapon's authored `fireSound` is registered and played in place of the default clip (verified); per-drill background **music** is still authored-only, not started/stopped by the runtime |
| 10 | Play a drill: pause, restart, finish, results | **Done** — `CourseDirector` fully wired into editor Play: real countdown, HUD/objectives/timer, Esc pause/resume, Restart, drill completion → results screen with score + high-score tracking; `editor_workflow` drives the whole state machine end to end (Playing → DrillComplete → RunComplete) and checks a `DrillResult` and final score are produced |
| 11 | Stop play and recover the unchanged scene | **Done** — `PlayController` snapshot/rebuild, `CourseDirector` torn down; `editor_workflow` asserts both |
| 12 | Close & reopen the project intact | **Done** — `CourseProject::Load`/`Save`; `editor_tests::TestCreateSaveReload` |
| 13 | Move the project folder and reopen | **Done** — project-relative refs + asset-root override; `editor_tests::TestPortableMove` |
| 14 | Useful errors for malformed data / missing assets / bad imports, no crash | **Done for data & assets** — Diagnostics panel + validation + `editor_tests::TestMalformedProject`; a missing `MainPlayer` logs a clear error instead of silently no-opping; import errors pending with the importer |
| 15 | Convert a `.blend` when Blender is available, explicit state when not | **Not started** |

## 5. Remaining sequence

Each step keeps every executable runnable and adds a test.

Done since the first pass (was Stage 2/3 below): drill content layers, the
objective editor, wiring `CourseDirector` into editor Play (single drill +
whole course, countdown/pause/restart/completion/results), parameterising
`PlayerControllerComponent` from `MovementSettings` with a frame-rate test, and
a first weapon runtime bridge (fire mode/rate/spread/recoil/sniper zoom/model/
distinct fire sound, auto-equipped per drill).

**Next — finish the weapon & target runtime**
1. Multi-weapon inventory on `Player`: carry every `allowedWeapon` for the
   drill, switch with the saved `KeyBindings` slots (today only one weapon is
   auto-equipped and slot bindings are authored but unconsumed), magazine/ammo
   count, reload timing, an obvious active-weapon HUD indicator.
2. Unify shot resolution so magazine/reload actually gate firing (currently
   `Player`'s cooldown is the only gate - `WeaponDef.magazine`/`reloadSeconds`
   are authored but not enforced) and confirm one trigger pull still can't
   double-count a hit (re-audit `ShootingController` + the cosmetic projectile
   path together, now that weapons vary).
3. Extend `MovingTargetComponent`: presets (H/V/diagonal), arbitrary + multi
   waypoint paths, speed units (constant travel speed), loop / ping-pong /
   one-shot, easing, start delay, zero-length + large-`dt` guards; draw the path
   and a ghost in the viewport.

**Then — asset pipeline & audio**
4. One import path (file pick + drag-drop) → format validation → copy into
   `<project>/assets/` with a stable id → editable import settings → progress /
   errors → missing-file relink. glTF/GLB + PNG/JPEG subset, tested.
5. Optional Blender step: detect/configure a local Blender, convert `.blend` →
   glTF with `--python-exit-code` and **no** auto-run of embedded scripts;
   surface unavailable/failed states; never a playback dependency.
6. Audio: per-drill music starts at real drill start, defined pause/resume,
   stop/transition on restart/complete/drill-change/return-to-editor; weapon
   sound pool so rapid fire doesn't cut previous shots.

**Finally — integration, portability, polish**
7. Runtime opens a **project** (not the bundled course) and plays it; retire the
   hard-coded `assets/drills/course.json` path (`CourseDirector`'s injectable-
   drill-list overload already supports this - `Game` just needs to build a
   `CourseProject` from a project path instead of calling `DrillCatalog::Load`).
8. Full example project demonstrating every feature; default assets with
   licenses beyond today's seeded checker/audio/font set; measured performance
   numbers on a large scene; regression pass of all suites.

## 6. Build & run

```bash
cmake -S . -B build            # configure (VS 2022 generator on Windows)
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

* `build/Debug/CourseMaker.exe [<project-folder>]` — the editor. No argument opens
  the bundled example (`assets/` + `assets/project.json`).
* `build/Debug/GameDevelopmentProject.exe` — the standalone course player.
* `CourseMaker.exe --demo` — headless end-to-end workflow check (used by ctest).
* `CourseMaker.exe --shots <dir> [--frames N]` — headless screenshots for UI review.

See `docs/CREATOR_GUIDE.md`, `docs/BUILD.md` and `docs/FORMATS_AND_DEPENDENCIES.md`.
