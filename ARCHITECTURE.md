# Architecture

This document is the "big picture" — read it before diving into files.

## Goal

A minimal, reusable SDL3 2D starter for macOS that is genuinely usable from
**both C and C++**, demonstrates **both** SDL3 game-loop styles, and ships a
small documented drawing API. It is intentionally tiny: a base to copy into
future 2D experiments, not a framework.

## Component map

```
            include/gfx.h            (extern "C" API + reference)
                  │
                  ▼
            src/gfx.c                (compiled ONCE, as C  ->  gfx.o / libgfx)
                  │
        ┌─────────┴──────────┐
        ▼                    ▼
examples/traditional.c   examples/callbacks.cpp
   (C, owns main()         (C++, SDL hands control
    + while loop)           to SDL_App* callbacks)
        │                    │
        ▼                    ▼
  demo_traditional       demo_callbacks
```

The single design decision that ties it together: **`gfx.c` is compiled as C
exactly once**, and both executables link that one object. `gfx.h` wraps every
declaration in `extern "C"` so the C++ translation unit asks the linker for
un-mangled symbol names that match what the C compiler emitted. This is why one
C example + one C++ example covers all four axes (C, C++, traditional loop,
callbacks) with **zero duplicated source**.

## Data / control flow

Both examples follow the same conceptual lifecycle; only *who drives it*
differs:

```
init SDL ─► create window+renderer ─► enable vsync
   ─► [ pump events ─► update with dt ─► clear ─► draw ─► present ] (repeat)
   ─► destroy renderer/window ─► SDL_Quit
```

- **`traditional.c`** owns `main()` and the `while (running)` loop explicitly.
  You can see every step. Quitting is a `bool` flip.
- **`callbacks.cpp`** defines `SDL_MAIN_USE_CALLBACKS` and implements
  `SDL_AppInit / SDL_AppEvent / SDL_AppIterate / SDL_AppQuit`. SDL owns the
  loop and calls those. Per-app state is a heap C++ `AppState` whose pointer
  SDL threads back through every callback (`appstate`) — no globals. Quitting
  is returning `SDL_APP_SUCCESS`. The callback runtime calls `SDL_Quit()`
  itself *after* `SDL_AppQuit`, so the app must destroy its own
  window/renderer but must **not** call `SDL_Quit()`.

The callback model is the SDL3-recommended one because it is what makes the
same code portable to environments that won't let you own the main loop
(Emscripten/web, iOS).

## Build architecture (two paths, same output)

| | Makefile (primary) | CMake (alternative) |
|---|---|---|
| SDL3 discovery | `pkg-config --cflags/--libs sdl3` | `find_package(SDL3 CONFIG)` |
| Best for | quick CLI use, smallest setup | IDEs, larger/multi-platform growth |
| macOS entry point | `<SDL3/SDL_main.h>` include handles it | `SDL3::SDL3` target also wires `SDL_main` |

Both compile `src/gfx.c` with the C compiler, link it into the C demo via
`clang` and the C++ demo via `clang++` (the C++ link pulls in the C++
runtime), and emit `build/demo_traditional` + `build/demo_callbacks`.

`make smoke` and the `--frames N` argument both examples accept exist so the
build can be verified **non-interactively** (render N frames, exit 0) without a
human closing windows — useful for CI later.

## Project-specific conventions

- **Coordinates are `float` everywhere.** SDL3's 2D API is float-native;
  passing floats straight through avoids lossy int↔float round-trips.
- **"Set colour, then draw."** No `gfx_*` function takes a colour; they all
  use the renderer's current draw colour. `gfx_fill_triangle` internally
  converts that 0–255 colour to the 0.0–1.0 `SDL_FColor` that
  `SDL_RenderGeometry`'s `SDL_Vertex` requires, so the convention holds
  uniformly.
- **Window size is queried each frame**, not hardcoded, so animation stays
  correct after a resize.
- **Comments explain *why* / the algorithm / SDL3 nuance**, never restate the
  code — this is a learning template.

## Key files to read first

1. `include/gfx.h` — the API and its contracts.
2. `examples/traditional.c` — the explicit lifecycle, fully narrated.
3. `examples/callbacks.cpp` — the same thing inverted under SDL control.
4. `src/gfx.c` — the shape algorithms (midpoint circle, scanline fill,
   geometry triangle).
