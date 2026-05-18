# CMakeLists.txt — ALTERNATIVE build path (the Makefile is the primary one).
#
# Use this if you want IDE integration (CLion/VS Code/Xcode) or plan to grow
# this into a larger, multi-platform project. It finds SDL3 via the CMake
# package config Homebrew installs at /opt/homebrew/lib/cmake/SDL3.
#
#   cmake -B build
#   cmake --build build
#   ./build/demo_traditional      ./build/demo_callbacks
#
# Produces the same two binaries as `make`.

cmake_minimum_required(VERSION 3.16)

# Declaring BOTH C and CXX is what lets one project compile gfx.c as C while
# building the C++ example with the C++ compiler.
project(sdl3_2d LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to a debuggable build if the user didn't pick a config.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# CONFIG mode = use SDL3's installed SDL3Config.cmake (from `brew install sdl3`).
find_package(SDL3 REQUIRED CONFIG)

# Shared draw-helper library, built from the C source.
add_library(gfx STATIC src/gfx.c)
target_include_directories(gfx PUBLIC include)
# PUBLIC so anything linking gfx also gets SDL3's include path/flags.
target_link_libraries(gfx PUBLIC SDL3::SDL3)
target_compile_options(gfx PRIVATE -Wall -Wextra)

# C example — traditional hand-written loop.
add_executable(demo_traditional examples/traditional.c)
target_link_libraries(demo_traditional PRIVATE gfx SDL3::SDL3)
target_compile_options(demo_traditional PRIVATE -Wall -Wextra)

# C++ example — SDL3 callback model. Linking SDL3::SDL3 also wires up the
# SDL_main entry-point shim, which on macOS is what satisfies Cocoa's
# requirement to start from its own main without you touching NSApplication.
add_executable(demo_callbacks examples/callbacks.cpp)
target_link_libraries(demo_callbacks PRIVATE gfx SDL3::SDL3)
target_compile_options(demo_callbacks PRIVATE -Wall -Wextra)
