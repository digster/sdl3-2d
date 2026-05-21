# Makefile — primary build / run / debug driver for the SDL3 2D template.
#
# No CMake required: SDL3's compiler/linker flags are discovered via
# pkg-config (Homebrew installs sdl3.pc when you `brew install sdl3`).
#
# This template ships FOUR fully independent examples that share NO source:
#
#   examples/c/                 — C:   gfx.c + gfx.h + traditional.c (hand-written loop)
#   examples/cpp/               — C++: gfx.cpp + gfx.hpp + callbacks.cpp (SDL callbacks)
#   examples/cpp-gpu/           — C++: triangle.cpp (SDL_GPU pipeline, embedded MSL)
#   examples/cpp-gpu-shadercross/ — C++: cross-platform shaders via SDL_shadercross
#                                  (OPTIONAL — only built if SDL_shadercross is
#                                  installed; the other three keep working without it)
#
# Each demo is built ONLY from its own folder; the object trees live in
# separate build subdirectories and never cross. Copy any folder out and it
# stands alone.
#
# Targets:
#   make                    build all available demos into build/   (release, -O2)
#   make run-c              build + run the C / traditional-loop demo
#   make run-cpp            build + run the C++ / callbacks demo
#   make run-gpu            build + run the C++ / SDL_GPU triangle demo
#   make run-shadercross    build + run the C++ / SDL_shadercross demo (needs shadercross)
#   make smoke              build all, run each with --frames 120 (non-interactive)
#   make shaders            compile the GLSL sources to SPIR-V via glslangValidator
#                           (default; no DXC needed). Writes into
#                           examples/cpp-gpu-shadercross/shaders/build/
#   make shaders-hlsl       same idea but from the HLSL sources via the `shadercross`
#                           CLI (needs DXC). Also emits MSL/DXIL side-outputs for
#                           inspection.
#   make debug-c            rebuild the C demo with -g -O0, launch lldb
#   make debug-cpp          rebuild the C++ demo with -g -O0, launch lldb
#   make debug-gpu          rebuild the GPU demo with -g -O0, launch lldb
#   make debug-shadercross  rebuild the shadercross demo with -g -O0, launch lldb
#   make compdb             (re)generate compile_commands.json for editors (clangd)
#   make clean              remove build/
#
#   make DEBUG=1 <target>   force -g -O0 instead of -O2

# Default to clang/clang++, but still honour an env var or `make CC=...`
# override. `?=` alone won't work here: Make predefines CC=cc / CXX=c++ as
# built-ins, so we only override when the value is that built-in default.
ifeq ($(origin CC),default)
CC := clang
endif
ifeq ($(origin CXX),default)
CXX := clang++
endif

# SDL3 flags from pkg-config (needs `brew install sdl3`).
SDL_CFLAGS := $(shell pkg-config --cflags sdl3 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs   sdl3 2>/dev/null)

# Optional: SDL_shadercross. The build doesn't fail without it — the
# demo_gpu_shadercross target is just skipped with a friendly message.
# Upstream's pkg-config file is `sdl3-shadercross`; some package managers
# install it as `SDL3_shadercross`, so we probe both.
SHADERCROSS_PC := $(shell pkg-config --exists sdl3-shadercross 2>/dev/null && echo sdl3-shadercross || (pkg-config --exists SDL3_shadercross 2>/dev/null && echo SDL3_shadercross))
ifneq ($(SHADERCROSS_PC),)
SHADERCROSS_OK     := yes
SHADERCROSS_CFLAGS := $(shell pkg-config --cflags $(SHADERCROSS_PC))
SHADERCROSS_LIBS   := $(shell pkg-config --libs   $(SHADERCROSS_PC))
# Bake an rpath for shadercross's libdir into the binary at link time. macOS's
# System Integrity Protection strips DYLD_* env vars when binaries are spawned
# under /bin/sh (which `make` uses), so DYLD_LIBRARY_PATH=… isn't a reliable
# fallback — a static rpath is what actually keeps `make run-shadercross` and
# `make smoke` working when shadercross was installed to a non-default prefix
# (e.g. /usr/local/lib on Apple Silicon where SDL3 lives under /opt/homebrew).
SHADERCROSS_LIBDIR := $(shell pkg-config --variable=libdir $(SHADERCROSS_PC) 2>/dev/null)
ifneq ($(SHADERCROSS_LIBDIR),)
SHADERCROSS_LIBS += -Wl,-rpath,$(SHADERCROSS_LIBDIR)
endif
endif
SHADERCROSS_BIN := $(shell command -v shadercross 2>/dev/null)
# glslangValidator (`brew install glslang`) compiles GLSL → SPIR-V without
# needing DirectXShaderCompiler. It is the preferred backend for `make shaders`
# because (a) it is in Homebrew, and (b) shadercross's HLSL-input path requires
# DXC, which isn't bundled on macOS. shadercross's CLI is still used for
# `make shaders-hlsl` (the HLSL-source workflow), which needs DXC.
GLSLANG_BIN := $(shell command -v glslangValidator 2>/dev/null)

# Release by default; `make DEBUG=1` or the debug-* targets switch to -g -O0.
ifdef DEBUG
OPT := -g -O0
else
OPT := -O2
endif

WARN := -Wall -Wextra

# Each example only ever sees its own directory on the include path, so there
# is no way for one to accidentally pick up the other's header.
C_DIR   := examples/c
CPP_DIR := examples/cpp
GPU_DIR := examples/cpp-gpu
SC_DIR  := examples/cpp-gpu-shadercross

CFLAGS       := -std=c11   $(WARN) $(OPT) -I$(C_DIR)   $(SDL_CFLAGS)
CXXFLAGS     := -std=c++17 $(WARN) $(OPT) -I$(CPP_DIR) $(SDL_CFLAGS)
# The GPU demo gets its OWN private include path so the cpp-gpu folder can
# never see the cpp folder (independence rule — same as the C/C++ split).
GPU_CXXFLAGS := -std=c++17 $(WARN) $(OPT) -I$(GPU_DIR) $(SDL_CFLAGS)
# Same rule for the shadercross demo, plus shadercross's own cflags.
SC_CXXFLAGS  := -std=c++17 $(WARN) $(OPT) -I$(SC_DIR)  $(SDL_CFLAGS) $(SHADERCROSS_CFLAGS)
LDLIBS       := $(SDL_LIBS)

BUILD   := build
C_BIN   := $(BUILD)/demo_traditional
CPP_BIN := $(BUILD)/demo_callbacks
GPU_BIN := $(BUILD)/demo_gpu
SC_BIN  := $(BUILD)/demo_gpu_shadercross

# Where `make shaders` writes its cross-compiled output.
SC_SHADER_DIR := $(SC_DIR)/shaders
SC_SHADER_OUT := $(SC_SHADER_DIR)/build

.PHONY: all run-c run-cpp run-gpu run-shadercross smoke shaders shaders-hlsl \
        debug-c debug-cpp debug-gpu debug-shadercross \
        compdb clean check-sdl check-shadercross check-shadercross-bin check-glslang

# `all` includes the shadercross demo ONLY when shadercross is installed —
# otherwise just the original three, plus a friendly note pointing at the
# build instructions. This is what preserves the "three demos build with just
# `brew install sdl3`" promise.
ifeq ($(SHADERCROSS_OK),yes)
all: check-sdl $(C_BIN) $(CPP_BIN) $(GPU_BIN) $(SC_BIN)
else
all: check-sdl $(C_BIN) $(CPP_BIN) $(GPU_BIN)
	@echo
	@echo "NOTE: SDL_shadercross not found via pkg-config — skipping demo_gpu_shadercross."
	@echo "      See $(SC_DIR)/README.md to build SDL_shadercross from source."
	@echo
endif

# Fail early with a clear message if SDL3 isn't installed yet.
check-sdl:
	@pkg-config --exists sdl3 || { \
		echo "ERROR: SDL3 not found by pkg-config."; \
		echo "Install it first:  brew install sdl3"; \
		exit 1; }

# Same idea for SDL_shadercross — but only fires when you explicitly target
# `make run-shadercross` / `make debug-shadercross` / `make shaders`, never
# during the default `make` (which gracefully skips it instead).
check-shadercross: check-sdl
	@[ "$(SHADERCROSS_OK)" = "yes" ] || { \
		echo "ERROR: SDL_shadercross not found by pkg-config."; \
		echo "See $(SC_DIR)/README.md for build instructions on macOS."; \
		exit 1; }

# `make shaders` needs glslangValidator (Homebrew: brew install glslang).
check-glslang:
	@[ -n "$(GLSLANG_BIN)" ] || { \
		echo "ERROR: glslangValidator not on PATH."; \
		echo "Install it first:  brew install glslang"; \
		exit 1; }

# `make shaders-hlsl` needs the shadercross CLI built with DXC support.
# (The CLI may exist independently of the library .pc file in unusual installs,
# so it gets its own check separate from check-shadercross.)
check-shadercross-bin:
	@[ -n "$(SHADERCROSS_BIN)" ] || { \
		echo "ERROR: shadercross CLI not on PATH."; \
		echo "Build it from source — see $(SC_DIR)/README.md."; \
		exit 1; }

# Separate object subdirectories keep each example's artifacts disjoint.
$(BUILD)/c $(BUILD)/cpp $(BUILD)/cpp-gpu $(BUILD)/cpp-gpu-shadercross:
	mkdir -p $@

# --- C example: its own gfx.o, compiled and linked with the C compiler. ---
$(BUILD)/c/gfx.o: $(C_DIR)/gfx.c $(C_DIR)/gfx.h | $(BUILD)/c
	$(CC) $(CFLAGS) -c $< -o $@

$(C_BIN): $(C_DIR)/traditional.c $(BUILD)/c/gfx.o | $(BUILD)/c
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

# --- C++ example: its own gfx.o, compiled and linked with the C++ compiler.
#     Nothing here is shared with the C target above. ---
$(BUILD)/cpp/gfx.o: $(CPP_DIR)/gfx.cpp $(CPP_DIR)/gfx.hpp | $(BUILD)/cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(CPP_BIN): $(CPP_DIR)/callbacks.cpp $(BUILD)/cpp/gfx.o | $(BUILD)/cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

# --- C++ SDL_GPU example: a single self-contained translation unit (shaders
#     are embedded as strings, so there is no gfx and no second .o). Built
#     only from examples/cpp-gpu/, with that folder as its sole include path. ---
$(GPU_BIN): $(GPU_DIR)/triangle.cpp | $(BUILD)/cpp-gpu
	$(CXX) $(GPU_CXXFLAGS) $< -o $@ $(LDLIBS)

# --- C++ SDL_GPU + SDL_shadercross example: same shape as the GPU demo, plus
#     SDL_shadercross linkage. Only built when shadercross was detected (the
#     `ifeq` around `all` keeps this target out of the default build otherwise),
#     but the rule below is always defined so `make $(SC_BIN)` gives a real
#     error if you try to build it directly without shadercross. ---
$(SC_BIN): $(SC_DIR)/triangle_shadercross.cpp | $(BUILD)/cpp-gpu-shadercross
	$(CXX) $(SC_CXXFLAGS) $< -o $@ $(LDLIBS) $(SHADERCROSS_LIBS)

run-c: check-sdl $(C_BIN)
	./$(C_BIN)

run-cpp: check-sdl $(CPP_BIN)
	./$(CPP_BIN)

run-gpu: check-sdl $(GPU_BIN)
	./$(GPU_BIN)

run-shadercross: check-shadercross $(SC_BIN)
	./$(SC_BIN)

# Non-interactive: render 120 frames each and exit. Used to verify the build
# without a human closing windows. The shadercross demo joins only when present.
smoke: all
	./$(C_BIN)   --frames 120
	./$(CPP_BIN) --frames 120
	./$(GPU_BIN) --frames 120
ifeq ($(SHADERCROSS_OK),yes)
	./$(SC_BIN)  --frames 120
endif

# Offline-CLI workflow — DEFAULT (no DXC needed).
#
# Compiles the GLSL sources in shaders/*.glsl to SPIR-V using glslangValidator
# (Homebrew: brew install glslang). The .spv outputs are what
# `./demo_gpu_shadercross --mode spirv` (the default mode) loads at runtime;
# shadercross then transpiles each one into the device's native format
# (MSL/SPIRV/DXIL) — that transpile path is the part that does NOT need DXC.
#
# Authoring in HLSL instead? `make shaders-hlsl` below uses the shadercross
# CLI on the .hlsl sources; that path DOES need DXC linked into shadercross,
# which on macOS is a separate install — see $(SC_DIR)/README.md.
shaders: check-glslang
	@mkdir -p $(SC_SHADER_OUT)
	$(GLSLANG_BIN) -V $(SC_SHADER_DIR)/triangle.vert.glsl -o $(SC_SHADER_OUT)/triangle.vert.spv
	$(GLSLANG_BIN) -V $(SC_SHADER_DIR)/triangle.frag.glsl -o $(SC_SHADER_OUT)/triangle.frag.spv
	@echo
	@echo "Cross-compiled SPIR-V in $(SC_SHADER_OUT)/ (from GLSL via glslangValidator):"
	@ls -1 $(SC_SHADER_OUT)/*.spv | sed 's/^/  /'
	@echo
	@echo "Now run:  ./$(SC_BIN) --mode spirv"

# Offline-CLI workflow — HLSL variant (REQUIRES shadercross built with DXC).
#
# Same idea as `make shaders`, but starts from the .hlsl sources and exercises
# the shadercross CLI's HLSL pipeline. Produces SPIR-V plus the inspectable
# MSL / DXIL side-outputs that show what each backend natively consumes. Most
# users won't need this — the default `make shaders` is sufficient to run
# `--mode spirv`. Use this when you want to verify the HLSL workflow itself,
# or to see the per-backend bytecode side by side. MSL and DXIL lines are
# prefixed with `-` so the recipe degrades gracefully if a particular target
# isn't built into the local shadercross.
shaders-hlsl: check-shadercross-bin
	@mkdir -p $(SC_SHADER_OUT)
	$(SHADERCROSS_BIN) $(SC_SHADER_DIR)/triangle.vert.hlsl -s HLSL -d SPIRV -t vertex   -o $(SC_SHADER_OUT)/triangle.vert.spv
	$(SHADERCROSS_BIN) $(SC_SHADER_DIR)/triangle.frag.hlsl -s HLSL -d SPIRV -t fragment -o $(SC_SHADER_OUT)/triangle.frag.spv
	-$(SHADERCROSS_BIN) $(SC_SHADER_DIR)/triangle.vert.hlsl -s HLSL -d MSL   -t vertex   -o $(SC_SHADER_OUT)/triangle.vert.msl
	-$(SHADERCROSS_BIN) $(SC_SHADER_DIR)/triangle.frag.hlsl -s HLSL -d MSL   -t fragment -o $(SC_SHADER_OUT)/triangle.frag.msl
	-$(SHADERCROSS_BIN) $(SC_SHADER_DIR)/triangle.vert.hlsl -s HLSL -d DXIL  -t vertex   -o $(SC_SHADER_OUT)/triangle.vert.dxil
	-$(SHADERCROSS_BIN) $(SC_SHADER_DIR)/triangle.frag.hlsl -s HLSL -d DXIL  -t fragment -o $(SC_SHADER_OUT)/triangle.frag.dxil
	@echo
	@echo "Cross-compiled shaders in $(SC_SHADER_OUT)/ (from HLSL via shadercross):"
	@ls -1 $(SC_SHADER_OUT)/ | sed 's/^/  /'

# Force a fresh debug build (compiler flags aren't tracked by make, so we
# remove the artifacts first), then drop into lldb — the macOS debugger.
debug-c: check-sdl
	rm -f $(C_BIN) $(BUILD)/c/gfx.o
	$(MAKE) DEBUG=1 $(C_BIN)
	lldb ./$(C_BIN)

debug-cpp: check-sdl
	rm -f $(CPP_BIN) $(BUILD)/cpp/gfx.o
	$(MAKE) DEBUG=1 $(CPP_BIN)
	lldb ./$(CPP_BIN)

debug-gpu: check-sdl
	rm -f $(GPU_BIN)
	$(MAKE) DEBUG=1 $(GPU_BIN)
	lldb ./$(GPU_BIN)

debug-shadercross: check-shadercross
	rm -f $(SC_BIN)
	$(MAKE) DEBUG=1 $(SC_BIN)
	lldb ./$(SC_BIN)

# Editor IntelliSense (clangd / VS Code C/C++) needs a compilation database
# to know SDL3's include path and the per-file flags. CMake emits one when
# configured; we drop it in a dedicated subdir so it never collides with the
# Makefile's object trees, then symlink it to the repo root where clangd
# auto-discovers it. Re-run after `make clean` (which wipes build/).
compdb: check-sdl
	cmake -S . -B $(BUILD)/cmake >/dev/null
	ln -sf $(BUILD)/cmake/compile_commands.json compile_commands.json
	@echo "compile_commands.json ready - reload VS Code (or run: clangd: Restart language server)."

clean:
	rm -rf $(BUILD)
	rm -f compile_commands.json
