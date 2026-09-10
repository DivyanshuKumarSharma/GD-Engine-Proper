# Supported formats & dependency matrix

Only what has actually been built and exercised is listed as supported. "Planned"
means the schema/UI exists but the pipeline is not finished.

## Asset formats

| Kind | Extensions | Status | Notes |
|---|---|---|---|
| 3D model | `.gltf`, `.glb` | **Load: works** (cgltf, triangles + base-colour texture + node transforms + skinned-clip import). **Import pipeline: planned.** | Materials beyond base colour, cameras, lights, morph targets are ignored. `.glb` binary chunk parsing is inherited from cgltf and not yet regression-tested here. |
| 3D model | `.blend` | **Planned** — optional local-Blender conversion step, never a playback dependency. Not started. |
| Image | `.png`, `.jpg`/`.jpeg` | **Load: works** (stb_image). Image-target builder keeps aspect ratio; transparent-area hit behaviour is authored explicitly (`meta.transparentAreaHits`, default off). **Import/relink UI: planned.** |
| Image | `.bmp`, `.tga` | Recognised by the browser; stb_image can decode them but they are not part of the tested subset. |
| Audio | `.wav` | **Works** (miniaudio) — used for weapon/step/jump sounds today. |
| Audio | `.mp3`, `.ogg`, `.flac` | miniaudio can decode these in principle; **not tested** in this build. Music playback wiring is planned. |
| Font | `.ttf` | **Works** (FreeType) — used by the in-game HUD/menus. |
| Scene | `.sc` | Engine scene JSON (objects / components / children / prefabs / stable ids). |
| Project | `project.json` + `drills/*.json` | CourseMaker project. Schema v1. Versioned; a newer file is refused with a clear message. |

## Runtime / library dependencies (all vendored)

| Library | Version | Role | License |
|---|---|---|---|
| GLFW | 3.4 | window + input | Zlib |
| GLEW | (vendored) | GL function loading | BSD/MIT |
| GLM | 1.0.1 | math | MIT |
| Bullet | 3.25 | physics (kinematic character controller, raycasts, rigid bodies) | Zlib |
| miniaudio | (vendored) | audio playback | public domain / MIT-0 |
| FreeType | 2.14.1 | text | FreeType License (BSD-like) |
| cgltf | (vendored) | glTF parsing | MIT |
| stb_image / stb_image_write | (vendored) | image load / screenshot save | public domain / MIT |
| nlohmann/json | (vendored) | JSON | MIT |
| **Dear ImGui** | **docking branch, 1.92.x WIP** (fetched 2026-09-09) | **editor UI only** | MIT |
| **ImGuizmo** | **master** (fetched 2026-09-09) | **editor transform gizmos only** | MIT |

ImGui + ImGuizmo are compiled into a separate `imgui` static library that is
linked **only** into `CourseMaker`. `GameDevelopmentProject` (the runtime player)
does not depend on them.

## Platform support

| Platform | Status |
|---|---|
| Windows 11 (MSVC 2022) | **Verified** — builds, all tests pass, editor + player run. |
| macOS | Unverified. Platform-specific code (`FileSystem`, audio frameworks, the Win32 folder picker) is isolated; the editor falls back to a typed-path dialog where a native picker is missing. |
| Linux | Unverified. Same as macOS; X11 is forced in `Engine::Init`. |
