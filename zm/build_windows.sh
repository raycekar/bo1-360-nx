#!/usr/bin/env bash
# Cross-builds bo1zm.exe (BO1 Zombies, TU11 default.xex) for Windows x64 on Linux: LLVM clang + lld-link with the
# xwin MSVC CRT / Windows SDK splat (cmake/windows-xwin.cmake). Needs generated/default from codegen.sh.
# Output: $OUT/bo1zm.exe (default ../../out/build-win-zm). JOBS (default 1) parallel compiles.
set -euo pipefail
ZM="$(cd "$(dirname "$0")" && pwd)"
ROOT="$ZM/.."
T="${BO1_TOOLS:-/mnt/workspace/tools}"
OUT="${BO1_WIN_OUT:-$ROOT/../out/build-win-zm}"
export PATH="$T/cmake-4.5.1/bin:$T/ninja-1.13.1:$PATH"
[ -f "$ZM/generated/default/sources.cmake" ] || { echo "run zm/codegen.sh first" >&2; exit 1; }
for d in "$ROOT"/sdk/thirdparty/*/; do [ -e "$d.git" ] || mkdir "$d.git"; done
cmake -S "$ZM" -B "$OUT" -G Ninja \
  "-DCMAKE_TOOLCHAIN_FILE=$ROOT/cmake/windows-xwin.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  -DREXGLUE_BUILD_HOST_TOOLS=OFF -DREXGLUE_BUILD_TESTS=OFF -DREXGLUE_ENABLE_TRACY=OFF \
  -DREXGLUE_USE_VULKAN=ON -DREXGLUE_RECOMP_DEBUG_INFO=line-tables-only \
  -DGIT_EXECUTABLE=/nonexistent/git "-DREXGLUE_OUTPUT_DIR=$OUT/sdk-out" \
  "-DREXSDK_DIR=$ROOT/sdk"
# Low priority + limits (tools/lowprio.sh: nice 19, idle I/O, CPUQuota, MemoryMax) so the build cannot overload the host.
"$ROOT/tools/lowprio.sh" cmake --build "$OUT" --target bo1zm -j "${JOBS:-1}"
