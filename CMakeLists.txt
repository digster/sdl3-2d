# CMakeLists.txt — ALTERNATIVE build path (the Makefile is the primary one).
#
# Use this if you want IDE integration (CLion/VS Code/Xcode) or plan to grow
# this into a larger, multi-platform project. It finds SDL3 via the CMake
# package config Homebrew installs at /opt/homebrew/lib/cmake/SDL3.
#
#   cmake -B build
#   cmake --build build
#   ./build/demo_traditional   ./build/demo_callbacks
#   ./build/demo_gpu           ./build/demo_gpu_shadercross    (the last only
#                                                               if SDL_shadercross
#                                                               is installed)
#
# The demos are FULLY INDEPENDENT: each executable is built only from its own
# folder (examples/c, examples/cpp, examples/cpp-gpu, examples/cpp-gpu-shadercross),
# with that folder on its private include path. There is no shared gfx library
# and no cross-linking. The shadercross target is optional — see the find_package
# block at the bottom.

cmake_minimum_required(VERSION 3.16)

# C for the C example, CXX for the C++ example — two separate languages, two
# separate targets, nothing linked across the boundary.
project(sdl3_2d LANGUAGES C CXX)

# Emit build/<dir>/compile_commands.json so editor language servers (clangd,
# VS Code C/C++) parse files with the SAME SDL3 include paths and flags the
# real build uses. Without this the editor parses each file with no flags,
# can't find <SDL3/SDL.h>, and floods the file with phantom "unknown type"
# errors even though the build itself is fine.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# --- C example — traditional hand-written loop. Compiles its own gfx.c. ---
add_executable(demo_traditional
    examples/c/traditional.c
    examples/c/gfx.c)
target_include_directories(demo_traditional PRIVATE examples/c)
target_link_libraries(demo_traditional PRIVATE SDL3::SDL3)
target_compile_options(demo_traditional PRIVATE -Wall -Wextra)

# --- C++ example — SDL3 callback model. Compiles its own gfx.cpp. Linking
#     SDL3::SDL3 also wires up the SDL_main entry-point shim, which on macOS is
#     what satisfies Cocoa's requirement to start from its own main without you
#     touching NSApplication. ---
add_executable(demo_callbacks
    examples/cpp/callbacks.cpp
    examples/cpp/gfx.cpp)
target_include_directories(demo_callbacks PRIVATE examples/cpp)
target_link_libraries(demo_callbacks PRIVATE SDL3::SDL3)
target_compile_options(demo_callbacks PRIVATE -Wall -Wextra)

# --- C++ SDL_GPU example — a basic graphics pipeline (the "hello triangle")
#     on SDL3's GPU API, which abstracts Metal/Vulkan/D3D12. Single self-
#     contained translation unit: shaders are embedded MSL strings, so there
#     is no gfx and nothing shared with the other examples. Its own private
#     include path keeps the cpp-gpu folder isolated from cpp. ---
add_executable(demo_gpu
    examples/cpp-gpu/triangle.cpp)
target_include_directories(demo_gpu PRIVATE examples/cpp-gpu)
target_link_libraries(demo_gpu PRIVATE SDL3::SDL3)
target_compile_options(demo_gpu PRIVATE -Wall -Wextra)

# --- C++ SDL_GPU + SDL_shadercross example — cross-platform shaders authored
#     ONCE in HLSL and runtime-translated to whatever the GPU backend wants
#     (SPIR-V on Vulkan, MSL on Metal, DXIL on D3D12). This target is the only
#     one in the project with a dependency beyond SDL3 itself, so it is
#     OPTIONAL: detected via find_package and silently skipped if missing,
#     keeping the other three demos building cleanly on a vanilla
#     `brew install sdl3` machine. See examples/cpp-gpu-shadercross/README.md
#     for shadercross build instructions on macOS. ---
find_package(SDL3_shadercross CONFIG QUIET)
if(SDL3_shadercross_FOUND)
    add_executable(demo_gpu_shadercross
        examples/cpp-gpu-shadercross/triangle_shadercross.cpp)
    target_include_directories(demo_gpu_shadercross
        PRIVATE examples/cpp-gpu-shadercross)
    # Upstream may export either the shared or the namespaced umbrella target
    # depending on how shadercross was configured (-DSDLSHADERCROSS_SHARED /
    # -DSDLSHADERCROSS_STATIC). Prefer the shared variant, fall back to either.
    if(TARGET SDL3_shadercross::SDL3_shadercross-shared)
        target_link_libraries(demo_gpu_shadercross
            PRIVATE SDL3::SDL3 SDL3_shadercross::SDL3_shadercross-shared)
    elseif(TARGET SDL3_shadercross::SDL3_shadercross-static)
        target_link_libraries(demo_gpu_shadercross
            PRIVATE SDL3::SDL3 SDL3_shadercross::SDL3_shadercross-static)
    else()
        target_link_libraries(demo_gpu_shadercross
            PRIVATE SDL3::SDL3 SDL3_shadercross::SDL3_shadercross)
    endif()
    target_compile_options(demo_gpu_shadercross PRIVATE -Wall -Wextra)
else()
    message(STATUS
        "SDL_shadercross not found — skipping demo_gpu_shadercross. "
        "See examples/cpp-gpu-shadercross/README.md to build it from source.")
endif()
