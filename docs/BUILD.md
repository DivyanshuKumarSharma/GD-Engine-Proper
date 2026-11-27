# Building GD-Engine / CourseMaker

## Requirements

| | |
|---|---|
| Compiler | C++17 (MSVC 2022 verified; Clang/GCC should work, unverified) |
| Build system | CMake ≥ 3.10 (CMake 4 works; a policy floor is set for vendored deps) |
| Platform | **Windows 11 — verified.** macOS / Linux: code paths exist, **unverified**. |
| GPU | OpenGL 3.3 core |

Default-build dependencies are vendored under `engine/thirdparty/` — no package
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
| `GameDevelopmentProject` | `build/<cfg>/GameDevelopmentProject.exe` | standalone course player — plays a CourseMaker project folder, or the bundled example |
| `course_tests` | `build/<cfg>/course_tests.exe` | pure-logic course/objective/score tests |
| `editor_tests` | `build/<cfg>/editor_tests.exe` | project persistence / undo-redo / movement-model tests |
| `gameplay_tests` | `build/<cfg>/gameplay_tests.exe` | weapon fire timing / magazines / reload / spread, moving-target paths, key-binding and mouse-wheel input tests |
| `Engine` / `imgui` | static libs | engine + editor UI |

The `assets/` folder is copied to the build directory at configure time. Reconfigure after changing assets. Packaged builds place assets beside the executables.

## Run

```bash
build/Debug/CourseMaker.exe                 # opens the bundled example project
build/Debug/CourseMaker.exe path/to/project # opens a specific project folder
build/Debug/GameDevelopmentProject.exe      # play the bundled example course
build/Debug/GameDevelopmentProject.exe path/to/project  # play a CourseMaker project
```

### Headless / CI

```bash
ctest --test-dir build -C Debug --output-on-failure
```

Runs `course_tests`, `editor_tests`, `gameplay_tests`, and `editor_workflow`
(a scripted end-to-end editor run: open → add targets → undo/redo → save →
play → switch weapon and fire → pause/resume music → complete → stop → verify
the authored scene is restored). `editor_workflow` needs an OpenGL context; its
music checks are skipped (not failed) when no audio device is available.

Extra CourseMaker flags:

* `--demo` — run the scripted workflow, print `DEMO WORKFLOW PASSED/FAILED`, quit.
* `--shots <dir> [--frames N]` — run N frames, save PNG screenshots, quit.

`GameDevelopmentProject --selftest` runs a bot playthrough of all three
drills of the bundled example (loaded from its `project.json`); `--shots <dir>`
adds per-state screenshots. Any argument not starting with `-` is a project
folder to play.

## Notes

* First launch writes `coursemaker_layout.ini` next to the executable (the dock
  layout). Delete it to get the default layout back.
* `highscores.json` is written next to `GameDevelopmentProject.exe`; delete it
  before re-running `--selftest` (the harness expects a clean slate for the
  "first run sets a high score" check).

## Project C systems and profiling

See [Project C](PROJECT_C.md) for the headless systems build, Tracy and Google
Benchmark options, frame captures, UDP experiment, Linux sanitizer script and ZIP
packaging. Default builds now include `systems_tests`, `netcode_demo` and
`systems_bench`; CTest has six suites. Optional profiler/benchmark dependencies
require an initial download. The default build still uses vendored dependencies.
