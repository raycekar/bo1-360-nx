#!/usr/bin/env bash
# Builds the ReXGlue host tools (rexglue codegen + xexdump) on Linux x86-64.
# Toolchain: LLVM 20, CMake 4.x, Ninja (BO1_TOOLS points at a folder with llvm-20.1.8/, cmake-4.5.1/, ninja-1.13.1/).
# Build deps (Debian/Ubuntu): pkg-config libgtk-3-dev libx11-xcb-dev libxss-dev libwayland-dev libwayland-bin
#   wayland-protocols libxkbcommon-dev libdecor-0-dev libasound2-dev libpulse-dev libpipewire-0.3-dev libvulkan-dev
# Output: sdk/out/linux-amd64/rexglue and $OUT/switch_bo2/xexdump (run with LD_LIBRARY_PATH=sdk/out/linux-amd64).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
T="${BO1_TOOLS:-/mnt/workspace/tools}"
OUT="${BO1_HOST_OUT:-$ROOT/../out/host}"
export PATH="$T/llvm-20.1.8/bin:$T/cmake-4.5.1/bin:$T/ninja-1.13.1:$PATH"
[ -d "$ROOT/sdk/thirdparty/cli11/include" ] || python3 "$ROOT/tools/fetch_thirdparty.py"
for d in "$ROOT"/sdk/thirdparty/*/; do [ -e "$d.git" ] || mkdir "$d.git"; done
cmake -S "$ROOT/sdk" -B "$OUT" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2 \
  -DGIT_EXECUTABLE=/nonexistent/git -DREXGLUE_ENABLE_TRACY=OFF \
  "-DCMAKE_PROJECT_ReXGlue_INCLUDE=$ROOT/cmake/host_tools_inject.cmake"
cmake --build "$OUT" --target rexglue xexdump -j "${JOBS:-2}"
