# bo1zm - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(bo1zm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(BO1ZM_SOURCES
    src/main.cpp
    src/bo1zm_local_players.cpp
)

if(WIN32)
    add_executable(bo1zm WIN32 ${BO1ZM_SOURCES})
else()
    add_executable(bo1zm ${BO1ZM_SOURCES})
endif()

rexglue_setup_target(bo1zm GPU_PLUGINS xenos)

# rex_app.cpp (compiled into this target) includes imgui.h, but rexruntime links imgui privately, so the include
# path does not reach us when the SDK is a subdirectory (as in bo2-360-nx).
if(TARGET imgui)
    target_include_directories(bo1zm PRIVATE $<TARGET_PROPERTY:imgui,INTERFACE_INCLUDE_DIRECTORIES>)
endif()
if(WIN32)
    target_link_options(bo1zm PRIVATE "LINKER:/errorlimit:0")  # every missing symbol in one link
endif()
