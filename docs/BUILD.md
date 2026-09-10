# Building GD-Engine / CourseMaker

## Requirements

| | |
|---|---|
| Compiler | C++17 (MSVC 2022 verified; Clang/GCC should work, unverified) |
| Build system | CMake ≥ 3.10 (CMake 4 works; a policy floor is set for vendored deps) |
| Platform | **Windows 11 — verified.** macOS / Linux: code paths exist, **unverified**. |
| GPU | OpenGL 3.3 core |

All other dependencies are vendored under `engine/thirdparty/` — no package
manager needed.

## Configure & build

```bash
cmake -S . -B build
cmake --build build --config Debug        # or Release
```

On Windows this uses the Visual Studio 2022 generator by default
(`gen_proj_vs2022.bat` does the same). Open `build/GameDevelopmentProject.sln`
in Visual Studio if you prefer.

### Targets

| Target | Output | What it is |
|---|---|---|
| `CourseMaker` | `build/<cfg>/CourseMaker.exe` | the visual editor |
| `GameDevelopmentProject` | `build/<cfg>/GameDevelopmentProject.exe` | standalone course player (the original FPS demo) |
| `course_tests` | `build/<cfg>/course_tests.exe` | pure-logic course/objective/score tests |
| `editor_tests` | `build/<cfg>/editor_tests.exe` | project persistence / undo-redo / movement-model tests |
| `Engine` / `imgui` | static libs | engine + editor UI |

The `assets/` folder is copied next to the binaries as a post-build step.

## Run

```bash
build/Debug/CourseMaker.exe                 # opens the bundled example project
build/Debug/CourseMaker.exe path/to/project # opens a specific project folder
build/Debug/GameDevelopmentProject.exe      # play the built-in course
```

### Headless / CI

```bash
ctest --test-dir build -C Debug --output-on-failure
```

Runs `course_tests`, `editor_tests`, and `editor_workflow` (a scripted
end-to-end editor run: open → add targets → undo/redo → save → play → stop →
verify the authored scene is restored). `editor_workflow` needs an OpenGL
context.

Extra CourseMaker flags:

* `--demo` — run the scripted workflow, print `DEMO WORKFLOW PASSED/FAILED`, quit.
* `--shots <dir> [--frames N]` — run N frames, save PNG screenshots, quit.

`GameDevelopmentProject --selftest` runs the original bot playthrough of all
three drills; `--shots <dir>` adds per-state screenshots.

## Notes

* First launch writes `coursemaker_layout.ini` next to the executable (the dock
  layout). Delete it to get the default layout back.
* `highscores.json` is written next to `GameDevelopmentProject.exe`; delete it
  before re-running `--selftest` (the harness expects a clean slate for the
  "first run sets a high score" check).
