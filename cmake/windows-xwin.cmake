# Cross-compile for Windows x64 from Linux: LLVM clang (MSVC target) + lld-link, with the MSVC CRT and Windows SDK
# laid out by xwin (`xwin --accept-license splat --output <dir>`).
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/windows-xwin.cmake -DBO1_LLVM=<llvm dir> -DBO1_XWIN=<splat dir> ...
# Defaults: /mnt/workspace/tools/llvm-20.1.8 and /mnt/workspace/tools/xwin-sdk-0.10.0.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

if(NOT BO1_LLVM)
  set(BO1_LLVM "$ENV{BO1_LLVM}")
endif()
if(NOT BO1_LLVM)
  set(BO1_LLVM "/mnt/workspace/tools/llvm-20.1.8")
endif()
if(NOT BO1_XWIN)
  set(BO1_XWIN "$ENV{BO1_XWIN}")
endif()
if(NOT BO1_XWIN)
  set(BO1_XWIN "/mnt/workspace/tools/xwin-sdk-0.10.0")
endif()
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES BO1_LLVM BO1_XWIN)

set(_triple x86_64-pc-windows-msvc)
# On a Windows host the tools carry .exe.
set(_exe "")
if(CMAKE_HOST_WIN32)
  set(_exe ".exe")
endif()
set(CMAKE_C_COMPILER "${BO1_LLVM}/bin/clang${_exe}")
set(CMAKE_CXX_COMPILER "${BO1_LLVM}/bin/clang++${_exe}")
set(CMAKE_C_COMPILER_TARGET ${_triple})
set(CMAKE_CXX_COMPILER_TARGET ${_triple})
set(CMAKE_ASM_COMPILER "${BO1_LLVM}/bin/clang${_exe}")
set(CMAKE_ASM_COMPILER_TARGET ${_triple})
set(CMAKE_RC_COMPILER "${BO1_LLVM}/bin/llvm-rc${_exe}")
set(CMAKE_AR "${BO1_LLVM}/bin/llvm-ar${_exe}")
set(CMAKE_RANLIB "${BO1_LLVM}/bin/llvm-ranlib${_exe}")
set(CMAKE_LINKER "${BO1_LLVM}/bin/lld-link${_exe}")
set(CMAKE_MT "${BO1_LLVM}/bin/llvm-mt${_exe}")

# System headers from the splat, searched AFTER clang's own builtin headers (stddef.h, *intrin.h), as clang-cl
# orders /imsvc paths: -internal-isystem via -Xclang (plain -isystem would come first and shadow them).
set(_inc "")
foreach(d crt/include sdk/include/ucrt sdk/include/um sdk/include/shared sdk/include/winrt sdk/include/cppwinrt)
  if(EXISTS "${BO1_XWIN}/${d}")
    string(APPEND _inc " -Xclang -internal-isystem -Xclang \"${BO1_XWIN}/${d}\"")
  endif()
endforeach()
# CPU level of the PC build: x86-64-v2 (SSE4.2, default) or x86-64-v3 (AVX2, Intel Haswell / AMD Zen and newer), from
# the environment variable BO1_MARCH. It is read when a build folder is configured first: use a new folder to change it.
set(_bo1_march "x86-64-v2")
if(DEFINED ENV{BO1_MARCH} AND NOT "$ENV{BO1_MARCH}" STREQUAL "")
  set(_bo1_march "$ENV{BO1_MARCH}")
endif()
# BO1_DEBUG_LINES=1: line tables (CodeView) everywhere and PDBs for the DLLs too, so profilers can name native code.
set(_dbg "")
set(_dbg_link "")
if(DEFINED ENV{BO1_DEBUG_LINES} AND NOT "$ENV{BO1_DEBUG_LINES}" STREQUAL "")
  set(_dbg "-gline-tables-only -gcodeview")
  set(_dbg_link " -Wl,/debug")
endif()
set(CMAKE_C_FLAGS_INIT "-fuse-ld=lld -march=${_bo1_march} ${_dbg} ${_inc}")
set(CMAKE_CXX_FLAGS_INIT "-fuse-ld=lld -march=${_bo1_march} ${_dbg} ${_inc}")
set(CMAKE_RC_FLAGS_INIT "-I \"${BO1_XWIN}/sdk/include/um\" -I \"${BO1_XWIN}/sdk/include/shared\"")

set(_lib "-L\"${BO1_XWIN}/crt/lib/x86_64\" -L\"${BO1_XWIN}/sdk/lib/um/x86_64\" -L\"${BO1_XWIN}/sdk/lib/ucrt/x86_64\"")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_lib} -Wl,/manifest:no")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_lib} -Wl,/manifest:no${_dbg_link}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_lib} -Wl,/manifest:no")

set(CMAKE_FIND_ROOT_PATH "${BO1_XWIN}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
