# Architecture

This document is the "big picture" — read it before diving into files.

## Goal

A minimal, reusable SDL3 2D starter for macOS shipped as **two fully
independent examples** — one pure C, one pure C++ — each demonstrating a
different SDL3 game-loop style and each carrying its own small documented
drawing API. It is intentionally tiny: a base to copy into future 2D
experiments, not a framework.

The defining decision: **the two examples share no source.** Either folder can
be copied out on its own and built with nothing else present. The ~135 lines
of drawing helpers are *deliberately duplicated* (once in C, once in idiomatic
C++) so that independence — not code reuse — is the property this template
teaches.

## Component map

```
examples/c/                          examples/cpp/
  gfx.h    (C API + reference)         gfx.hpp  (C++ API + reference)
  gfx.c    (compiled as C)             gfx.cpp  (compiled as C++)
  traditional.c                        callbacks.cpp
  (owns main() + while loop)           (SDL hands control to SDL_App*)
        │                                    │
        ▼                                    ▼
  build/demo_traditional               build/demo_callbacks
```

There is **no arrow between the two columns.** `examples/c/` is compiled and
linked entirely by the C compiler; `examples/cpp/` entirely by the C++
compiler. Neither include path can see the other folder. The only thing they
have in common is that both call the same plain-C **SDL3** library — which is
exactly why the same scene can be expressed twice with no shared code.

> Contrast with C's prefix convention: the C copy disambiguates names with a
> `gfx_` prefix (`gfx_circle`); the C++ copy uses a `gfx` **namespace**
> (`gfx::circle`) — each language's native way to scope an API. This pairing is
> itself part of what the template demonstrates.

## Data / control flow

Both examples follow the same conceptual lifecycle; only *who drives it*
differs:

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

## Build architecture (two paths, both keep the examples disjoint)

| | Makefile (primary) | CMake (alternative) |
|---|---|---|
| SDL3 discovery | `pkg-config --cflags/--libs sdl3` | `find_package(SDL3 CONFIG)` |
| Best for | quick CLI use, smallest setup | IDEs, larger/multi-platform growth |
| macOS entry point | `<SDL3/SDL_main.h>` include handles it | `SDL3::SDL3` target also wires `SDL_main` |

Both build systems compile each example **only from its own folder**, with
that folder as the sole project include path (`-Iexamples/c` /
`-Iexamples/cpp`), into separate object trees (`build/c/`, `build/cpp/`).
There is no shared object and no shared library — removing any one folder
cannot break the other binary.

`make smoke` and the `--frames N` argument both examples accept exist so the
build can be verified **non-interactively** (render N frames, exit 0) without a
human closing windows — useful for CI later. Proving independence directly:
`cd examples/c && clang -I. *.c $(pkg-config --cflags --libs sdl3)` builds the
C demo with the C++ folder literally invisible (and vice versa).

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
- **Window size is queried each frame**, not hardcoded, so animation stays
  correct after a resize.
- **Comments explain *why* / the algorithm / SDL3 nuance**, never restate the
  code — this is a learning template.

## Key files to read first

1. `examples/c/gfx.h` — the C API and its contracts.
2. `examples/c/traditional.c` — the explicit lifecycle, fully narrated.
3. `examples/cpp/callbacks.cpp` — the same scene inverted under SDL control.
4. `examples/cpp/gfx.hpp` — the same API expressed idiomatically in C++.
5. Either `gfx.c` / `gfx.cpp` — the shape algorithms (midpoint circle,
   scanline fill, geometry triangle).
