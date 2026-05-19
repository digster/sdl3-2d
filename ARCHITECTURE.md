# Architecture

This document is the "big picture" — read it before diving into files.

## Goal

A minimal, reusable SDL3 starter for macOS shipped as **three fully
independent examples**, each isolating exactly one thing to learn:

- **`examples/c/`** — C, a hand-written `while` game loop, 2D renderer.
- **`examples/cpp/`** — C++, the SDL3 callback loop, 2D renderer.
- **`examples/cpp-gpu/`** — C++, the lower-level `SDL_GPU` API: a basic
  graphics pipeline that abstracts Metal/Vulkan/D3D12.

It is intentionally tiny: a base to copy into future experiments, not a
framework. The first two demonstrate the two SDL3 game-loop styles on the
high-level 2D renderer; the third drops a tier to show the GPU-pipeline
machinery (it does not use the 2D renderer or `gfx` at all — the two APIs are
mutually exclusive).

The defining decision: **the examples share no source.** Any folder can be
copied out on its own and built with nothing else present. The ~135 lines of
drawing helpers are *deliberately duplicated* between `c/` and `cpp/` (once in
C, once in idiomatic C++); the GPU example is self-contained in a single file.
Independence — not code reuse — is the property this template teaches.

## Component map

```
examples/c/                  examples/cpp/                 examples/cpp-gpu/
  gfx.h   (C API + ref)        gfx.hpp (C++ API + ref)       triangle.cpp
  gfx.c   (compiled as C)      gfx.cpp (compiled as C++)       embedded MSL
  traditional.c                callbacks.cpp                   shaders;
  (owns main()+while loop)     (SDL drives SDL_App*)           no gfx
        │                            │                            │
        ▼                            ▼                            ▼
  build/demo_traditional       build/demo_callbacks         build/demo_gpu
   (2D renderer)                (2D renderer)                (SDL_GPU pipeline)
```

There is **no arrow between the columns.** `examples/c/` is compiled and
linked entirely by the C compiler; `examples/cpp/` and `examples/cpp-gpu/`
entirely by the C++ compiler, each with its own private include path so no
folder can see another. The only thing the three share is that all call the
same plain-C **SDL3** library — which is exactly why each scene can be
expressed with no shared code. The GPU example notably does **not** include
`gfx`: it uses `SDL_GPU`, a different SDL3 subsystem from the 2D renderer the
other two use.

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

## Build architecture (two paths, both keep the examples disjoint)

| | Makefile (primary) | CMake (alternative) |
|---|---|---|
| SDL3 discovery | `pkg-config --cflags/--libs sdl3` | `find_package(SDL3 CONFIG)` |
| Best for | quick CLI use, smallest setup | IDEs, larger/multi-platform growth |
| macOS entry point | `<SDL3/SDL_main.h>` include handles it | `SDL3::SDL3` target also wires `SDL_main` |

Both build systems compile each example **only from its own folder**, with
that folder as the sole project include path (`-Iexamples/c` /
`-Iexamples/cpp` / `-Iexamples/cpp-gpu`), into separate object trees
(`build/c/`, `build/cpp/`, `build/cpp-gpu/`). There is no shared object and no
shared library — removing any one folder cannot break the others.

`make smoke` and the `--frames N` argument all three examples accept exist so
the build can be verified **non-interactively** (render N frames, exit 0)
without a human closing windows — useful for CI later. (The GPU example's
NULL-swapchain handling is what keeps it valid headless; see the lifecycle
above.) Proving independence directly: `cd examples/cpp-gpu && clang++ -I.
triangle.cpp $(pkg-config --cflags --libs sdl3)` builds the GPU demo with the
other folders literally invisible (and likewise for `c/`, `cpp/`).

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
- **GPU shaders are embedded MSL source, never precompiled bytecode.** The
  Metal backend compiles them at runtime, so the template needs no offline
  shader toolchain (SPIR-V/`shadercross`). This is a deliberate constraint —
  it preserves the "just `brew install sdl3`" promise. Keep it that way.
- **In the GPU example a NULL swapchain texture must still submit the command
  buffer.** It is a valid no-draw frame (minimized / headless), not an error;
  treating it as an error would break headless `make smoke`.
- **The GPU example is exempt from the `gfx` conventions above** — it uses
  `SDL_GPU`, not the 2D renderer, so no `gfx`, no "set colour then draw", no
  shared drawing code.
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
