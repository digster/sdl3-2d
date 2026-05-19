# Makefile — primary build / run / debug driver for the SDL3 2D template.
#
# No CMake required: SDL3's compiler/linker flags are discovered via
# pkg-config (Homebrew installs sdl3.pc when you `brew install sdl3`).
#
# This template ships THREE fully independent examples that share NO source:
#
#   examples/c/        — C:   gfx.c + gfx.h + traditional.c (hand-written loop)
#   examples/cpp/      — C++: gfx.cpp + gfx.hpp + callbacks.cpp (SDL callbacks)
#   examples/cpp-gpu/  — C++: triangle.cpp (SDL_GPU pipeline, callbacks)
#
# Each demo is built ONLY from its own folder; the object trees live in
# separate build subdirectories and never cross. Copy any folder out and it
# stands alone.
#
# Targets:
#   make             build all three demos into build/    (release, -O2)
#   make run-c       build + run the C / traditional-loop demo
#   make run-cpp     build + run the C++ / callbacks demo
#   make run-gpu     build + run the C++ / SDL_GPU triangle demo
#   make smoke       build all, run each with --frames 120 (non-interactive)
#   make debug-c     rebuild the C demo with -g -O0, launch lldb
#   make debug-cpp   rebuild the C++ demo with -g -O0, launch lldb
#   make debug-gpu   rebuild the GPU demo with -g -O0, launch lldb
#   make compdb      (re)generate compile_commands.json for editors (clangd)
#   make clean       remove build/
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

CFLAGS       := -std=c11   $(WARN) $(OPT) -I$(C_DIR)   $(SDL_CFLAGS)
CXXFLAGS     := -std=c++17 $(WARN) $(OPT) -I$(CPP_DIR) $(SDL_CFLAGS)
# The GPU demo gets its OWN private include path so the cpp-gpu folder can
# never see the cpp folder (independence rule — same as the C/C++ split).
GPU_CXXFLAGS := -std=c++17 $(WARN) $(OPT) -I$(GPU_DIR) $(SDL_CFLAGS)
LDLIBS       := $(SDL_LIBS)

BUILD   := build
C_BIN   := $(BUILD)/demo_traditional
CPP_BIN := $(BUILD)/demo_callbacks
GPU_BIN := $(BUILD)/demo_gpu

.PHONY: all run-c run-cpp run-gpu smoke debug-c debug-cpp debug-gpu compdb clean check-sdl

all: check-sdl $(C_BIN) $(CPP_BIN) $(GPU_BIN)

# Fail early with a clear message if SDL3 isn't installed yet.
check-sdl:
	@pkg-config --exists sdl3 || { \
		echo "ERROR: SDL3 not found by pkg-config."; \
		echo "Install it first:  brew install sdl3"; \
		exit 1; }

# Separate object subdirectories keep each example's artifacts disjoint.
$(BUILD)/c $(BUILD)/cpp $(BUILD)/cpp-gpu:
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

run-c: check-sdl $(C_BIN)
	./$(C_BIN)

run-cpp: check-sdl $(CPP_BIN)
	./$(CPP_BIN)

run-gpu: check-sdl $(GPU_BIN)
	./$(GPU_BIN)

# Non-interactive: render 120 frames each and exit. Used to verify the build
# without a human closing windows.
smoke: all
	./$(C_BIN)   --frames 120
	./$(CPP_BIN) --frames 120
	./$(GPU_BIN) --frames 120

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
