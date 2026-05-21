/*
 * hello_bgfx.cpp — SDL3 + bgfx integration starter (the "hello window").
 *
 * THE OTHER GPU EXAMPLES in this repo (examples/cpp-gpu/, cpp-gpu-shadercross/)
 * draw with SDL3's own SDL_GPU API. This one swaps that backend out for
 *
 *     bgfx — https://github.com/bkaradzic/bgfx
 *
 * a long-running, BSD-2-licensed cross-platform rendering library that sits
 * between your code and the native graphics API (Metal here on macOS, Vulkan
 * on Linux, D3D12 on Windows, …). SDL3 still owns the window, input and the
 * main loop; bgfx takes everything below — device creation, the swapchain,
 * shaders, and command submission — and abstracts it under one C++ API.
 *
 * WHY ANOTHER GPU EXAMPLE
 * -----------------------
 * SDL_GPU and bgfx solve the same problem in two different idioms. Knowing
 * both is useful precisely because real projects pick one based on its design:
 *
 *     SDL_GPU — explicit, low-level, "you describe a pipeline; SDL drives it."
 *     bgfx   — view-based, higher-level, "you submit draw calls to views; bgfx
 *              schedules and dispatches them." Comes with its own shader
 *              language and shaderc tool. Far older and more battle-tested.
 *
 * The two demos in this folder are intentionally graduated:
 *
 *     hello_bgfx.cpp     (THIS FILE) — wire SDL3 ↔ bgfx, clear the screen,
 *                                       overlay debug text. No shader pipeline.
 *     triangle_bgfx.cpp              — same wiring + a full hello-triangle
 *                                       with vertex/index buffers and a shader
 *                                       program compiled offline by bgfx's
 *                                       `shaderc` tool.
 *
 * Start with this file: it teaches the SDL3-handoff and the bgfx lifecycle
 * (the genuinely new things). The triangle file then adds the shader-and-
 * buffer machinery on top of the exact same scaffolding.
 *
 * THE THREE PIECES OF SDL3↔BGFX WIRING
 * ------------------------------------
 *   1. Get the native window handle from SDL3 (on macOS that's the
 *      `NSWindow*`) and put it into `bgfx::PlatformData::nwh`. bgfx
 *      uses it to attach a Metal layer to the window's content view.
 *
 *   2. Call `bgfx::renderFrame()` ONCE *before* `bgfx::init()`. This is the
 *      documented opt-in to bgfx's single-threaded mode. By default bgfx
 *      spawns a render thread that owns the GPU, but that doesn't play well
 *      with SDL3's "everything on main thread" expectation on macOS — and
 *      single-threaded mode is plenty fast for a starter.
 *
 *   3. Hand the `PlatformData` to `bgfx::Init` and call `bgfx::init(init)`.
 *      From here on the bgfx API works the same as it would on any platform.
 *
 * Per-frame the lifecycle is unusually simple — no command buffers, no
 * render-pass begin/end. You set up the view (clear colour + rect), submit
 * any draw calls you want (none here — just `bgfx::touch()` so the view is
 * processed), and `bgfx::frame()` flushes everything to the GPU and presents.
 *
 * Loop style mirrors examples/cpp/callbacks.cpp and cpp-gpu/triangle.cpp
 * (SDL_MAIN_USE_CALLBACKS), so all three C++ examples read consistently.
 *
 * Run `./demo_bgfx` for a window with the active renderer name + frame count
 * printed on top, or `./demo_bgfx --frames 120` to render 120 frames and exit
 * (used by `make smoke`).
 */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>      // bgfx::PlatformData, bgfx::renderFrame

#include <cstring>              // std::strcmp
#include <cstdlib>              // std::atol

/* ──────────────────────────────────────────────────────────────────────────
 * AppState — everything we need between frames.
 *
 * Same shape as the other C++ examples in this repo: allocated in
 * SDL_AppInit, freed in SDL_AppQuit. SDL stores the pointer for us and
 * threads it back into every callback as `appstate`, so there are no globals.
 * The bool fields below are for the resize handler — bgfx is initialised
 * with the window's current size, and we mirror that here so we can call
 * bgfx::reset(...) on the next iterate after the OS tells us the window
 * changed size.
 * ────────────────────────────────────────────────────────────────────────── */
struct AppState {
    SDL_Window *window = nullptr;
    int         width  = 0;       // current backbuffer width  (px)
    int         height = 0;       // current backbuffer height (px)
    bool        bgfx_initialised = false;  // for SDL_AppQuit's clean tear-down
    bool        resize_pending   = false;  // event queued; reset on next iterate
    long        max_frames = -1;           // -1 = run until quit; set by --frames
    long        frame      = 0;
};

/* Small helper: log an SDL error with context. Keeps the init-failure paths
 * below short and uniform — identical convention to cpp-gpu/triangle.cpp. */
static SDL_AppResult fail(const char *what)
{
    SDL_Log("%s: %s", what, SDL_GetError());
    return SDL_APP_FAILURE;
}

/* Pull the native window handle out of SDL3 for bgfx.
 *
 * SDL3 exposes platform-specific window pointers through the property API.
 * On macOS (Cocoa) the relevant property is the NSWindow*; bgfx grabs its
 * contentView internally and attaches a CAMetalLayer to it. The Linux and
 * Windows builds would use SDL_PROP_WINDOW_X11_WINDOW_NUMBER or
 * SDL_PROP_WINDOW_WIN32_HWND_POINTER respectively — left as a comment so the
 * shape of the per-platform branch is visible even though we don't compile
 * it on macOS.
 *
 * Returns nullptr if SDL hasn't realised a Cocoa window yet — caller should
 * treat that as a fatal init error rather than handing nullptr to bgfx. */
static void *get_native_window_handle(SDL_Window *window)
{
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
#if defined(__APPLE__)
    return SDL_GetPointerProperty(props,
                                  SDL_PROP_WINDOW_COCOA_WINDOW_POINTER,
                                  nullptr);
#elif defined(__linux__)
    /* return reinterpret_cast<void *>(
     *     SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0)); */
    (void)props; return nullptr;
#elif defined(_WIN32)
    /* return SDL_GetPointerProperty(props,
     *     SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr); */
    (void)props; return nullptr;
#else
    (void)props; return nullptr;
#endif
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata("SDL3 2D - bgfx hello (C++)", "1.0",
                       "com.example.sdl3_2d");

    if (!SDL_Init(SDL_INIT_VIDEO)) {            // SDL3: true == success
        return fail("SDL_Init");
    }

    AppState *st = new AppState();
    *appstate = st;                             // store early so SDL_AppQuit can clean up

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            st->max_frames = std::atol(argv[++i]);
        }
    }

    /* 1. A plain SDL3 window — bgfx will attach its rendering surface to it.
     *    No SDL_WINDOW_METAL flag is needed; bgfx creates the Metal layer
     *    itself when it sees the NSWindow* below. The window is resizable so
     *    the bgfx::reset path further down gets exercised. */
    st->window = SDL_CreateWindow("SDL3 2D - bgfx hello (C++)",
                                  800, 600, SDL_WINDOW_RESIZABLE);
    if (!st->window) {
        return fail("SDL_CreateWindow");
    }
    SDL_GetWindowSizeInPixels(st->window, &st->width, &st->height);

    /* 2. Single-threaded mode — see the file header. This MUST be called
     *    before bgfx::init() to take effect: a single renderFrame() call
     *    here is bgfx's documented signal that the application will drive
     *    rendering on the main thread itself, so bgfx should not spawn its
     *    own render thread. Skipping this on macOS leads to a noticeable
     *    shutdown stall while bgfx joins its worker. */
    bgfx::renderFrame();

    /* 3. Build the PlatformData struct that bgfx needs to talk to the
     *    operating system. `nwh` is the native window handle (NSWindow* on
     *    macOS) — the one piece of platform state SDL3 can give us that
     *    bgfx doesn't already know how to find on its own. `ndt` is the
     *    native display type, used on Linux for the X11 Display* or Wayland
     *    wl_display* — on Cocoa it is unused. The other fields are for
     *    advanced backends (custom Metal layer, swapchain, render context)
     *    and stay zero for a normal app. */
    void *nwh = get_native_window_handle(st->window);
    if (!nwh) {
        SDL_Log("get_native_window_handle: SDL3 returned a NULL native "
                "handle — the window probably isn't realised yet, or this "
                "platform isn't supported by the wiring in this example.");
        return SDL_APP_FAILURE;
    }
    bgfx::PlatformData pd{};
    pd.nwh = nwh;
    pd.ndt = nullptr;

    /* 4. Initialise bgfx. `RendererType::Count` means "let bgfx auto-select"
     *    — on macOS that resolves to Metal. `BGFX_RESET_VSYNC` requests a
     *    vsync'd backbuffer; bgfx pairs this with reset() on resize below.
     *    Debug flag enables internal logging and the debug-text overlay
     *    (which is otherwise inert even if we call dbgTextPrintf). */
    bgfx::Init init;
    init.type                 = bgfx::RendererType::Count;
    init.resolution.width     = static_cast<uint32_t>(st->width);
    init.resolution.height    = static_cast<uint32_t>(st->height);
    init.resolution.reset     = BGFX_RESET_VSYNC;
    init.platformData         = pd;
    init.debug                = true;
    if (!bgfx::init(init)) {
        return fail("bgfx::init");
    }
    st->bgfx_initialised = true;
    bgfx::setDebug(BGFX_DEBUG_TEXT);

    /* Log which native backend bgfx picked. Useful at a glance — and means
     * `make smoke` output unambiguously proves this demo ran. */
    SDL_Log("bgfx hello: renderer = %s",
            bgfx::getRendererName(bgfx::getRendererType()));

    /* 5. Configure view 0 — bgfx's "view" is a logical drawing target with a
     *    clear colour, viewport rect and (later) framebuffer attachment. A
     *    real game uses many views (shadow pass, gbuffer, post, UI); we use
     *    just view 0 for the backbuffer. The clear colour is 0xRRGGBBAA;
     *    0x121214ff is a dark slate close to the GPU example's clear colour
     *    so the two demos look visually similar. */
    bgfx::setViewClear(0,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       /*rgba=*/0x121214ff,
                       /*depth=*/1.0f,
                       /*stencil=*/0);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *e)
{
    AppState *st = static_cast<AppState *>(appstate);

    switch (e->type) {
    case SDL_EVENT_QUIT:                                /* window close button */
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
        SDL_Log("key down: scancode=%d", (int)e->key.scancode);
        if (e->key.scancode == SDL_SCANCODE_ESCAPE) {
            return SDL_APP_SUCCESS;
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        SDL_Log("mouse button %d at (%.0f, %.0f)",
                (int)e->button.button, e->button.x, e->button.y);
        break;
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        /* Defer the actual bgfx::reset to SDL_AppIterate. Calling it from
         * inside an event callback is fine on macOS, but doing it once per
         * iterate keeps the lifecycle linear and avoids re-resetting on
         * back-to-back resize events while the user is dragging the corner. */
        SDL_GetWindowSizeInPixels(st->window, &st->width, &st->height);
        st->resize_pending = true;
        SDL_Log("window resized to %dx%d (px)", st->width, st->height);
        break;
    default:
        break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *st = static_cast<AppState *>(appstate);

    /* Apply any pending resize before drawing. bgfx::reset re-creates the
     * swapchain and resizes the backbuffer; everything else (views, shaders,
     * buffers) stays valid across the call. */
    if (st->resize_pending) {
        bgfx::reset(static_cast<uint32_t>(st->width),
                    static_cast<uint32_t>(st->height),
                    BGFX_RESET_VSYNC);
        st->resize_pending = false;
    }

    /* Tell view 0 to cover the whole backbuffer. Cheap to re-set every frame;
     * doing so keeps the viewport correct across resizes without any extra
     * bookkeeping. */
    bgfx::setViewRect(0, 0, 0,
                      static_cast<uint16_t>(st->width),
                      static_cast<uint16_t>(st->height));

    /* `touch()` submits a no-op draw to the named view. Without at least one
     * submit or touch per frame, bgfx is allowed to skip processing the view
     * entirely — meaning the clear colour we set in init() would never reach
     * the screen. (The triangle file's real submit() makes this implicit; we
     * have to opt in explicitly because we don't draw anything.) */
    bgfx::touch(0);

    /* Debug-text overlay — bgfx's built-in 8x16 bitmap font. dbgTextClear()
     * wipes any leftover text from the previous frame so the lines below
     * don't double up. Attribute 0x0f means "white foreground, transparent
     * background"; the bgfx debug palette is a classic 16-colour DOS one. */
    bgfx::dbgTextClear();
    bgfx::dbgTextPrintf(0, 1, 0x0f, "SDL3 + bgfx hello");
    bgfx::dbgTextPrintf(0, 2, 0x0f, "renderer : %s",
                        bgfx::getRendererName(bgfx::getRendererType()));
    bgfx::dbgTextPrintf(0, 3, 0x0f, "backbuffer: %d x %d px",
                        st->width, st->height);
    bgfx::dbgTextPrintf(0, 4, 0x0f, "frame    : %ld", st->frame);

    /* Flush all submitted work to the GPU and present. bgfx::frame() returns
     * a monotonic frame counter, but we don't use it — we keep our own so
     * the --frames N exit path doesn't depend on a bgfx internal. */
    bgfx::frame();

    /* Always tick the frame counter (we display it above), then check the
     * --frames N auto-quit limit. Same convention as cpp-gpu/triangle.cpp,
     * just hoisted out of the if so the debug-text counter stays live in
     * interactive mode too. */
    ++st->frame;
    if (st->max_frames >= 0 && st->frame >= st->max_frames) {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    AppState *st = static_cast<AppState *>(appstate);
    if (!st) {
        return;
    }

    /* Tear down in reverse order of creation. bgfx::shutdown() releases the
     * GPU device and the swapchain it created from our NSWindow*; only after
     * that is it safe to destroy the SDL window itself. We do NOT call
     * SDL_Quit(): the SDL_MAIN_USE_CALLBACKS runtime calls it after this
     * returns (same contract as examples/cpp/callbacks.cpp and
     * examples/cpp-gpu/triangle.cpp). */
    if (st->bgfx_initialised) {
        bgfx::shutdown();
    }
    if (st->window) {
        SDL_DestroyWindow(st->window);
    }
    delete st;
}
