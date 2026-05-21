# `examples/cpp-gpu-shadercross/` — cross-platform shaders via SDL_shadercross

This example renders the same gradient triangle as
[`examples/cpp-gpu/triangle.cpp`](../cpp-gpu/triangle.cpp), but with shaders
that are **portable across Metal, Vulkan and D3D12 from a single HLSL source.**
It uses [SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross), SDL's
official shader cross-compiler — both its **runtime library** (HLSL → SPIR-V
→ backend-native, all in process) and its **CLI** (offline build step that
emits inspectable `.spv` / `.msl` / `.dxil` files).

> **The one example with an extra dependency.** The other three demos in this
> repo build on just `brew install sdl3`. SDL_shadercross is not in Homebrew
> yet — see [Building SDL_shadercross on macOS](#building-sdl_shadercross-on-macos)
> below. The top-level Makefile and CMake **detect it optionally** and skip
> this target with a friendly message when it is absent, so the other examples
> keep their stand-alone build guarantee.

## Contents

```
triangle_shadercross.cpp     The example — single TU, ~340 lines, well-commented.
shaders/
  triangle.vert.glsl         Vertex source in GLSL — paired with `make shaders`
                             (uses glslangValidator; no DXC needed). Default path.
  triangle.frag.glsl         Fragment source in GLSL.
  triangle.vert.hlsl         Vertex source in HLSL — paired with `make shaders-hlsl`
                             (uses shadercross CLI; needs DXC). Optional path.
  triangle.frag.hlsl         Fragment source in HLSL. Also embedded as a string
                             in triangle_shadercross.cpp for `--mode runtime`.
  build/                     (gitignored) Cross-compiled .spv output.
```

The GLSL and HLSL sources are different *languages* expressing the same
shader — pick whichever toolchain you have. The embedded HLSL strings in
`triangle_shadercross.cpp` mirror the `.hlsl` files; keep them in sync if you
edit either.

## Two workflows, two source languages

shadercross can take **either HLSL or SPIR-V** as input. HLSL → SPIR-V goes
through the DirectXShaderCompiler (DXC), which is *not* bundled with
shadercross on macOS — you have to build with it explicitly. SPIR-V → native
(MSL / DXIL) goes through SPIRV-Cross, which **always** ships with shadercross.

That means the **cross-platform shader piece** — turning one binary's SPIR-V
into whatever the device's GPU backend natively wants — runs without DXC. The
only thing that needs DXC is converting HLSL into SPIR-V in the first place.
Since `glslangValidator` (Homebrew: `brew install glslang`) does the same job
for GLSL with no DXC dependency, that is the path the default workflow uses.

| Mode | How it gets SPIR-V | Needs DXC? | Typical use |
|------|--------------------|------------|-------------|
| `--mode spirv` (**default**) | Pre-compiled `shaders/build/*.spv` from disk. Built by `make shaders` (GLSL → glslang) or `make shaders-hlsl` (HLSL → shadercross). | No (default path) | Shipped games. Compile once at build time, ship the bytecode. |
| `--mode runtime` | Embedded HLSL → `SDL_ShaderCross_CompileSPIRVFromHLSL` at startup. | **Yes** | Editors, hot-reload, anywhere shaders are authored on the user's machine. |

Both modes converge on `SDL_ShaderCross_CompileGraphicsShaderFromSPIRV` — that
single function is the one doing the cross-platform dispatch. The `--mode`
switch only changes how you get to the SPIR-V you hand it.

## Building SDL_shadercross on macOS

```sh
# Required: SPIRV-Cross. shadercross uses it for every SPIR-V → native path
# (including the runtime cross-platform dispatch this example demonstrates).
brew install spirv-cross

# Required: glslangValidator. Used by `make shaders` to turn the .glsl sources
# into the SPIR-V the C++ example loads at runtime. Comes from Homebrew.
brew install glslang

# Clone shadercross (recursive picks up any submodules it needs).
git clone --recursive https://github.com/libsdl-org/SDL_shadercross.git
cd SDL_shadercross

# Default build: SPIR-V / MSL paths (no DXC). This is enough to run the
# example in `--mode spirv` (the default) after `make shaders` from the
# repo root. See "Advanced: enable HLSL input (DXC)" below if you want
# `--mode runtime` and `make shaders-hlsl` to work too.
cmake -B build \
      -DSDLSHADERCROSS_INSTALL=ON \
      -DSDLSHADERCROSS_DXC=OFF \
      -DSDLSHADERCROSS_CLI=ON

cmake --build build -j

# Install. /usr/local is fine; use --prefix to redirect.
sudo cmake --install build
```

After this, the top-level Makefile and CMake of this repo will find
shadercross automatically. Verify:

```sh
pkg-config --modversion sdl3-shadercross    # expect: 3.0.0 (or higher)
which glslangValidator                       # expect: /opt/homebrew/bin/glslangValidator
```

If `pkg-config` cannot find shadercross, point it at the install prefix you
used:

```sh
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
```

If the shadercross **dylib** isn't found at run time (`Library not loaded:
@rpath/libSDL3_shadercross.0.dylib`), the install put it in a directory the
default dyld search doesn't cover. Export `DYLD_LIBRARY_PATH` to that prefix:

```sh
export DYLD_LIBRARY_PATH=/usr/local/lib   # or wherever shadercross was installed
```

### Advanced: enable HLSL input (DXC)

The `--mode runtime` path in the example, and the `make shaders-hlsl` target,
both go through DirectXShaderCompiler (DXC) to turn HLSL into SPIR-V. DXC is
not bundled with shadercross; on macOS you supply it yourself:

1. Download the macOS DXC release from
   <https://github.com/microsoft/DirectXShaderCompiler/releases>
   (look for `dxc_<date>_macos*.tar.gz`).
2. Copy `libdxcompiler.dylib` (and `libdxil.dylib` if present) into a library
   path — e.g. `/usr/local/lib`.
3. Re-configure shadercross with `-DSDLSHADERCROSS_DXC=ON`, rebuild,
   reinstall.

Without DXC, the C++ example still works in `--mode spirv` (the default) — it
just can't compile HLSL strings at startup. `make shaders` (the default
GLSL+glslang variant) doesn't care either way.

## Running the example

After SDL_shadercross + glslang are installed and you have re-run `make` from
the repo root:

```sh
# The default workflow — works on any shadercross install (DXC not required).
make shaders                               # GLSL → SPIR-V via glslangValidator
make run-shadercross                       # loads the .spv files, dispatches via shadercross
./build/demo_gpu_shadercross --frames 120  # headless 120-frame smoke run

# Inspect the offline output:
ls examples/cpp-gpu-shadercross/shaders/build/
#   triangle.vert.spv   triangle.frag.spv      (SPIR-V — what the C++ loads)

# The optional HLSL workflow — needs shadercross built with DXC:
make shaders-hlsl                          # HLSL → SPIRV/MSL/DXIL via shadercross CLI
ls examples/cpp-gpu-shadercross/shaders/build/
#   triangle.vert.spv   triangle.frag.spv      (now from HLSL)
#   triangle.vert.msl   triangle.frag.msl      (inspectable MSL output)
#   triangle.vert.dxil  triangle.frag.dxil     (D3D12 — only if DXC enabled)

./build/demo_gpu_shadercross --mode runtime    # compiles embedded HLSL at startup
                                               # (DXC required — see Advanced section above)
```

Both modes render the **exact same image**; the console log on startup shows
which mode was selected and which native format the device chose.

## What to read next

- [`triangle_shadercross.cpp`](triangle_shadercross.cpp) — the full annotated
  example. The header block (lines 1–66) is the conceptual map.
- [`../cpp-gpu/triangle.cpp`](../cpp-gpu/triangle.cpp) — the embedded-MSL
  sibling. Diffing the two is the cleanest way to see what shadercross adds.
- [`../../docs/3d/13-shadercross-in-practice.html`](../../docs/3d/13-shadercross-in-practice.html)
  — the recipe page version of this example.
- [SDL_shadercross README](https://github.com/libsdl-org/SDL_shadercross/blob/main/README.txt)
  and its [public header](https://github.com/libsdl-org/SDL_shadercross/blob/main/include/SDL3_shadercross/SDL_shadercross.h)
  — the canonical API reference (`SDL_ShaderCross_Init`,
  `CompileSPIRVFromHLSL`, `CompileGraphicsShaderFromSPIRV`, …).
