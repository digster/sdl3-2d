# sdl3-2d

A small, reusable **SDL3** 2D starter for macOS. It ships **two fully
independent examples** — one pure **C**, one pure **C++** — each bootstrapping
a window, a render/input loop, and its own documented shape-drawing helpers,
with a different **game-loop style** to learn from.

The two examples share **no source**. Copy either folder out on its own and it
still builds — that independence (not code reuse) is the point of the template.

## What you get

| Path | Purpose |
|---|---|
| `examples/c/gfx.h` + `gfx.c` | The C example's own drawing API + implementation (also the API reference — every function is documented inline). Plain C, `gfx_` prefix. |
| `examples/c/traditional.c` | C example using a hand-written `while` game loop. |
| `examples/cpp/gfx.hpp` + `gfx.cpp` | The C++ example's own, idiomatic copy: a `gfx::` namespace, no `extern "C"`. |
| `examples/cpp/callbacks.cpp` | C++ example using SDL3's `SDL_AppInit/Iterate/Event/Quit` callback model. |
| `Makefile` | Primary build/run/debug driver (pkg-config based). Independent targets. |
| `CMakeLists.txt` | Alternative build path (IDE-friendly, `find_package(SDL3)`). |

Both examples render the same scene (every helper exercised once, plus a
delta-time–driven bouncing ball) so you can compare the two loop styles
side by side — implemented separately in each language.

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
draw colour, so the pattern is always: pick a colour, then draw. The two
examples expose the same operations under each language's native scoping —
the **C** copy uses a `gfx_` prefix, the **C++** copy a `gfx::` namespace:

```c
// C  (examples/c/gfx.h)            // C++ (examples/cpp/gfx.hpp)
gfx_clear        (ren, r,g,b);      gfx::clear        (ren, r,g,b);
gfx_set_color    (ren, r,g,b,a);    gfx::set_color    (ren, r,g,b,a);  // 0..255
gfx_point        (ren, x,y);        gfx::point        (ren, x,y);
gfx_line         (ren, x1,y1,x2,y2);gfx::line         (ren, x1,y1,x2,y2);
gfx_rect         (ren, x,y,w,h);    gfx::rect         (ren, x,y,w,h);   // outline
gfx_fill_rect    (ren, x,y,w,h);    gfx::fill_rect    (ren, x,y,w,h);   // filled
gfx_circle       (ren, cx,cy,r);    gfx::circle       (ren, cx,cy,r);   // outline
gfx_fill_circle  (ren, cx,cy,r);    gfx::fill_circle  (ren, cx,cy,r);   // filled
gfx_triangle     (ren, ...);        gfx::triangle     (ren, ...);       // outline
gfx_fill_triangle(ren, ...);        gfx::fill_triangle(ren, ...);       // filled
```

See `examples/c/gfx.h` and `examples/cpp/gfx.hpp` for full per-function docs
and caveats.

## Customising

- **Want a C++ traditional-loop or a C callback variant?** Copy the whole
  `examples/c` or `examples/cpp` folder (it is self-contained — gfx + the
  example), swap the loop style, and add a target. Makefile: add a rule
  mirroring the existing ones (compile that folder's `gfx` plus its example,
  no shared object). CMake: one `add_executable(... that_folder/gfx.* the
  example)` + `target_include_directories(... that_folder)` +
  `target_link_libraries(... SDL3::SDL3)`.
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
