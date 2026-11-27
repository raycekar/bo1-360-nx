#!/usr/bin/env bash
# Cross-builds bo1mp.exe (BO1 Multiplayer, TU11 default_mp.xex) for Windows x64 on Linux: LLVM clang + lld-link with the
# xwin MSVC CRT / Windows SDK splat (cmake/windows-xwin.cmake). Needs generated/default_mp from codegen.sh.
# Output: $OUT/bo1mp.exe (default ../../out/build-win-mp). JOBS (default 2) parallel compiles.
set -euo pipefail
MP="$(cd "$(dirname "$0")" && pwd)"
ROOT="$MP/.."
T="${BO1_TOOLS:-/mnt/workspace/tools}"
OUT="${BO1_WIN_OUT:-$ROOT/../out/build-win-mp}"
export PATH="$T/cmake-4.5.1/bin:$T/ninja-1.13.1:$PATH"
[ -f "$MP/generated/default_mp/sources.cmake" ] || { echo "run mp/codegen.sh first" >&2; exit 1; }
for d in "$ROOT"/sdk/thirdparty/*/; do [ -e "$d.git" ] || mkdir "$d.git"; done
cmake -S "$MP" -B "$OUT" -G Ninja \
  "-DCMAKE_TOOLCHAIN_FILE=$ROOT/cmake/windows-xwin.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DREXGLUE_BUILD_HOST_TOOLS=OFF -DREXGLUE_BUILD_TESTS=OFF -DREXGLUE_ENABLE_TRACY=OFF \
  -DREXGLUE_USE_VULKAN=ON -DREXGLUE_RECOMP_DEBUG_INFO=none \
  -DGIT_EXECUTABLE=/nonexistent/git "-DREXGLUE_OUTPUT_DIR=$OUT/sdk-out" \
  "-DREXSDK_DIR=$ROOT/sdk"
cmake --build "$OUT" --target bo1mp -j "${JOBS:-2}"
