# sdl3-2d

A small, reusable **SDL3** starter for macOS. It ships **four fully
independent examples**, each in its own folder and each isolating one thing to
learn:

- **`examples/c/`** — pure **C**, hand-written `while` game loop, 2D renderer.
- **`examples/cpp/`** — pure **C++**, SDL3 callback loop, 2D renderer.
- **`examples/cpp-gpu/`** — **C++**, the lower-level **`SDL_GPU`** API: a basic
  graphics pipeline (the "hello triangle") that abstracts Metal/Vulkan/D3D12.
- **`examples/cpp-gpu-shadercross/`** — **C++**, the same pipeline but with
  **cross-platform shaders** authored once in HLSL and translated to
  MSL/SPIRV/DXIL at runtime via **`SDL_shadercross`** (the only example with a
  dependency beyond `brew install sdl3` — see [its README](examples/cpp-gpu-shadercross/README.md);
  detected optionally so the other three keep building without it).

The examples share **no source**. Copy any folder out on its own and it still
builds — that independence (not code reuse) is the point of the template.

## What you get

| Path | Purpose |
|---|---|
| `examples/c/gfx.h` + `gfx.c` | The C example's own drawing API + implementation (also the API reference — every function is documented inline). Plain C, `gfx_` prefix. |
| `examples/c/traditional.c` | C example using a hand-written `while` game loop. |
| `examples/cpp/gfx.hpp` + `gfx.cpp` | The C++ example's own, idiomatic copy: a `gfx::` namespace, no `extern "C"`. |
| `examples/cpp/callbacks.cpp` | C++ example using SDL3's `SDL_AppInit/Iterate/Event/Quit` callback model. |
| `examples/cpp-gpu/triangle.cpp` | C++ example: a basic **`SDL_GPU`** graphics pipeline (device → shaders → pipeline → render pass). Single self-contained file, embedded MSL shaders, callback model. No `gfx` — see below. |
| `examples/cpp-gpu-shadercross/triangle_shadercross.cpp` | C++ example: the **same triangle, cross-platform shaders**. One HLSL source → SPIR-V via `SDL_shadercross` → backend-native (MSL/SPIRV/DXIL). Two run modes: embedded HLSL at runtime, or pre-compiled `.spv` from `make shaders`. |
| `examples/cpp-gpu-shadercross/shaders/*.hlsl` | The HLSL sources used both as embedded strings (runtime mode) and as input to the offline `shadercross` CLI driven by `make shaders`. |
| `Makefile` | Primary build/run/debug driver (pkg-config based). Independent targets, optional shadercross detection. |
| `CMakeLists.txt` | Alternative build path (IDE-friendly, `find_package(SDL3)` + optional `find_package(SDL3_shadercross)`). |

The two **2D-renderer** examples (`c/`, `cpp/`) render the same scene (every
helper exercised once, plus a delta-time–driven bouncing ball) so you can
compare the two loop styles side by side — implemented separately in each
language. The **GPU** example (`cpp-gpu/`) is deliberately different: it drops
to SDL3's `SDL_GPU` API and draws a single gradient triangle to show the
graphics-pipeline machinery itself (see *The GPU pipeline example* below). The
**shadercross** example (`cpp-gpu-shadercross/`) draws the *same* triangle but
swaps the embedded MSL for HLSL fed through SDL_shadercross — the same binary
can target Metal/Vulkan/D3D12 from one shader source (see *The cross-platform
shaders example* below).

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
make                    # build all available demos into build/   (release, -O2)
make run-c              # build + run the C / traditional-loop demo
make run-cpp            # build + run the C++ / callbacks demo
make run-gpu            # build + run the C++ / SDL_GPU triangle demo
make run-shadercross    # build + run the C++ / SDL_shadercross demo (needs shadercross)
make smoke              # build all, run each headless-ish for 120 frames, exit
make shaders            # compile the GLSL sources to SPIR-V via glslangValidator (default;
                        # no DXC needed). Writes into examples/cpp-gpu-shadercross/shaders/build/
make shaders-hlsl       # same idea but from HLSL via the `shadercross` CLI (needs DXC)
make debug-c            # rebuild C demo with -g -O0 and open lldb
make debug-cpp          # rebuild C++ demo with -g -O0 and open lldb
make debug-gpu          # rebuild GPU demo with -g -O0 and open lldb
make debug-shadercross  # rebuild shadercross demo with -g -O0 and open lldb
make compdb             # (re)generate compile_commands.json for editor IntelliSense
make clean              # remove build/
```

> `make run-shadercross`, `make debug-shadercross`, and `make shaders` require
> SDL_shadercross to be installed — see
> [examples/cpp-gpu-shadercross/README.md](examples/cpp-gpu-shadercross/README.md).
> The default `make` and `make smoke` skip those targets cleanly when shadercross
> is absent.

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
./build/demo_gpu
./build/demo_gpu_shadercross      # only present if SDL_shadercross was found
                                  #   (CMake prints a STATUS message either way)
```

## Editor / IDE setup (IntelliSense)

The build works without this — it only fixes the editor. SDL3's flags reach
the compiler at build time (`pkg-config`), but a language server parses each
file with no flags, can't find `<SDL3/SDL.h>`, and floods the file with
phantom *"unknown type `SDL_Renderer`"* errors. The fix is a compilation
database that hands the editor the real per-file flags:

```sh
make compdb     # writes build/cmake/compile_commands.json, symlinks it to root
```

Then reload VS Code (or run *"clangd: Restart language server"*). Re-run
`make compdb` after `make clean` (it wipes `build/`).

- **clangd (recommended, default config):** `.vscode/` ships pre-wired —
  install the [clangd extension](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd)
  and it auto-discovers the database. The Microsoft C/C++ IntelliSense engine
  is disabled in `.vscode/settings.json` so the two language servers don't
  emit duplicate squiggles.
- **Prefer the Microsoft C/C++ extension?** Set
  `"C_Cpp.intelliSenseEngine": "default"` in `.vscode/settings.json` and
  disable the clangd extension — `.vscode/c_cpp_properties.json` already
  points it at the same `compile_commands.json`.
- **No CMake / zero-build option:** rename `.clangd.old` → `.clangd`. It
  statically adds the Homebrew SDL3 include path (edit it for Intel/`/usr/local`
  or a custom prefix). No `make compdb` needed.

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

## The GPU pipeline example

`examples/cpp-gpu/triangle.cpp` is a different tier from the other two. They
use SDL3's high-level 2D renderer (`SDL_Renderer`); this one uses **`SDL_GPU`**
— SDL3's modern API that abstracts the platform's real graphics API (Metal on
macOS, Vulkan on Linux, Direct3D 12 on Windows) behind one explicit pipeline.
The two APIs are mutually exclusive, so this example shares **no** code with
`gfx` and is a single self-contained file.

It is the canonical "hello triangle": create a GPU device → compile a vertex +
fragment shader → build a graphics pipeline → per frame, record a command
buffer, run a render pass, submit. The scene is one triangle with red/green/
blue corners blended across its face by the rasterizer.

Things worth knowing:

- **Shaders are embedded as Metal Shading Language (MSL) strings.** The Metal
  backend compiles them at runtime, so there is **no offline shader toolchain**
  — the template's "just `brew install sdl3`" promise still holds. (SPIR-V
  would have required `glslang`/`shadercross` in the build.)
- **No vertex buffer.** The three vertices are baked into the vertex shader and
  selected by `[[vertex_id]]`, keeping the focus on the pipeline itself rather
  than GPU resource upload.
- **NULL swapchain is normal, not an error.** When the window is minimized/
  occluded — or there is no display at all (e.g. `make smoke` in CI) —
  `SDL_WaitAndAcquireGPUSwapchainTexture` succeeds but yields a NULL texture.
  The example skips drawing that frame but still submits the command buffer,
  which is why `./demo_gpu --frames 120` exits 0 even headless.
- **Resize needs no pipeline rebuild** — SDL recreates the swapchain for you.

Run it with `make run-gpu`.

## The cross-platform shaders example

`examples/cpp-gpu-shadercross/triangle_shadercross.cpp` draws the same gradient
triangle as `cpp-gpu/`, but solves the portability problem the embedded-MSL
example explicitly side-steps: that binary only runs on Metal because MSL is
Apple-only. This one uses [**SDL_shadercross**](https://github.com/libsdl-org/SDL_shadercross)
— SDL's official shader cross-compiler — to author the two shaders **once** in
HLSL and produce the right native format (SPIR-V, MSL, DXIL) on every backend.

The flow inside `SDL_AppInit` is just three function calls:

```
HLSL source string  ──►  SDL_ShaderCross_CompileSPIRVFromHLSL ──►  SPIR-V bytes
                                                                       │
                                                                       ▼
                                          SDL_ShaderCross_CompileGraphicsShaderFromSPIRV
                                                                       │
                                            (silently transpiles to MSL/DXIL if needed)
                                                                       │
                                                                       ▼
                                                                 SDL_GPUShader *
```

Two modes are selectable from the command line so you can compare the offline
workflow that real games ship with against the runtime convenience layer:

- `./demo_gpu_shadercross` (default `--mode spirv`) — loads pre-compiled
  `.spv` files from `shaders/build/`. Run `make shaders` first to produce
  them. Works on any shadercross install.
- `./demo_gpu_shadercross --mode runtime` — embedded HLSL is compiled to
  SPIR-V at startup via `SDL_ShaderCross_CompileSPIRVFromHLSL`. Requires
  shadercross built with DXC (see the sibling README for the DXC install
  steps on macOS).

**This is the only example with a dependency beyond `brew install sdl3`** —
SDL_shadercross is not in Homebrew, you build it from source. The trade is
explained, with macOS-specific build steps, in
[examples/cpp-gpu-shadercross/README.md](examples/cpp-gpu-shadercross/README.md).
Both the Makefile and CMake detect shadercross at configure time and skip this
target cleanly when it is absent — so the other three demos keep building on a
vanilla SDL3-only machine.

Run it with `make shaders && make run-shadercross` (the `shaders` step
compiles the GLSL sources to SPIR-V via `glslangValidator`; the
shadercross-only path needs DXC which isn't bundled — see the sibling
README's *Advanced* section to enable that).

## Recipes (tutorial track)

Inside [`docs/`](docs/index.html) is a progressive set of **25 callback-style
C++ recipes** — 12 for 2D (`SDL_Renderer`) and 13 for 3D (`SDL_GPU`), ordered
beginner to advanced. Each recipe is a self-contained HTML page with a
narrative walkthrough plus a complete drop-in listing for either
`examples/cpp/callbacks.cpp` (2D), `examples/cpp-gpu/triangle.cpp` (3D), or
`examples/cpp-gpu-shadercross/triangle_shadercross.cpp` (the advanced
shadercross recipe).

- Open `docs/index.html` in any browser to pick a track.
- 2D: hello window → drawing → input → delta time → textures → animation →
  collision → audio → text → tilemap → particles → a Pong capstone.
- 3D: hello triangle → cross-platform shaders → vertex buffers → indices →
  uniforms/MVP → cube → depth → textures → Phong lighting → many objects →
  FPS camera → render-to-texture → **shadercross in practice** (advanced).

No extra build steps — the recipes are read-only docs that describe code you
paste into the existing examples folders and build with `make run-cpp` /
`make run-gpu`.

## Customising

- **Want a C++ traditional-loop or a C callback variant?** Copy the whole
  `examples/c` or `examples/cpp` folder (it is self-contained — gfx + the
  example), swap the loop style, and add a target. Makefile: add a rule
  mirroring the existing ones (compile that folder's `gfx` plus its example,
  no shared object). CMake: one `add_executable(... that_folder/gfx.* the
  example)` + `target_include_directories(... that_folder)` +
  `target_link_libraries(... SDL3::SDL3)`.
- **Extending the GPU example?** Next steps from `triangle.cpp`, in order of
  effort: add a vertex buffer + transfer buffer (real geometry instead of
  `[[vertex_id]]`); add a uniform buffer (`SDL_PushGPUVertexUniformData`) to
  animate it per frame; add a depth texture for 3D. The folder is
  self-contained — copy it out and grow it like the others.
- **Want the shadercross example to render something other than the triangle?**
  Edit the embedded HLSL strings in `triangle_shadercross.cpp` (runtime mode)
  *and* the matching `shaders/triangle.{vert,frag}.hlsl` files (offline mode)
  — they are deliberately mirrored. Re-run `make shaders` after the edit so
  the `.spv` artifacts match. The `SDL_ShaderCross_ReflectGraphicsSPIRV` API
  is what you would call if you add uniform buffers or samplers and want the
  binding counts discovered automatically rather than hand-typed.
- **Resolution / title:** edit the `SDL_CreateWindowAndRenderer(...)` call in
  the 2D examples, or the `SDL_CreateWindow(...)` call in the GPU example.
- **Frame pacing:** the 2D examples enable vsync (`SDL_SetRenderVSync`); if a
  display/driver doesn't honour it the loop may run uncapped — add a frame cap
  (e.g. `SDL_DelayNS`) if you see that. The GPU example paces itself via
  `SDL_WaitAndAcquireGPUSwapchainTexture` (it blocks until a frame is ready).

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
