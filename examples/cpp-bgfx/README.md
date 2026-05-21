# `examples/cpp-bgfx/` — SDL3 + bgfx

Two C++ demos in one folder, both using [bgfx](https://github.com/bkaradzic/bgfx)
— a mature, BSD-2-licensed rendering library — as the GPU layer behind an
SDL3 window. Same scene as the [`cpp-gpu/`](../cpp-gpu/) example, rendered
through a different GPU abstraction so you can diff the two and see the
trade-offs side by side.

> **One of two optional examples in this repo.** The four core demos in
> `examples/{c,cpp,cpp-gpu}/` build on just `brew install sdl3`. bgfx is not
> in Homebrew, so it has to be built from source (steps below). The top-level
> Makefile and CMake **detect it optionally** and skip the bgfx targets with
> a friendly message when it is absent, so the other examples keep their
> stand-alone build guarantee.

## Contents

```
hello_bgfx.cpp         The wiring demo — SDL3↔bgfx handoff, clear, debug-text
                       overlay. No shaders, no buffers. ~225 lines.
triangle_bgfx.cpp      The full hello-triangle — vertex/index buffers + offline-
                       compiled shaders + per-frame submit. ~350 lines.
shaders/
  varying.def.sc       bgfx's central registry (attribute names → semantics).
  vs_triangle.sc       Vertex shader source — bgfx .sc dialect.
  fs_triangle.sc       Fragment shader source.
  README.md            Notes on the .sc files (including a comment-gotcha).
  build/               (gitignored) Cross-compiled per-renderer bytecode.
    metal/             Metal bytecode produced by `make bgfx-shaders` on macOS.
```

## Two demos, intentionally graduated

| Demo | What it teaches | Toolchain it needs |
|---|---|---|
| **`demo_bgfx`** (`hello_bgfx.cpp`) | The SDL3↔bgfx wiring — native window handoff, single-threaded mode opt-in, init, per-frame `setViewClear` + `touch` + `dbgTextPrintf` + `frame`. The new things. | bgfx headers + libs only. |
| **`demo_bgfx_triangle`** (`triangle_bgfx.cpp`) | Same wiring, plus `bgfx::VertexLayout`, vertex/index buffers via `bgfx::makeRef`, offline-compiled shader binaries loaded via `bgfx::createShader` / `bgfx::createProgram`, full submit cycle. | bgfx + the `shaderc` CLI (built as part of bgfx.cmake). |

Read `hello_bgfx.cpp` first; the triangle file's first half (init through view
clear) is intentionally identical so the additions on top are obvious.

## Building bgfx.cmake on macOS

bgfx's official CMake wrapper repo, [bgfx.cmake](https://github.com/bkaradzic/bgfx.cmake),
bundles bgfx + bimg + bx + the `shaderc` shader compiler into one CMake
project. We point our build at it via `BGFX_PREFIX` (default:
`third_party/bgfx-install/install`).

```sh
# a. Clone bgfx.cmake with submodules. The submodules are bgfx/, bimg/, bx/
#    — the upstream source trees this repo wraps.
git clone https://github.com/bkaradzic/bgfx.cmake \
    third_party/bgfx-install --recursive

# b. Configure & build inside the source tree (separate build subdir).
#    BGFX_BUILD_TOOLS_SHADER=ON pulls in shaderc and its dependencies
#    (glslang, spirv-cross, spirv-opt, fcpp, glsl-optimizer). BGFX_INSTALL=ON
#    enables the install() rules used in step (c).
cmake -S third_party/bgfx-install \
      -B third_party/bgfx-install/cmake-build \
      -DBGFX_BUILD_TOOLS=ON \
      -DBGFX_BUILD_TOOLS_SHADER=ON \
      -DBGFX_INSTALL=ON \
      -DCMAKE_INSTALL_PREFIX=$(pwd)/third_party/bgfx-install/install
cmake --build third_party/bgfx-install/cmake-build -j

# c. Install into the local prefix. This populates
#    third_party/bgfx-install/install/{include,lib,bin}/ and the
#    lib/cmake/bgfx/ package config that the top-level CMakeLists.txt's
#    find_package(bgfx CONFIG) picks up.
cmake --install third_party/bgfx-install/cmake-build

# d. Round out the install with the .sc shader common headers
#    (bgfx_shader.sh, bgfx_compute.sh). bgfx.cmake's install rules do NOT
#    ship these — an upstream omission. Copying them into share/bgfx/shaders/
#    makes the installed prefix self-contained, so `make bgfx-shaders` can
#    find them via the canonical `-i $(BGFX_PREFIX)/share/bgfx/shaders` path.
mkdir -p third_party/bgfx-install/install/share/bgfx/shaders
cp third_party/bgfx-install/bgfx/src/bgfx_shader.sh \
   third_party/bgfx-install/bgfx/src/bgfx_compute.sh \
   third_party/bgfx-install/install/share/bgfx/shaders/
```

**After step (d)** the source tree is no longer needed at runtime — you can
delete `third_party/bgfx-install/{bgfx,bx,bimg,cmake,cmake-build}/` to reclaim
~500 MB. `install/` and its `share/bgfx/shaders/` headers are enough for both
demos to build and for `make bgfx-shaders` to compile new shaders.

Skip step (d) if you want to keep the source tree around as the source of
truth — the Makefile's detection falls back to it automatically.

### Debug builds

To get bgfx symbols in lldb, add `-DCMAKE_BUILD_TYPE=Debug` to the configure
step. Library names then become `libbgfxDebug.a` / `libbxDebug.a` /
`libbimgDebug.a`; the detection in the top-level Makefile is currently wired
to the unsuffixed names. If you go debug, override `BGFX_LIBS` on the
command line or symlink the debug archives.

## `BGFX_PREFIX` — non-default install locations

Both the Makefile and CMake honour `BGFX_PREFIX` for the install location:

```sh
# Build with a different prefix (Makefile)
make BGFX_PREFIX=/path/to/bgfx-install run-bgfx

# Same for CMake — pass via -D or set as an env var
BGFX_PREFIX=/path/to/bgfx-install cmake -B build
```

Detection is two-shape: it first probes the canonical **installed** layout
(`$PFX/include/bgfx/bgfx.h`), then falls back to the **in-tree-build** layout
(`$PFX/bgfx/include/bgfx/bgfx.h`) — so users who skip the `cmake --install`
step still get a working build pointing at `bgfx.cmake`'s source root.

## Running

```sh
make bgfx-shaders             # compile shaders/*.sc into shaders/build/metal/
make run-bgfx                 # window with cleared bg + "renderer: Metal" overlay
make run-bgfx-triangle        # red/green/blue gradient triangle
make debug-bgfx               # rebuild -g -O0, drop into lldb
make debug-bgfx-triangle      # ditto for the triangle
./build/demo_bgfx          --frames 120   # headless smoke (no display needed)
./build/demo_bgfx_triangle --frames 120
```

## Cross-platform shaders

bgfx shader binaries are **renderer-specific** — a Metal binary won't run on
Vulkan or D3D. On macOS we only compile for Metal because that's the only
backend bgfx will select. To re-target other backends, re-run `shaderc` with
a different `-p` flag (see `shaders/README.md`):

```sh
PFX=third_party/bgfx-install/install
out=examples/cpp-bgfx/shaders/build
mkdir -p $out/spirv $out/d3d11

# SPIR-V (Vulkan, also bgfx's intermediate format)
$PFX/bin/shaderc -f examples/cpp-bgfx/shaders/vs_triangle.sc \
    --varyingdef examples/cpp-bgfx/shaders/varying.def.sc \
    -i $PFX/share/bgfx/shaders \
    --platform linux -p spirv --type vertex \
    -o $out/spirv/vs_triangle.bin

# DXBC (D3D11)
$PFX/bin/shaderc -f examples/cpp-bgfx/shaders/vs_triangle.sc \
    --varyingdef examples/cpp-bgfx/shaders/varying.def.sc \
    -i $PFX/share/bgfx/shaders \
    --platform windows -p s_5_0 --type vertex \
    -o $out/d3d11/vs_triangle.bin
```

To actually USE those on the right machine you'd extend `load_shader_blob()`
in `triangle_bgfx.cpp` to choose the right subdirectory based on
`bgfx::getRendererType()`. Left as an exercise — the single-platform default
keeps this example focused on the wiring.

## Why bgfx alongside SDL_GPU?

SDL_GPU and bgfx solve the same problem in two different idioms. Both
abstract Metal/Vulkan/D3D12 behind a portable API; both let you write one
shader and dispatch it to any backend. They differ in design centre:

|  | SDL_GPU (this repo's [`cpp-gpu/`](../cpp-gpu/)) | bgfx (this folder) |
|---|---|---|
| **Lifecycle** | Explicit: command buffer → render pass → bind pipeline → draw → submit. You build a `SDL_GPUGraphicsPipeline` once. | View-based: `setViewClear`, `submit(view, program)`, `frame()`. bgfx schedules and batches under the hood. |
| **Shader pipeline** | You pick a shader format SDL_GPU accepts (MSL, SPIR-V, DXIL) — [`cpp-gpu-shadercross/`](../cpp-gpu-shadercross/) handles that portably via HLSL→SPIR-V→native. | bgfx ships its own `.sc` dialect + `shaderc` compiler. One source, per-backend bytecode. |
| **Threading** | Main-thread, deterministic. | Multi-threaded by default with an explicit single-threaded opt-in (`bgfx::renderFrame()` before `init`). |
| **Maturity** | New (SDL3-era). | ~12 years; powers a long list of shipped games. |
| **Dependencies** | Just SDL3. | bgfx + bimg + bx + glslang + spirv-cross. |
| **Footprint** | ~35 KB per demo binary. | ~3 MB per demo binary (static-linked bgfx). |

Both are interesting. Real projects pick on taste, team familiarity, and
whether they already have bgfx shader tooling in their pipeline. The two
examples in this repo render the same triangle so you can read both files
back-to-back and see exactly what each idiom asks of you.
