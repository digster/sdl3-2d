/*
 * triangle_bgfx.cpp — SDL3 + bgfx hello triangle (a full graphics pipeline).
 *
 * THE SIBLING `hello_bgfx.cpp` taught the SDL3↔bgfx platform wiring with no
 * shaders — just a cleared backbuffer and a debug-text overlay. This file
 * adds the next layer of bgfx machinery on top of that exact same wiring:
 *
 *     - a vertex layout describing per-vertex data on the CPU side,
 *     - a vertex buffer + index buffer uploaded to the GPU once at init,
 *     - a SHADER PROGRAM compiled offline (by `make bgfx-shaders`) and
 *       loaded as bytecode at runtime,
 *     - per-frame submit() of the program against view 0.
 *
 * The result on screen is the canonical "hello triangle": three vertices
 * (red, green, blue) with the rasterizer blending the colours smoothly across
 * the face. Same scene as examples/cpp-gpu/triangle.cpp, but rendered through
 * bgfx instead of SDL_GPU — diff the two side-by-side to see how the same
 * idea looks under two different GPU abstractions.
 *
 * HOW SHADERS GET HERE
 * --------------------
 * bgfx ships its own shader compiler, `shaderc`, that consumes a `.sc`
 * dialect (extended GLSL) and emits per-backend bytecode (Metal on macOS,
 * SPIR-V on Linux, DXBC on Windows). The two `.sc` sources live in this
 * folder under `shaders/`, and `make bgfx-shaders` invokes shaderc on them
 * to produce `shaders/build/metal/{vs,fs}_triangle.bin`. We then `SDL_LoadFile`
 * those .bin blobs at startup and hand them to `bgfx::createShader`.
 *
 * This split is intentional — shader compilation is offline (the way a real
 * shipped game does it), runtime just loads bytecode. The single-platform
 * default (Metal only) keeps the example focused on the bgfx pipeline; the
 * sibling shaders/README.md explains how to add SPIR-V / DXBC outputs for
 * cross-platform deployment.
 *
 * Run `./demo_bgfx_triangle`, or `./demo_bgfx_triangle --frames 120` for the
 * non-interactive smoke build. The example MUST be run from the repo root
 * (which is what `make run-bgfx-triangle` does) so the relative shader paths
 * below resolve.
 */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

#include <cstring>
#include <cstdlib>
#include <cstdint>

/* ──────────────────────────────────────────────────────────────────────────
 * The geometry. Three vertices in clip space [-1, +1], one per corner of the
 * triangle. Colour is packed as RGBA8 (4 bytes) because that is bgfx's most
 * compact common attribute type and matches the COLOR0 declaration in
 * shaders/varying.def.sc. The PosColorVertex layout matches what the vertex
 * shader expects byte-for-byte; bgfx::VertexLayout below describes that to
 * the GPU.
 *
 * The `[3]` arrays are `static`-storage so we can hand them to bgfx via
 * `bgfx::makeRef` — that path takes a non-owning reference rather than
 * copying. As long as the storage outlives the buffer handle (which here
 * means "for the lifetime of the program"), makeRef is the cheapest way to
 * upload static geometry.
 * ────────────────────────────────────────────────────────────────────────── */
struct PosColorVertex {
    float    x, y, z;
    uint32_t abgr;          /* packed colour; see ABGR(...) below */
};

/* bgfx's colour packing is ABGR (alpha-blue-green-red, little-endian) when
 * declared with AttribType::Uint8/normalised=true. The vertex shader sees it
 * as a vec4 already in 0..1 range. */
static constexpr uint32_t ABGR(uint8_t a, uint8_t b, uint8_t g, uint8_t r)
{
    return (uint32_t(a) << 24) | (uint32_t(b) << 16)
         | (uint32_t(g) <<  8) |  uint32_t(r);
}

static const PosColorVertex kVertices[3] = {
    {  0.0f,  0.5f, 0.0f, ABGR(0xff, 0x00, 0x00, 0xff) },   /* top   — red   */
    { -0.5f, -0.5f, 0.0f, ABGR(0xff, 0x00, 0xff, 0x00) },   /* left  — green */
    {  0.5f, -0.5f, 0.0f, ABGR(0xff, 0xff, 0x00, 0x00) },   /* right — blue  */
};

static const uint16_t kIndices[3] = { 0, 1, 2 };

/* AppState — everything we keep between frames. Same shape and ownership
 * model as hello_bgfx.cpp; the extra members below hold the GPU resources
 * we allocated at init and must release in SDL_AppQuit. */
struct AppState {
    SDL_Window               *window           = nullptr;
    int                       width            = 0;
    int                       height           = 0;
    bool                      bgfx_initialised = false;
    bool                      resize_pending   = false;

    bgfx::VertexBufferHandle  vbh   = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle   ibh   = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle       prog  = BGFX_INVALID_HANDLE;

    long max_frames = -1;
    long frame      = 0;
};

static SDL_AppResult fail(const char *what)
{
    SDL_Log("%s: %s", what, SDL_GetError());
    return SDL_APP_FAILURE;
}

/* Identical helper to hello_bgfx.cpp. Duplicated on purpose — the per-folder
 * isolation rule means no shared header between these two files. */
static void *get_native_window_handle(SDL_Window *window)
{
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
#if defined(__APPLE__)
    return SDL_GetPointerProperty(props,
                                  SDL_PROP_WINDOW_COCOA_WINDOW_POINTER,
                                  nullptr);
#else
    (void)props; return nullptr;
#endif
}

/* Load a pre-compiled bgfx shader binary from disk and wrap it in an
 * SDL3-owned buffer. The caller frees the bytes with SDL_free() — but in
 * practice we hand them straight to bgfx::copy(), which makes its own copy
 * for the GPU upload, so the caller can free immediately. Returns nullptr
 * on failure and logs a helpful "did you run `make bgfx-shaders`?" hint.
 *
 * The path is resolved relative to the current working directory (which is
 * the repo root when launched via `make run-bgfx-triangle`), the same
 * convention examples/cpp-gpu-shadercross/triangle_shadercross.cpp uses. */
static void *load_shader_blob(const char *basename, size_t *out_size)
{
    char path[256];
    /* Renderer subdir under shaders/build/ — Metal-only on macOS today; the
     * sibling shaders/README.md explains how to add others. */
    SDL_snprintf(path, sizeof(path),
                 "examples/cpp-bgfx/shaders/build/metal/%s", basename);

    void *bytes = SDL_LoadFile(path, out_size);
    if (!bytes) {
        SDL_Log("SDL_LoadFile(%s): %s", path, SDL_GetError());
        SDL_Log("  Run `make bgfx-shaders` first (compiles shaders/*.sc into the");
        SDL_Log("  per-renderer .bin files this demo loads at startup).");
    }
    return bytes;
}

/* One shader stage end-to-end: load .bin → bgfx::ShaderHandle.
 * Returns BGFX_INVALID_HANDLE on failure (loader already logged the cause).
 * bgfx::copy() takes ownership of the GPU-side bytes, so we can free the
 * SDL-allocated host buffer right after the createShader call. */
static bgfx::ShaderHandle load_shader(const char *basename)
{
    size_t size = 0;
    void  *bytes = load_shader_blob(basename, &size);
    if (!bytes) {
        return BGFX_INVALID_HANDLE;
    }

    /* bgfx::copy() duplicates the buffer into bgfx's internal memory pool
     * so the GPU upload can run on its own schedule. Pairs with bgfx::makeRef
     * (no copy, you keep the storage alive) which we use further down for
     * the static vertex/index data. */
    const bgfx::Memory *mem = bgfx::copy(bytes, static_cast<uint32_t>(size));
    SDL_free(bytes);

    bgfx::ShaderHandle sh = bgfx::createShader(mem);
    if (!bgfx::isValid(sh)) {
        SDL_Log("bgfx::createShader(%s): invalid handle (corrupt .bin?)", basename);
    }
    return sh;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata("SDL3 2D - bgfx triangle (C++)", "1.0",
                       "com.example.sdl3_2d");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return fail("SDL_Init");
    }

    AppState *st = new AppState();
    *appstate = st;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            st->max_frames = std::atol(argv[++i]);
        }
    }

    /* --- 1-4: same SDL3↔bgfx wiring sequence as hello_bgfx.cpp. The first
     *         four steps are identical between the two files on purpose: the
     *         per-folder isolation rule says no shared header, so duplication
     *         is the cost of independence (same rule as examples/c/ vs
     *         examples/cpp/ for the gfx helpers). See hello_bgfx.cpp for the
     *         narrated walkthrough of each step. ----------------------- */
    st->window = SDL_CreateWindow("SDL3 2D - bgfx triangle (C++)",
                                  800, 600, SDL_WINDOW_RESIZABLE);
    if (!st->window) {
        return fail("SDL_CreateWindow");
    }
    SDL_GetWindowSizeInPixels(st->window, &st->width, &st->height);

    bgfx::renderFrame();                                /* single-threaded mode */

    void *nwh = get_native_window_handle(st->window);
    if (!nwh) {
        SDL_Log("get_native_window_handle: no Cocoa window pointer.");
        return SDL_APP_FAILURE;
    }
    bgfx::PlatformData pd{};
    pd.nwh = nwh;
    pd.ndt = nullptr;

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

    /* Log the active backend — same pattern as hello_bgfx.cpp; makes
     * `make smoke` output prove this demo ran (the bgfx debug-text overlay
     * itself doesn't go to stdout). */
    SDL_Log("bgfx triangle: renderer = %s",
            bgfx::getRendererName(bgfx::getRendererType()));

    bgfx::setViewClear(0,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x121214ff, 1.0f, 0);

    /* --- 5. Declare the vertex layout. This is the CPU-side description of
     *        how each PosColorVertex above maps to the shader's $input
     *        attributes (a_position : POSITION, a_color0 : COLOR0). The
     *        begin/add/end fluent shape is bgfx-canonical. ------------- */
    bgfx::VertexLayout layout;
    layout.begin()
          .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
          .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8,
               /*normalized=*/true)
          .end();

    /* --- 6. Upload the geometry. makeRef hands bgfx a non-owning view of
     *        our static arrays — no copy, and we promise the storage lives
     *        as long as the buffer handle does (here: forever; the arrays
     *        are global statics). For dynamic / per-frame data you'd use
     *        a transient buffer instead (createTransientVertexBuffer). - */
    st->vbh = bgfx::createVertexBuffer(
        bgfx::makeRef(kVertices, sizeof(kVertices)),
        layout);
    st->ibh = bgfx::createIndexBuffer(
        bgfx::makeRef(kIndices, sizeof(kIndices)));
    if (!bgfx::isValid(st->vbh) || !bgfx::isValid(st->ibh)) {
        SDL_Log("bgfx vertex/index buffer creation returned invalid handle.");
        return SDL_APP_FAILURE;
    }

    /* --- 7. Load the two compiled shader stages and link them into a
     *        program. The 3rd arg of createProgram below — passing true —
     *        marks the two ShaderHandle inputs for automatic release when
     *        the program itself is destroyed, so we don't keep their handles
     *        around. -------------------------------------------------- */
    bgfx::ShaderHandle vsh = load_shader("vs_triangle.bin");
    bgfx::ShaderHandle fsh = load_shader("fs_triangle.bin");
    if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
        if (bgfx::isValid(vsh)) bgfx::destroy(vsh);
        if (bgfx::isValid(fsh)) bgfx::destroy(fsh);
        return SDL_APP_FAILURE;
    }
    st->prog = bgfx::createProgram(vsh, fsh, /*_destroyShaders=*/true);
    if (!bgfx::isValid(st->prog)) {
        SDL_Log("bgfx::createProgram: invalid program handle.");
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *e)
{
    AppState *st = static_cast<AppState *>(appstate);

    switch (e->type) {
    case SDL_EVENT_QUIT:
        return SDL_APP_SUCCESS;
    case SDL_EVENT_KEY_DOWN:
        if (e->key.scancode == SDL_SCANCODE_ESCAPE) {
            return SDL_APP_SUCCESS;
        }
        break;
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        SDL_GetWindowSizeInPixels(st->window, &st->width, &st->height);
        st->resize_pending = true;
        break;
    default:
        break;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *st = static_cast<AppState *>(appstate);

    if (st->resize_pending) {
        bgfx::reset(static_cast<uint32_t>(st->width),
                    static_cast<uint32_t>(st->height),
                    BGFX_RESET_VSYNC);
        st->resize_pending = false;
    }

    bgfx::setViewRect(0, 0, 0,
                      static_cast<uint16_t>(st->width),
                      static_cast<uint16_t>(st->height));

    /* The actual draw call sequence. Unlike hello_bgfx.cpp's bgfx::touch()
     * (which submits a no-op so the view's clear still happens), here we:
     *
     *   a) bind the vertex stream — slot 0, our full vertex buffer.
     *   b) bind the index buffer  — bgfx uses indexed drawing whenever an
     *      index buffer is set; sequential drawing is the alternative if you
     *      omit setIndexBuffer.
     *   c) set the render state    — BGFX_STATE_DEFAULT covers the things a
     *      hello-triangle wants (write RGB+A, write depth, depth test less,
     *      cull cw, msaa). It's the bgfx-canonical "boring opaque draw".
     *   d) submit the program against view 0 — this is what records the
     *      drawcall. bgfx batches and dispatches in bgfx::frame() below.
     */
    bgfx::setVertexBuffer(0, st->vbh);
    bgfx::setIndexBuffer(st->ibh);
    bgfx::setState(BGFX_STATE_DEFAULT);
    bgfx::submit(0, st->prog);

    /* Debug-text overlay — same trick as hello_bgfx.cpp; useful for
     * confirming the program loaded and the renderer is Metal. */
    bgfx::dbgTextClear();
    bgfx::dbgTextPrintf(0, 1, 0x0f, "SDL3 + bgfx triangle");
    bgfx::dbgTextPrintf(0, 2, 0x0f, "renderer : %s",
                        bgfx::getRendererName(bgfx::getRendererType()));
    bgfx::dbgTextPrintf(0, 3, 0x0f, "frame    : %ld", st->frame);

    bgfx::frame();

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

    /* Release GPU resources BEFORE bgfx::shutdown — they live inside the
     * device. The createProgram call further up passed `true` as its third
     * argument, which means destroying the program also destroys the two
     * ShaderHandles we passed it; we don't need separate destroy() calls
     * for those. */
    if (st->bgfx_initialised) {
        if (bgfx::isValid(st->prog)) bgfx::destroy(st->prog);
        if (bgfx::isValid(st->vbh))  bgfx::destroy(st->vbh);
        if (bgfx::isValid(st->ibh))  bgfx::destroy(st->ibh);
        bgfx::shutdown();
    }
    if (st->window) {
        SDL_DestroyWindow(st->window);
    }
    delete st;
}
