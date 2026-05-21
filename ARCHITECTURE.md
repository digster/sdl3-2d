# Architecture

This document is the "big picture" — read it before diving into files.

## Goal

A minimal, reusable SDL3 starter for macOS shipped as **five fully
independent examples**, each isolating exactly one thing to learn:

- **`examples/c/`** — C, a hand-written `while` game loop, 2D renderer.
- **`examples/cpp/`** — C++, the SDL3 callback loop, 2D renderer.
- **`examples/cpp-gpu/`** — C++, the lower-level `SDL_GPU` API: a basic
  graphics pipeline that abstracts Metal/Vulkan/D3D12. Shaders are embedded
  MSL source — macOS-only by design, no extra tooling.
- **`examples/cpp-gpu-shadercross/`** — C++, the same pipeline but with
  **cross-platform shaders**: one HLSL source compiled to SPIR-V at runtime
  (or build time) via `SDL_shadercross`, then dispatched to whatever the
  device's backend natively wants (MSL/SPIRV/DXIL).
- **`examples/cpp-bgfx/`** — C++, the *same hello-triangle* as `cpp-gpu/`
  rendered through [bgfx](https://github.com/bkaradzic/bgfx) instead of
  SDL_GPU. Two binaries from one folder: a clear-and-debug-text wiring
  demo (`hello_bgfx.cpp`) and the full triangle (`triangle_bgfx.cpp`) with
  `.sc` shaders compiled offline by bgfx's `shaderc`.

It is intentionally tiny: a base to copy into future experiments, not a
framework. The first two demonstrate the two SDL3 game-loop styles on the
high-level 2D renderer; the third drops a tier to show the GPU-pipeline
machinery (it does not use the 2D renderer or `gfx` at all — the two APIs are
mutually exclusive); the fourth keeps the same render pipeline but swaps the
shader-acquisition path for the portable one a real cross-platform engine
ships with; the fifth swaps the *entire GPU layer* — SDL_GPU out, bgfx in —
so the SDL3↔bgfx integration and bgfx's distinctly different lifecycle
(views, no command buffers, its own shader pipeline) can be compared
head-to-head with the SDL_GPU triangle.

The defining decision: **the examples share no source.** Any folder can be
copied out on its own and built with nothing else present. The ~135 lines of
drawing helpers are *deliberately duplicated* between `c/` and `cpp/` (once in
C, once in idiomatic C++); the three GPU-tier examples (`cpp-gpu/`,
`cpp-gpu-shadercross/`, `cpp-bgfx/`) are each self-contained — single-file
where possible, two-file in `cpp-bgfx/` because the wiring is taught first.
Independence — not code reuse — is the property this template teaches.

## Component map

```
examples/c/             examples/cpp/           examples/cpp-gpu/     examples/cpp-gpu-shadercross/    examples/cpp-bgfx/
  gfx.h (C API + ref)     gfx.hpp (C++ API+ref)   triangle.cpp           triangle_shadercross.cpp         hello_bgfx.cpp + triangle_bgfx.cpp
  gfx.c (compiled as C)   gfx.cpp (as C++)          embedded MSL           embedded HLSL strings +          shaders/*.sc compiled offline
  traditional.c           callbacks.cpp             shaders;               shaders/*.hlsl files;            via bgfx's shaderc;
  (main()+while loop)     (SDL drives SDL_App*)     no gfx                 no gfx                           no gfx; bgfx replaces SDL_GPU
        │                       │                         │                         │                              │
        ▼                       ▼                         ▼                         ▼                              ▼
  build/demo_traditional  build/demo_callbacks      build/demo_gpu          build/demo_gpu_shadercross     build/demo_bgfx + demo_bgfx_triangle
  (2D renderer)           (2D renderer)             (SDL_GPU pipeline)      (SDL_GPU + SDL_shadercross,    (bgfx pipeline, OPTIONAL —
                                                                             OPTIONAL — skipped cleanly     skipped cleanly if bgfx not
                                                                             if shadercross not installed)  found at BGFX_PREFIX)
```

There is **no arrow between the columns.** `examples/c/` is compiled and
linked entirely by the C compiler; the four C++ folders entirely by the C++
compiler, each with its own private include path so no folder can see another.
The only thing the five share is that all call the same plain-C **SDL3**
library — which is exactly why each scene can be expressed with no shared
code. The three GPU-tier examples notably do **not** include `gfx`: they use
`SDL_GPU` or bgfx, a different rendering subsystem from the 2D renderer the
other two use. The shadercross and bgfx examples each additionally depend on
their own off-Homebrew library — those two links are the **two exceptions**
to the brew-only-deps rule (see *conventions* below); both are optional and
independently detected.

> Contrast with C's prefix convention: the C copy disambiguates names with a
> `gfx_` prefix (`gfx_circle`); the C++ copy uses a `gfx` **namespace**
> (`gfx::circle`) — each language's native way to scope an API. This pairing is
> itself part of what the template demonstrates.

## Data / control flow

The two **2D-renderer** examples follow the same conceptual lifecycle; only
*who drives it* differs:

```
init SDL ─► create window+renderer ─► enable vsync
   ─► [ pump events ─► update with dt ─► clear ─► draw ─► present ] (repeat)
   ─► destroy renderer/window ─► SDL_Quit
```

- **`examples/c/traditional.c`** owns `main()` and the `while (running)` loop
  explicitly. You can see every step. Quitting is a `bool` flip.
- **`examples/cpp/callbacks.cpp`** defines `SDL_MAIN_USE_CALLBACKS` and
  implements `SDL_AppInit / SDL_AppEvent / SDL_AppIterate / SDL_AppQuit`. SDL
  owns the loop and calls those. Per-app state is a heap C++ `AppState` whose
  pointer SDL threads back through every callback (`appstate`) — no globals.
  Quitting is returning `SDL_APP_SUCCESS`. The callback runtime calls
  `SDL_Quit()` itself *after* `SDL_AppQuit`, so the app must destroy its own
  window/renderer but must **not** call `SDL_Quit()`.

The callback model is the SDL3-recommended one because it is what makes the
same code portable to environments that won't let you own the main loop
(Emscripten/web, iOS).

**`examples/cpp-gpu/triangle.cpp`** uses the same callback *loop* but a
different *render* lifecycle, because `SDL_GPU` records commands instead of
executing them immediately like the 2D renderer:

```
init SDL ─► create GPU device ─► create window ─► claim window (swapchain)
   ─► compile vertex+fragment shaders ─► build graphics pipeline (once)
   ─► [ acquire command buffer ─► acquire swapchain texture
        ─► (if non-NULL) begin render pass ─► bind pipeline ─► draw 3 verts
            ─► end render pass
        ─► submit command buffer ] (repeat)
   ─► release pipeline/window/device ─► destroy window
```

Two facts shape this path:

- **The pipeline is immutable and built once** in `SDL_AppInit`; each frame
  only *binds* it. A window resize does **not** rebuild it — SDL recreates the
  swapchain transparently.
- **A NULL swapchain texture is a normal outcome, not an error.**
  `SDL_WaitAndAcquireGPUSwapchainTexture` can return success yet hand back NULL
  (window minimized/occluded, or no display at all). The frame then skips the
  render pass but **still submits** the command buffer. This is what lets the
  headless `make smoke` run (`--frames 120`, no display) exit 0.

Like `callbacks.cpp`, it must **not** call `SDL_Quit()` (the
`SDL_MAIN_USE_CALLBACKS` runtime does that after `SDL_AppQuit`); it releases
its GPU objects through the device.

**`examples/cpp-gpu-shadercross/triangle_shadercross.cpp`** runs the **same
render lifecycle** as `cpp-gpu/triangle.cpp` (acquire command buffer → acquire
swapchain → render pass → submit, with identical NULL-swapchain handling), but
swaps in a different **shader-acquisition lifecycle** in front of pipeline
creation:

```
SDL_ShaderCross_Init()
  ─► (mode RUNTIME) HLSL source string ─► SDL_ShaderCross_CompileSPIRVFromHLSL
  ─► (mode SPIRV)   .spv file on disk   ─► SDL_LoadFile
  ─► SDL_ShaderCross_CompileGraphicsShaderFromSPIRV (transpiles inside to MSL /
                                                     DXIL when needed)
  ─► SDL_GPUShader * ─► SDL_CreateGPUGraphicsPipeline
                                                       …same per-frame loop…
  ─► SDL_ShaderCross_Quit() in SDL_AppQuit (paired with the Init above; called
                                            even on early-fail paths)
```

The pivotal API is `SDL_ShaderCross_CompileGraphicsShaderFromSPIRV` — it takes
SPIR-V and returns a shader in whatever format the device actually wanted, so
the rest of the file never has to branch on the GPU backend. This is what makes
*one* binary build from *one* HLSL source viable on Metal, Vulkan and D3D12.
The `--mode runtime|spirv` flag exists only to let the same example show both
where the SPIR-V can come from (embedded source compiled at startup vs.
pre-built bytecode loaded from disk).

**`examples/cpp-bgfx/`** swaps out the **entire GPU layer**. SDL3 still owns
the window, input and main loop; bgfx (not SDL_GPU) drives the GPU. The
lifecycle has a different shape than either SDL_GPU example:

```
SDL_Init ─► SDL_CreateWindow
   ─► get NSWindow* via SDL_PROP_WINDOW_COCOA_WINDOW_POINTER
   ─► bgfx::renderFrame()                    (BEFORE init — single-threaded mode)
   ─► bgfx::Init{ .platformData = {nwh, …},
                  .resolution   = {w, h, BGFX_RESET_VSYNC},
                  .type         = RendererType::Count }
   ─► bgfx::init(init)
   ─► bgfx::setViewClear(0, …)               (view 0 = backbuffer)
   ─► (triangle only) declare VertexLayout, createVertexBuffer / IndexBuffer,
                      createShader from on-disk .bin × 2, createProgram
   ─► [ on resize: bgfx::reset(w, h, BGFX_RESET_VSYNC)
        bgfx::setViewRect(0, 0,0, w,h)
        (triangle) setVertexBuffer + setIndexBuffer + setState + submit(0, prog)
        (hello)    bgfx::touch(0)            (no-op so the clear runs)
        bgfx::dbgTextPrintf(…)               (debug overlay)
        bgfx::frame() ]                      (flush & present)
   ─► (triangle) destroy(prog/vbh/ibh)
   ─► bgfx::shutdown() ─► SDL_DestroyWindow
```

Three things distinguish this from SDL_GPU:

- **No command buffers, no render-pass scope.** bgfx records draws as they
  arrive and batches them inside `bgfx::frame()`. The lifecycle is shorter,
  and view state (clear color, rect, framebuffer) is sticky across frames.
- **`bgfx::renderFrame()` before `bgfx::init()` opts into single-threaded
  mode.** Without it, bgfx spawns its own render thread that owns the GPU —
  which conflicts with SDL3's "everything on main thread" expectation on
  macOS and complicates shutdown ordering. The pre-init call is bgfx's
  documented single-threaded opt-in. Skip it at your peril.
- **Shaders go through bgfx's own pipeline.** bgfx ships a `.sc` dialect
  (extended GLSL) and a `shaderc` CLI that compiles to per-backend bytecode.
  `examples/cpp-bgfx/shaders/*.sc` → `shaders/build/metal/*.bin` (Metal on
  macOS) is the offline step, driven by `make bgfx-shaders`. The triangle
  binary loads those `.bin` blobs with `SDL_LoadFile` → `bgfx::copy` →
  `bgfx::createShader` → `bgfx::createProgram`.

The two demos in the folder are intentionally graduated: `hello_bgfx.cpp`
teaches the wiring with no shader pipeline at all (its per-frame body is
`setViewRect` + `touch` + `dbgTextPrintf` + `frame`); `triangle_bgfx.cpp`
keeps every line of that wiring identical and layers the buffer/program
machinery on top. Diff the two side-by-side to see the additions in
isolation.

## Build architecture (two paths, both keep the examples disjoint)

| | Makefile (primary) | CMake (alternative) |
|---|---|---|
| SDL3 discovery | `pkg-config --cflags/--libs sdl3` | `find_package(SDL3 CONFIG)` |
| Best for | quick CLI use, smallest setup | IDEs, larger/multi-platform growth |
| macOS entry point | `<SDL3/SDL_main.h>` include handles it | `SDL3::SDL3` target also wires `SDL_main` |

Both build systems compile each example **only from its own folder**, with
that folder as the sole project include path (`-Iexamples/c` /
`-Iexamples/cpp` / `-Iexamples/cpp-gpu` / `-Iexamples/cpp-gpu-shadercross` /
`-Iexamples/cpp-bgfx`), into separate object trees (`build/c/`, `build/cpp/`,
`build/cpp-gpu/`, `build/cpp-gpu-shadercross/`, `build/cpp-bgfx/`). There is
no shared object and no shared library — removing any one folder cannot break
the others.

The **two optional targets** (shadercross, bgfx) are independently
conditionally included:

- *shadercross* — detected via `pkg-config --exists sdl3-shadercross` (Make)
  or `find_package(SDL3_shadercross CONFIG QUIET)` (CMake).
- *bgfx* — detected via the `BGFX_PREFIX` variable (defaults to
  `third_party/bgfx-install/install`). Probes BOTH the canonical installed
  layout (`$PFX/include/bgfx/bgfx.h`) and the in-tree-build layout
  (`$PFX/bgfx/include/bgfx/bgfx.h`, for users who skip `cmake --install` on
  `bgfx.cmake`). CMake's path resolves through `find_package(bgfx CONFIG)`
  against the installed prefix, falling back to manual IMPORTED-target shims
  for the in-tree layout.

If either optional dependency is missing, the build proceeds without it and
a friendly note points at the relevant README. The three core demos (`c/`,
`cpp/`, `cpp-gpu/`) always build on a vanilla `brew install sdl3` machine.

`make smoke` and the `--frames N` argument all examples accept exist so the
build can be verified **non-interactively** (render N frames, exit 0) without
a human closing windows — useful for CI later. The SDL_GPU examples'
NULL-swapchain handling is what keeps them valid headless; the bgfx demos
get the same property for free because `bgfx::frame()` always returns whether
or not a display is attached. `smoke` runs the optional binaries too **when**
present (shadercross independently of bgfx, both gated on their own
detection). Proving independence directly: `cd examples/cpp-gpu && clang++
-I. triangle.cpp $(pkg-config --cflags --libs sdl3)` builds the GPU demo
with the other folders literally invisible (and likewise for `c/`, `cpp/`,
`cpp-gpu-shadercross/` — the last also needs `pkg-config --cflags --libs
sdl3-shadercross`, and `cpp-bgfx/` needs the equivalent `-I$BGFX_PREFIX/include
-L$BGFX_PREFIX/lib -lbgfx -lbimg -lbx` plus the Apple frameworks bgfx's
Metal backend links against).

**Editor IntelliSense is a third, read-only consumer of the build, not part
of it.** A language server parses files with no flags and so can't resolve
`<SDL3/SDL.h>`. `make compdb` reuses the CMake path purely to emit a
`compile_commands.json` (CMake's `CMAKE_EXPORT_COMPILE_COMMANDS`) into
`build/cmake/`, symlinked to the repo root where clangd auto-discovers it —
each file then gets the *same* per-folder include path the build uses, so the
per-folder isolation holds in the editor too. It is configured into its own
`build/cmake/` subdir specifically so it never collides with the Makefile's
`build/c`, `build/cpp` and `build/cpp-gpu` object trees. `.vscode/` (clangd default, Microsoft
C/C++ fallback) and a static `.clangd.old` zero-build fallback are committed
so the template is correct on clone; the generated DB itself is git-ignored
(machine-specific absolute paths).

## Project-specific conventions

- **Coordinates are `float` everywhere.** SDL3's 2D API is float-native;
  passing floats straight through avoids lossy int↔float round-trips.
- **"Set colour, then draw."** No draw function takes a colour; they all use
  the renderer's current draw colour. The filled-triangle helper internally
  converts that 0–255 colour to the 0.0–1.0 `SDL_FColor` that
  `SDL_RenderGeometry`'s `SDL_Vertex` requires, so the convention holds
  uniformly.
- **The two gfx copies are kept behaviourally identical on purpose.** If you
  change an algorithm, change it in *both* `examples/c/gfx.c` and
  `examples/cpp/gfx.cpp` (this duplication is the cost of independence and is
  intentional — do not "DRY" it back into a shared file).
- **Window size is queried each frame** in the 2D examples, not hardcoded, so
  animation stays correct after a resize.
- **GPU shaders in `cpp-gpu/` are embedded MSL source, never precompiled
  bytecode.** The Metal backend compiles them at runtime, so the template
  needs no offline shader toolchain (SPIR-V/`shadercross`). This is a
  deliberate constraint — it preserves the "just `brew install sdl3`" promise
  for that example. Keep it that way.
- **The `cpp-gpu-shadercross/` example is the one intentional exception** to
  the brew-only-deps rule above. It depends on `SDL_shadercross` (built from
  source on macOS — Homebrew has no formula yet), plus `glslang`
  (`brew install glslang`) for `make shaders`. Both build systems detect
  shadercross optionally and skip the target when missing. The other three
  examples remain build-clean on a vanilla SDL3-only machine — verify with
  `pkg-config --exists sdl3-shadercross || make` (the three demos build,
  shadercross note prints, exit 0). Do not promote shadercross into a hard
  prerequisite for the rest of the repo.
- **`SDL_ShaderCross_CompileSPIRVFromHLSL` requires DXC** (Microsoft's
  DirectX Shader Compiler), which is *not* bundled with shadercross on
  macOS. So `--mode runtime` and `make shaders-hlsl` are the "advanced"
  paths — they need a separate DXC install. The default `--mode spirv` and
  `make shaders` path uses `glslangValidator` on the sibling `.glsl` files
  and works on any shadercross build. The cross-platform-dispatch part of
  the example (`SDL_ShaderCross_CompileGraphicsShaderFromSPIRV`) uses
  SPIRV-Cross, which always ships with shadercross — that part never needs
  DXC. Document this distinction; do not regress it.
- **In the GPU example a NULL swapchain texture must still submit the command
  buffer.** It is a valid no-draw frame (minimized / headless), not an error;
  treating it as an error would break headless `make smoke`.
- **The GPU example is exempt from the `gfx` conventions above** — it uses
  `SDL_GPU`, not the 2D renderer, so no `gfx`, no "set colour then draw", no
  shared drawing code.
- **`examples/cpp-bgfx/` is the second intentional exception** to the
  brew-only-deps rule. It depends on bgfx (built from source via
  [`bgfx.cmake`](https://github.com/bkaradzic/bgfx.cmake)) and is detected
  via the `BGFX_PREFIX` variable in both build systems; absence is handled
  identically to shadercross (skip cleanly, friendly note). Two-shape
  detection (installed prefix preferred, in-tree-build fallback) is what
  lets users avoid the `cmake --install` step if they want.
- **`bgfx::renderFrame()` MUST be called once BEFORE `bgfx::init()`.** It is
  the documented opt-in to bgfx's single-threaded mode. Without it bgfx
  spawns a render thread that fights SDL3's main-thread expectation on
  macOS, complicates shutdown ordering, and makes the example harder to
  reason about. Both `hello_bgfx.cpp` and `triangle_bgfx.cpp` do this; do
  not regress.
- **bgfx's `varying.def.sc` parser silently drops declarations that follow
  `//` line comments.** Keep that file mechanical — declarations only, blank
  lines fine, no comments. The narrative comments belong in the `.sc`
  shader files (which use a real C preprocessor) or in the sibling
  `examples/cpp-bgfx/shaders/README.md`.
- **The bgfx examples on macOS compile shaders for Metal only.** Cross-platform
  re-compilation is a one-line `shaderc -p <platform>` swap documented in
  `examples/cpp-bgfx/shaders/README.md`, but the single-platform default
  keeps the binary self-contained and the focus on the SDL3↔bgfx wiring
  (mirrors `cpp-gpu/`'s "embedded MSL, macOS-only" choice).
- **bgfx.cmake's `install()` rules do NOT ship the `.sc` common headers
  (`bgfx_shader.sh`, `bgfx_compute.sh`)** — an upstream omission. Step (d)
  of the bgfx install README copies them into the canonical
  `$(BGFX_PREFIX)/share/bgfx/shaders/` so the install prefix is
  self-contained. The Makefile's `BGFX_SHADER_INC` detection falls back
  through the prefix's `share/bgfx/shaders/`, then a sibling source tree,
  then an explicit override — see Makefile for the chain.
- **Comments explain *why* / the algorithm / SDL3 nuance**, never restate the
  code — this is a learning template.

## Key files to read first

1. `examples/c/gfx.h` — the C API and its contracts.
2. `examples/c/traditional.c` — the explicit lifecycle, fully narrated.
3. `examples/cpp/callbacks.cpp` — the same scene inverted under SDL control.
4. `examples/cpp/gfx.hpp` — the same API expressed idiomatically in C++.
5. Either `gfx.c` / `gfx.cpp` — the shape algorithms (midpoint circle,
   scanline fill, geometry triangle).
6. `examples/cpp-gpu/triangle.cpp` — the separate `SDL_GPU` tier: the
   graphics-pipeline lifecycle, narrated end to end, with embedded MSL shaders.
7. (Optional/advanced) `examples/cpp-gpu-shadercross/triangle_shadercross.cpp` —
   same pipeline, but shaders authored once in HLSL and translated through
   `SDL_shadercross` so the binary is portable across Metal/Vulkan/D3D12.
   Diff against `triangle.cpp` (#6) to see what the cross-platform
   shader-acquisition path adds in isolation. Read its sibling
   [`README.md`](examples/cpp-gpu-shadercross/README.md) first — it covers the
   one-time `SDL_shadercross` install.
8. (Optional/advanced) `examples/cpp-bgfx/hello_bgfx.cpp` — the SDL3↔bgfx
   wiring with no shader pipeline. Teaches the native-window handoff
   (`SDL_PROP_WINDOW_COCOA_WINDOW_POINTER`), the `bgfx::renderFrame()`
   single-threaded-mode opt-in, and the bgfx per-frame lifecycle
   (`setViewClear` / `touch` / `dbgTextPrintf` / `frame`). Read its sibling
   [`README.md`](examples/cpp-bgfx/README.md) first — it covers the
   one-time `bgfx.cmake` install.
9. (Optional/advanced) `examples/cpp-bgfx/triangle_bgfx.cpp` — same wiring
   as #8 plus a full hello-triangle: `bgfx::VertexLayout`, vertex/index
   buffers, offline-compiled `.sc` shaders loaded as `.bin` bytecode at
   runtime. Diff against `cpp-gpu/triangle.cpp` (#6) to see how the same
   scene looks under two different GPU abstractions side by side.
