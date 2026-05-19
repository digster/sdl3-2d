# Prompts

## 2026-05-17 — Initial SDL3 2D template

- Set up a basic SDL3 template with a game loop for my mac which I can use
- Bootstrap a basic display pipeline and input
- Also add a few basic shape/drawing functions that I can use to display them
- If you need to install anything on the Mac, let me know, I will install it
  manually
- It needs to support C and C++ both

Follow-up clarifications during planning:

- Make sure the code is well documented by comments so it's easier to
  understand.
- Use a Makefile for building, running, debugging, etc.
- Show examples for both build systems (Makefile + CMake).

Decisions reached: ship both loop styles as separate examples (C traditional
loop + C++ callbacks), SDL3 installed via Homebrew, shared `extern "C"` draw
helper consumed by both languages.

## 2026-05-19 — Split into two fully independent C / C++ examples

- In the current structure the C++ code calls the C code. Is it possible to
  have two separate parallel examples, one in C++ and one in C, without any
  interdependency?

Decisions reached (the previous shared-object design is replaced, not kept):

- Each example gets its **own** gfx in its own language: `examples/c/`
  (`gfx.c`/`gfx.h`, plain C, `gfx_` prefix) and `examples/cpp/`
  (`gfx.cpp`/`gfx.hpp`, idiomatic C++ — a `gfx::` namespace, no `extern "C"`).
- **One top-level build** (single Makefile + CMakeLists) but with **fully
  independent targets**: separate object trees, per-folder include paths, no
  shared `gfx.o`/library.
- The `extern "C"` / "zero duplicated source" interop demonstration is
  **replaced**; ~135 lines of gfx are duplicated on purpose so either folder
  is copy-paste independent. ARCHITECTURE.md / README.md rewritten to teach
  the new "two standalone starters" story.

## 2026-05-19 — Fix SDL3 red squiggles / phantom errors in VS Code

- In VS Code on my mac, for anything SDL related, I keep seeing red lines and
  errors (everything runs fine, only an issue in the editor). How do I solve
  it? (Screenshot: `Unknown type name 'SDL_Renderer' clang(unknown_typename)`.)

Decisions reached:

- Root cause: the editor's clangd parses files with no flags, so
  `<SDL3/SDL.h>` doesn't resolve. Build is unaffected.
- Fix via a CMake-generated `compile_commands.json` (primary), plus ship a
  static `.clangd.old` zero-build fallback (renamed by hand if wanted).
- "Not sure" which extension → configure both (clangd default with the
  Microsoft engine disabled; `c_cpp_properties.json` as the documented
  fallback), commit `.vscode/` so the template is correct on clone.

## 2026-05-19 — Add a C++ SDL_GPU graphics-pipeline example

- Add a C++ example demonstrating a basic SDL3 graphics pipeline which
  abstracts the underlying GPU
- Add it as a separate C++ example
- It should be well commented

Decisions reached during planning:

- It is the SDL_GPU "hello triangle": create device → compile shaders → build
  graphics pipeline → per-frame command buffer / render pass / swapchain.
- Shaders are embedded MSL strings (Metal backend compiles at runtime) so the
  template keeps its "just `brew install sdl3`, no toolchain" promise.
- SDL callback loop (consistent with `examples/cpp/callbacks.cpp`); minimal
  static triangle with vertices baked into the vertex shader (no vertex/
  transfer buffers) — tightest focus on the pipeline itself.
- New `examples/cpp-gpu/` folder, single self-contained `triangle.cpp`, its
  own build target/object tree; shares no source and does not use `gfx`.
