# Makefile — primary build / run / debug driver for the SDL3 2D template.
#
# No CMake required: SDL3's compiler and linker flags are discovered via
# pkg-config (Homebrew installs sdl3.pc when you `brew install sdl3`). The
# shared draw helper (src/gfx.c) is compiled ONCE as C; the C example links it
# with clang, the C++ example with clang++ (so the C++ runtime is linked in).
# The symbol names line up across that boundary because gfx.h wraps the API in
# `extern "C"`.
#
# Targets:
#   make             build both demos into build/        (release, -O2)
#   make run-c       build + run the C / traditional-loop demo
#   make run-cpp     build + run the C++ / callbacks demo
#   make smoke       build both, run each with --frames 120 (non-interactive)
#   make debug-c     rebuild the C demo with -g -O0, launch lldb
#   make debug-cpp   rebuild the C++ demo with -g -O0, launch lldb
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

WARN     := -Wall -Wextra
COMMON   := $(WARN) $(OPT) -Iinclude $(SDL_CFLAGS)
CFLAGS   := -std=c11   $(COMMON)
CXXFLAGS := -std=c++17 $(COMMON)
LDLIBS   := $(SDL_LIBS)

BUILD   := build
C_BIN   := $(BUILD)/demo_traditional
CPP_BIN := $(BUILD)/demo_callbacks

.PHONY: all run-c run-cpp smoke debug-c debug-cpp clean check-sdl

all: check-sdl $(C_BIN) $(CPP_BIN)

# Fail early with a clear message if SDL3 isn't installed yet.
check-sdl:
	@pkg-config --exists sdl3 || { \
		echo "ERROR: SDL3 not found by pkg-config."; \
		echo "Install it first:  brew install sdl3"; \
		exit 1; }

$(BUILD):
	mkdir -p $(BUILD)

# Shared draw-helper object, compiled as C.
$(BUILD)/gfx.o: src/gfx.c include/gfx.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# C example: compiled and linked with the C compiler.
$(C_BIN): examples/traditional.c $(BUILD)/gfx.o | $(BUILD)
	$(CC) $(CFLAGS) $^ -o $@ $(LDLIBS)

# C++ example: compiled/linked with the C++ compiler; it consumes the
# C-compiled gfx.o (linkage works because gfx.h is extern "C").
$(CPP_BIN): examples/callbacks.cpp $(BUILD)/gfx.o | $(BUILD)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

run-c: check-sdl $(C_BIN)
	./$(C_BIN)

run-cpp: check-sdl $(CPP_BIN)
	./$(CPP_BIN)

# Non-interactive: render 120 frames each and exit. Used to verify the build
# without a human closing windows.
smoke: all
	./$(C_BIN)   --frames 120
	./$(CPP_BIN) --frames 120

# Force a fresh debug build (compiler flags aren't tracked by make, so we
# remove the artifacts first), then drop into lldb — the macOS debugger.
debug-c: check-sdl
	rm -f $(C_BIN) $(BUILD)/gfx.o
	$(MAKE) DEBUG=1 $(C_BIN)
	lldb ./$(C_BIN)

debug-cpp: check-sdl
	rm -f $(CPP_BIN) $(BUILD)/gfx.o
	$(MAKE) DEBUG=1 $(CPP_BIN)
	lldb ./$(CPP_BIN)

clean:
	rm -rf $(BUILD)
