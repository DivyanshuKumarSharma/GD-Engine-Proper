# Windows evidence - 26 November 2026

Hardware: Intel Core i9-9900K @ 3.60 GHz, 16 logical processors. Windows build
10.0.26300. Compiler: MSVC 19.44.35228, x64 Release. Editor viewport/application
window: 1600x900. These are local exploratory measurements, not controlled lab
results. No other-platform results are implied.

## Correctness

- Full application: six CTest suites passed, including the real editor workflow.
- Systems suite: 15,314 assertions across concurrency, spatial queries, navigation,
  packet validation, prediction and socket delivery.
- The editor workflow verifies background image read/decode/upload and A* target
  serialization in addition to the existing shooting, weapons, audio and editing
  checks. Full logs: `editor-main-verification.txt`, `editor-worker-verification.txt`.
- UDP: `netcode_demo --ticks 600 --latency 75 --jitter 20 --loss 0.05` acknowledged
  all 600 inputs with zero final X-position error and 95 simulated drops in the
  recorded run. Latency is configured per direction; this is a virtual timeline
  over real localhost UDP, not an Internet measurement.

## Benchmarks

`windows-algorithms.csv` is the dependency-free runner: five warmups, 100 samples,
128 rays per sample, seed 42. The 10,000-AABB run measured about 21.901 ms brute
force versus 0.367 ms BVH for 128 rays; rebuilding the BVH cost 2.629 ms. These are
static sparse bounds, so the result does not describe heavily overlapping or
constantly rebuilt scenes. The build amortization and distribution matter.

`windows-google-benchmark.json` is a separate experiment using Google Benchmark
1.9.1, three repetitions and a 0.1-second minimum measurement. It uses a single
fixed ray and an open navigation grid. It was built with Tracy enabled but no
capture connection. Do not mix its timings with the portable runner's workload.

## Frame captures

`windows-main.tracy` and `windows-worker.tracy` are actual Tracy 0.11.1 captures of
the same scripted editor workflow. Their zone exports contain rendering,
application update, navigation and asset-streaming zones; the worker capture also
contains the Bullet worker step. Open them in the matching Tracy profiler.

The companion frame CSVs use wall time including presentation. The chart discards
the first 20 frames, leaving 101 samples per run. Nearest-rank p99 is the second
largest value for this sample count. A rare scene-load transition is therefore
excluded from p99 but included in the mean:

| Scheduling | Mean | p99 | Maximum |
|---|---:|---:|---:|
| Main thread | 13.715 ms | 9.010 ms | 576.753 ms |
| Worker + wait | 14.128 ms | 15.728 ms | 607.717 ms |

These numbers **do not demonstrate a physics speedup**. Worker scheduling is off
by default. The traces reveal that asset/scene setup and application work, rather
than this tiny Bullet step, deserve further investigation. Longer repeated runs
with a representative physics-heavy scene are needed for a stable performance
claim. The screenshots in `editor-main/` and `editor-worker/` are separate visual
checks and were not taken during the final timing captures.

Linux headless TSan and ASan passed in [GitHub CI run 45914158897](https://github.com/DivyanshuKumarSharma/GD-Engine-Proper/actions/runs/45914158897)
on Ubuntu 22.04 with GNU 11.4. The filtered logs are `linux-tsan-ci.txt` and
`linux-asan-ci.txt`. This covers the systems library and UDP tests, not the entire
graphical engine or an Arch hardware run. The initial desktop Linux build caught
a pre-existing GCC default-argument compatibility error, fixed with an overload.

The subsequent TSan run reported a double-lock around the timed condition-variable
wait. The unnecessary timed idle wait was replaced with a notified predicate wait
(see the sanitizer follow-up in `docs/PROJECT_C.md`). On commit `9c267c2`, both
headless suites and ten consecutive TSan concurrency runs passed without warning
suppressions. `linux-tsan-stress-ci.txt` records that follow-up from
[CI run 45915230921](https://github.com/DivyanshuKumarSharma/GD-Engine-Proper/actions/runs/45915230921).
ASan also passed on that commit. Earlier captures and benchmarks predate this
wait-protocol change; they have not been relabeled as fresh measurements.

The same run passed all six Linux desktop suites under Mesa/Xvfb and five hosted
Windows suites. Excerpts: `linux-desktop-ci.txt` and `windows-desktop-ci.txt`.
The downloaded Windows CI package also passed the real graphical editor workflow
locally after extraction; see `windows-ci-package-editor.txt`. Both downloaded
archives matched commit `9c267c2`, had clean source-tree manifests, and passed all
136 file-hash checks. They are published as
[Project C Preview 1](https://github.com/DivyanshuKumarSharma/GD-Engine-Proper/releases/tag/v2.0.0-project-c-preview.1).
