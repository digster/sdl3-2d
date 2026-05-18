# sdl3-2d

A small, reusable **SDL3** 2D starter for macOS. It bootstraps a window, a
render/input loop, and a handful of documented shape-drawing helpers — usable
from **both C and C++**, with **two game-loop styles** to learn from.

## What you get

| File | Purpose |
|---|---|
| `include/gfx.h` | The drawing API (also the API reference — every function is documented inline). `extern "C"` so it works from C and C++. |
| `src/gfx.c` | Implementation: point, line, rect, circle, triangle (outline + filled). Compiled as C. |
| `examples/traditional.c` | C example using a hand-written `while` game loop. |
| `examples/callbacks.cpp` | C++ example using SDL3's `SDL_AppInit/Iterate/Event/Quit` callback model. |
| `Makefile` | Primary build/run/debug driver (pkg-config based). |
| `CMakeLists.txt` | Alternative build path (IDE-friendly, `find_package(SDL3)`). |

Both examples render the same scene (every helper exercised once, plus a
delta-time–driven bouncing ball) so you can compare the two loop styles
side by side.

## Prerequisites (one-time, manual)

SDL3 is not bundled. Install it with Homebrew:

```sh
brew install sdl3
```

`clang` and `make` ship with the Xcode Command Line Tools (`xcode-select
--install`). CMake is only needed for the alternative build path
(`brew install cmake`).

## Build & run (Makefile — primary)

```sh
make            # build both demos into build/   (release, -O2)
make run-c      # build + run the C / traditional-loop demo
make run-cpp    # build + run the C++ / callbacks demo
make smoke      # build both, run each headless-ish for 120 frames, exit
make debug-c    # rebuild C demo with -g -O0 and open lldb
make debug-cpp  # rebuild C++ demo with -g -O0 and open lldb
make clean      # remove build/
```

Force a debug build for any target with `make DEBUG=1 <target>`.

### Controls

- `Esc` or the window close button — quit.
- Key presses, mouse clicks, and window resizes are logged to the console.

## Build & run (CMake — alternative)

Use this for IDE integration or if the project grows:

```sh
cmake -B build
cmake --build build
./build/demo_traditional
./build/demo_callbacks
```

## The drawing API

All coordinates are `float`. Every shape draws with the renderer's *current*
draw colour, so the pattern is always: pick a colour, then draw.

```c
gfx_clear      (renderer, r, g, b);                 // clear the frame
gfx_set_color  (renderer, r, g, b, a);              // 0..255 each
gfx_point      (renderer, x, y);
gfx_line       (renderer, x1, y1, x2, y2);
gfx_rect       (renderer, x, y, w, h);              // outline
gfx_fill_rect  (renderer, x, y, w, h);              // filled
gfx_circle     (renderer, cx, cy, radius);          // outline
gfx_fill_circle(renderer, cx, cy, radius);          // filled
gfx_triangle   (renderer, x1,y1, x2,y2, x3,y3);     // outline
gfx_fill_triangle(renderer, x1,y1, x2,y2, x3,y3);   // filled
```

See `include/gfx.h` for full per-function docs and caveats.

## Customising

- **Want a C++ traditional-loop or a C callback variant?** Copy one example,
  swap the loop style, and add a target. Makefile: add a rule mirroring the
  existing ones. CMake: one `add_executable` + `target_link_libraries(... gfx
  SDL3::SDL3)`.
- **Resolution / title:** edit the `SDL_CreateWindowAndRenderer(...)` call in
  the example you use.
- **Frame pacing:** vsync is on (`SDL_SetRenderVSync(renderer, 1)`). If a
  display/driver doesn't honour vsync the loop may run uncapped — add a frame
  cap (e.g. `SDL_DelayNS`) if you see that.

## Notes & gotchas

- **SDL3 ≠ SDL2.** `SDL_Init` / `SDL_CreateWindowAndRenderer` return `bool`
  (`true` = success — the opposite of SDL2's `0`). Render primitives take
  `float` and use `SDL_FRect`.
- **Retina / high-DPI.** The window is created *without*
  `SDL_WINDOW_HIGH_PIXEL_DENSITY`, so render coordinates match window points
  1:1 and the examples stay simple. If you later enable high-DPI, query
  `SDL_GetRenderOutputSize` for the true pixel size.
- **Troubleshooting `make`:** if you see `ERROR: SDL3 not found by
  pkg-config`, run `brew install sdl3`. If CMake's `find_package(SDL3)` fails,
  ensure Homebrew's prefix is on CMake's search path (it is by default on
  Apple Silicon at `/opt/homebrew`).

## License

MIT — see [LICENSE](LICENSE).
