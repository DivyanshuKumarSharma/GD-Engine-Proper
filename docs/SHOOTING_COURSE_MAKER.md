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
targets: `course_tests`, `editor_tests`, `gameplay_tests`, `editor_workflow`).

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
* **Weapon bridge** (second pass; the single auto-equipped weapon described
  here is now the legacy path — see the loadout in the third pass below):
  `WeaponCatalog::EquipModel`/`PlayerOverrideDoc` attach a
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

### Weapon, target, audio and standalone runtime (third pass)
* **`WeaponInventory`** (`source/weapon/`, pure logic, no engine dependency):
  carries the drill's whole loadout (every `allowedWeapon`, else every project
  weapon, max 9). Per weapon: magazine count, reload timer (auto-reload on
  empty, switching cancels a reload), a short draw delay after switching, dry
  fire on an empty trigger pull, and fire cadence from an *accumulating*
  cooldown — `Update(dt, held, pressed)` returns how many rounds to fire this
  frame, so 1200 rpm at 30 fps fires 20 rounds/s exactly and a long stall is
  bounded (`kMaxShotsPerUpdate`). Semi = one round per press; burst = 3 rounds
  then a recovery gap; auto = while held. Spread *blooms* under sustained fire
  and recovers; recoil kick is per round; a seeded RNG keeps spread
  deterministic for tests.
* **`Player`** gained a loadout mode (legacy single-weapon path untouched):
  `WeaponSlot1..9`, `WeaponNext`/`WeaponPrev` (mouse wheel by default),
  `Reload`; every round is queued as a `Shot { origin, direction, damage }`
  so several rounds in one frame all resolve; recoil goes through the new
  `PlayerControllerComponent::AddLookOffset`; one model per weapon with only
  the active one shown (draw dip, reload dip, fire kick); per-weapon fire /
  reload / empty sounds; a cosmetic `Tracer` from the weapon's `Muzzle` marker.
* **Hit resolution stays single-sourced**: `ShootingController` drains the
  shot queue and raycasts each shot; tracers have no physics, so a round can't
  be counted twice. `TargetComponent` gained `health` (0 = one hit) and takes
  the weapon's `damage`, with a scale "punch" on non-lethal hits; the
  `TargetHit` event only fires on the kill. New **Armored target** preset.
* **HUD**: weapon name, `ammo / magazine` (red when low, `RELOADING n%`),
  slot strip (`[2 SMG]`), and the crosshair gap now opens with the *current*
  spread (`tan(spread)/tan(fov/2)` × half-height).
* **Input**: mouse wheel is a bindable action device (`MouseWheelUp/Down`);
  more key names (F1–F12, Shift/Ctrl/Alt, Caps, grave, minus…). The project's
  `KeyBindings` are merged into the engine action map for both editor Play and
  the standalone player; unrecognised tokens are reported, not silently dropped.
  The Course panel's binding editor captures the next key/button/wheel press.
* **`PathMotion`** (`source/targets/`, pure function of time): multi-waypoint
  paths at constant travel speed, ping-pong / loop / once, smoothstep easing
  per segment, start delay, degenerate-path guard, large-`dt` equivalence.
  Presets: horizontal, vertical, diagonal, zigzag, box. New Add-menu entries:
  Moving Target (ping-pong) and Circuit Target (looping box path).
  `MovingTargetComponent` reads `points` (legacy `pointA`/`pointB` still
  load), no longer moves the object on `Init` (which caused positions to drift
  on every load/save), and the inspector edits waypoints, presets, speed,
  playback, easing, delay. The viewport draws each path (closed for loops,
  numbered waypoints when selected) with an animated marker.
* **Audio**: `MusicPlayer` (non-spatial, looping) — the drill's `music` starts
  at GO, pauses/resumes with the director, stops on drill completion / run end
  / return to menu, and isn't restarted when the same track continues.
  `AudioComponent` clips have a voice pool (round-robin) so rapid fire doesn't
  cut previous shots; master / music / effects buses. `Audio` no longer
  un-initialises a sound that never initialised (crash on a bad file).
* **Shootability fixes**: the Add-menu circle/square/triangle and image targets
  previously had **no collider** and couldn't be hit; they now get one.
  Placement ghosts carry no physics so they can't block their own raycast.
* **`PlaySetup`** (`source/project/`): one module for "turn an authored scene
  into a playable one" (find player, strip drill root, apply movement, apply
  loadout, resolve loadout, merge input map), shared by the editor and the
  standalone player.
* **Standalone player plays projects**: `GameDevelopmentProject <folder>` (or
  a `project.json` in the asset root) loads the `CourseProject`, applies its
  mix, bindings, movement and loadout, and feeds its drills to `CourseDirector`.
  A project without a menu scene starts the run immediately. Projects without
  `project.json` fall back to the legacy `drills/course.json`.

### Verification
| Test | What it proves | Result |
|---|---|---|
| `course_tests` | existing course/objective/score/drill-catalog logic still correct | **101 checks, 0 failures** |
| `editor_tests` | project create/save/reload round-trip; drill add/reorder/duplicate/delete with order preserved; validation catches impossible configs & schema-too-new; **portable move**; malformed-JSON handling; movement/weapon JSON round-trip incl. `WeaponCatalog` equip/override docs; undo/redo incl. merge, redo-branch discard, dirty tracking; **a brand-new project has a spawned Player, `"camera":"MainPlayer"`, and a complete input action map** | **126 checks, 0 failures** |
| `gameplay_tests` | `WeaponInventory`: auto fire count identical at 30/60/144 fps, 1200 rpm at 30 fps fires multiple rounds per frame, semi one-per-press, burst = 3 + recovery, magazine empties → auto-reload → refilled after `reloadSeconds`, switch cancels reload + draw delay, dry fire, bloom grows/recovers, recoil, seeded-RNG determinism, a 10 s stall stays bounded; `PathMotion`: ping-pong / loop / once, degenerate path, start delay, easing endpoints, one big step == many small steps, presets; binding tokens → engine bindings; `InputManager` mouse-wheel actions | **136 checks, 0 failures** |
| `editor_workflow` (`CourseMaker --demo`) | end-to-end in the real editor: open example → add circle/square/triangle targets → undo/redo → move + undo → add a moving target and confirm save/reload doesn't drift it → **Save** → **Play Drill** → director reaches **Playing** after a real intro+countdown → drill content spawned exactly once → **the full loadout is carried, one model per weapon, only slot 1 visible** → switch to slot 2 → holding the trigger fires the SMG through `WeaponInventory` and **registers hits on real drill targets** → the moving target travels → **drill music plays, pauses with Esc, resumes** → completing objectives reaches **DrillComplete** (music stops) → **RunComplete** with a `DrillResult` and score → **Stop** → `CourseDirector` torn down → authored scene restored, moving target back at its authored position | **DEMO WORKFLOW PASSED (42 checks)** |
| `GameDevelopmentProject --selftest` | the bundled course plays through all 3 drills via real mechanics — now loaded **from `project.json`** with the project's movement and loadout applied | **32 checks, 0 failures** |

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
| 5 | Configure & preview a diagonal moving target | **Done** — Diagonal preset (or any waypoints) in the inspector; ping-pong / loop / once, speed, easing, start delay; path + animated marker drawn in the viewport; `gameplay_tests` covers `PathMotion`, `editor_workflow` confirms it moves in Play and is unchanged after Stop |
| 6 | Import an image target and a supported 3D model | **Partial** — browser + drag-drop place image/model *targets*; the copy-into-project importer with format validation is pending |
| 7 | Configure movement speed & jump height | **Done** — Course ▸ Movement defaults (+ per-drill override) persists and is baked into the play snapshot; `PlayerControllerComponent` gained a frame-rate-independent accel model + `jumpHeight` (v=√(2gh)); `editor_tests::TestMovementModel` proves frame-rate consistency; exercised live end-to-end by `editor_workflow` |
| 8 | Use all four weapon categories & rebind switching | **Done** — pistol / SMG / rifle / sniper carried together; slots 1–9 + wheel next/prev + reload, all rebindable with press-to-bind capture and conflict detection; magazines, reload, spread bloom, recoil, damage enforced by `WeaponInventory` (`gameplay_tests`); `editor_workflow` switches weapon and fires through it |
| 9 | Assign custom music & distinct weapon sounds | **Done** — per-drill music starts at GO, pauses/resumes, stops on completion (`editor_workflow`, real audio device); per-weapon fire / reload / empty sounds with overlapping voices; master / music / effects mix |
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

Done in the third pass: the weapon inventory (switching, rebinding incl. mouse
wheel, magazines, reload, bloom, recoil, damage, HUD), queued multi-round shot
resolution with no double counting, target health, multi-waypoint moving
targets with presets / playback modes / easing / delay / viewport preview,
per-drill music + voice-pooled weapon sounds + mix buses, and the standalone
player opening a project folder.

**Next — asset pipeline**
1. One import path (file pick + drag-drop) → format validation → copy into
   `<project>/assets/` with a stable id → editable import settings → progress /
   errors → missing-file relink. glTF/GLB + PNG/JPEG subset, tested.
2. Optional Blender step: detect/configure a local Blender, convert `.blend` →
   glTF with `--python-exit-code` and **no** auto-run of embedded scripts;
   surface unavailable/failed states; never a playback dependency.
3. Image targets: alpha-tested hits (sample the texture at the hit UV) instead
   of today's documented full-rectangle hit area.

**Then — runtime parity & polish**
4. Standalone whole-course runs apply each drill's own movement override and
   weapon restrictions as the drill begins (the editor's drill playtests
   already do; the standalone player currently uses the project defaults and
   full loadout for the whole run).
5. Crosshair spread uses the camera's live FOV instead of assuming 60°; a
   no-menu standalone run's "back to menu" at the results screen should quit or
   restart instead of landing on an empty menu state.
6. Full example project demonstrating every feature; default assets with
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
* `build/Debug/GameDevelopmentProject.exe [<project-folder>]` — the standalone
  course player; with a folder it plays that CourseMaker project.
* `CourseMaker.exe --demo` — headless end-to-end workflow check (used by ctest).
* `CourseMaker.exe --shots <dir> [--frames N]` — headless screenshots for UI review.

See `docs/CREATOR_GUIDE.md`, `docs/BUILD.md` and `docs/FORMATS_AND_DEPENDENCIES.md`.
