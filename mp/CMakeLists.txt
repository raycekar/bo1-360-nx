# bo1mp - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(bo1mp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(BO1MP_SOURCES
    src/main.cpp
    src/bo1mp_local_players.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/bo1_online.cpp
)

if(WIN32)
    add_executable(bo1mp WIN32 ${BO1MP_SOURCES})
else()
    add_executable(bo1mp ${BO1MP_SOURCES})
endif()

rexglue_setup_target(bo1mp GPU_PLUGINS xenos)

# rex_app.cpp includes imgui.h, but rexruntime links imgui privately (see zm/CMakeLists.txt).
target_include_directories(bo1mp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common)
if(TARGET imgui)
    target_include_directories(bo1mp PRIVATE $<TARGET_PROPERTY:imgui,INTERFACE_INCLUDE_DIRECTORIES>)
endif()
if(WIN32)
    target_link_options(bo1mp PRIVATE "LINKER:/errorlimit:0")
    # Symbol map for crash triage (tools/crash_triage.py): native RVA -> sub_XXXXXXXX. Not shipped.
    target_link_options(bo1mp PRIVATE "LINKER:/map:${CMAKE_BINARY_DIR}/bo1mp.map")
    # PDB with line tables of the generated code (guest instruction of a fault). /debug drops the default
    # /opt:ref,icf, so keep them on. The PDB stays in the build folder.
    target_link_options(bo1mp PRIVATE "LINKER:/debug" "LINKER:/opt:ref" "LINKER:/opt:icf")
endif()
