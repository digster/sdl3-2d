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
