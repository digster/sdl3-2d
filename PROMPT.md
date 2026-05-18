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
