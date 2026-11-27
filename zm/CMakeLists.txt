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
)

if(WIN32)
    add_executable(bo1zm WIN32 ${BO1ZM_SOURCES})
else()
    add_executable(bo1zm ${BO1ZM_SOURCES})
endif()

rexglue_setup_target(bo1zm)
