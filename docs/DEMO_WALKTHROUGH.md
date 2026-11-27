# Project C demonstration: about four minutes

Record your own explanation and actions. The lines below are prompts, not claims
of experience to memorize. Keep the profiler, editor and source visible enough
that a reviewer can connect each claim to the implementation.

## 0:00–0:40 — Show the tool

Open CourseMaker and play the supplied course briefly. Stop play, add an **A*
Navigation Target**, and edit its grid. Show the route changing around blocked
cells, then play again. Explain that the engine supports an authoring workflow
and a separate course runtime. The grid is authored explicitly; it is not a
navmesh generated from scene geometry.

## 0:40–1:30 — Explain concurrency ownership

Hover an image asset to show its preview. Open
`engine/source/graphics/Texture.cpp` and `engine/source/systems/JobSystem.cpp`.
Point out that file reads and decoding run in jobs while OpenGL upload remains
on the main thread. Explain owner-LIFO and thief-FIFO queues, cooperative nested
waits, exception propagation, and draining shutdown. Show the ten-repeat TSan
result in `docs/evidence/linux-tsan-stress-ci.txt`.

Be ready to explain why the queues use mutexes and why this is not advertised
as a lock-free scheduler. Describe the timed-wait sanitizer investigation using
the recorded follow-up, including that no warnings were suppressed.

## 1:30–2:20 — Show a measured algorithm tradeoff

Display `docs/evidence/algorithm-comparison.png`, then the raw CSV. Explain that
the 10,000 static-box experiment tested 128 rays per sample: approximately
21.901 ms brute force versus 0.367 ms BVH, with a separate 2.629 ms rebuild cost.
These are query measurements on one machine, not an engine FPS improvement.
Show the editor selection integration and the randomized reference comparisons
in `tests/systems_tests.cpp`.

Open `windows-main.tracy` and `windows-worker.tracy` in Tracy 0.11.1. Explain why
the optional worker physics path remains off: the small measured workload did
not show a speedup. Show the frame chart's maximum as well as its mean and p99;
the rare scene-load stall matters even though p99 misses it in this short sample.
The maximum is listed in the evidence README table alongside the plotted values.

## 2:20–3:15 — Demonstrate the networking lab

Run the shipped command, adding `.exe` on Windows if needed:

```sh
./netcode_demo --ticks 600 --latency 75 --jitter 20 --loss 0.05 --csv network.csv
```

Explain the two actual loopback UDP sockets, explicit packet format, prediction,
acknowledgements, replay after reconciliation, and delayed snapshot interpolation.
Show the CSV and final error. The configured delay is per direction and the
timeline is virtual. This is a reproducible netcode experiment; it does not add
multiplayer shooting or measure an Internet connection. Explain one next step,
such as a server-owned simulation clock and session protocol.

## 3:15–4:00 — Show reproducibility and next work

Open the GitHub Actions run associated with the release. Show separate desktop,
AddressSanitizer, ThreadSanitizer and benchmark jobs. Download a build, extract it
to a new folder, and launch CourseMaker with its assets beside it. Show
`build-manifest.json` and explain its commit and file hashes.

Finish with the real limitations and one concrete next experiment: representative
physics-heavy scenes, more complete asset streaming, or multiplayer integration.
Report the Arch ThinkCentre result only after running `bash tools/verify-linux.sh`
there; Ubuntu CI is separate evidence.
