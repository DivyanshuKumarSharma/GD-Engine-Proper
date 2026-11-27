#!/usr/bin/env bash
# Run from a checkout on the Arch ThinkCentre. Installs nothing and changes no
# machine settings. Required: cmake, ninja, gcc, python; GUI deps listed in docs.
set -euo pipefail
cd "$(dirname "$0")/.."
for sanitizer in address thread; do
  dir="build-linux-$sanitizer"
  cmake -S . -B "$dir" -G Ninja -DGD_SYSTEMS_ONLY=ON -DGD_SANITIZER="$sanitizer" -DCMAKE_BUILD_TYPE=Debug
  cmake --build "$dir" --parallel 2
  TSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ctest --test-dir "$dir" --output-on-failure --timeout 90
done
cmake -S . -B build-linux-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DGLFW_BUILD_WAYLAND=OFF -DBUILD_BULLET3=OFF -DBUILD_CPU_DEMOS=OFF -DBUILD_OPENGL3_DEMOS=OFF
cmake --build build-linux-release --parallel 2
ctest --test-dir build-linux-release --output-on-failure --timeout 90
python tools/package_build.py --build build-linux-release --output dist
