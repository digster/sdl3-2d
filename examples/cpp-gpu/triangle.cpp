/*
 * triangle.cpp — SDL3 GPU graphics-pipeline starter (the "hello triangle").
 *
 * The other two examples in this repo (examples/c/, examples/cpp/) draw with
 * SDL3's high-level 2D API, SDL_Renderer. This one is deliberately a tier
 * lower: it uses SDL3's **SDL_GPU** API, the modern abstraction that sits on
 * top of the platform's real graphics API — Metal here on macOS, Vulkan on
 * Linux, Direct3D 12 on Windows. SDL_GPU is what lets you write one explicit
 * GPU pipeline and have SDL translate it to whatever the machine actually has.
 *
 * A "graphics pipeline" is the fixed sequence the GPU runs to turn vertices
 * into pixels:
 *
 *     vertices ─► [ vertex shader ] ─► rasterizer ─► [ fragment shader ] ─► pixels
 *
 * The whole point of this file is to build that pipeline once and then drive
 * the per-frame command sequence it needs. The scene itself is intentionally
 * the simplest thing that exercises every required stage: one triangle whose
 * three corners are red / green / blue, with the colours smoothly interpolated
 * across the face by the rasterizer.
 *
 * SHADERS, AND WHY THEY ARE EMBEDDED AS TEXT
 * ------------------------------------------
 * A pipeline needs a compiled vertex shader and fragment shader. SDL_GPU can
 * consume several shader formats (SPIR-V, DXIL, MSL, ...). SPIR-V would force
 * an offline shader-compiler toolchain (glslang / shadercross) into the build
 * — which would break this template's core promise of "just `brew install
 * sdl3`, nothing else." Instead we hand SDL the Metal Shading Language source
 * directly (SDL_GPU_SHADERFORMAT_MSL); the macOS Metal backend compiles it at
 * runtime. The shaders therefore live below as commented string literals — no
 * extra files, no extra build step, this folder still copies out and builds
 * on its own like the other examples.
 *
 * There is no gfx.hpp here on purpose: SDL_GPU and SDL_Renderer are mutually
 * exclusive ways to draw, so this example shares no source with the others.
 *
 * Loop style mirrors examples/cpp/callbacks.cpp (SDL_MAIN_USE_CALLBACKS), so
 * the two C++ examples read consistently and this one stays portable to
 * web/mobile later.
 *
 * Run `./demo_gpu` for a window, or `./demo_gpu --frames 120` to render 120
 * frames then exit (used by `make smoke`; works even with no display because
 * a NULL swapchain frame is handled gracefully — see SDL_AppIterate).
 */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>     // provides the real entry point (also on macOS)

#include <cstring>            // std::strcmp
#include <cstdlib>            // std::atol

/* ──────────────────────────────────────────────────────────────────────────
 * The two shader stages of the pipeline, written in Metal Shading Language.
 *
 * Note there is NO vertex buffer anywhere in this program. The three vertices
 * are baked into the vertex shader and selected by `[[vertex_id]]` — the index
 * (0,1,2) of the vertex currently being processed. This is the leanest
 * possible pipeline: it keeps the focus on the pipeline/stage machinery itself
 * rather than on GPU resource upload (vertex/transfer buffers), which is a
 * separate topic. SDL_DrawGPUPrimitives(pass, 3, ...) below is what makes the
 * vertex shader run three times, with vid = 0, 1, 2.
 * ────────────────────────────────────────────────────────────────────────── */

/* VERTEX STAGE — runs once per vertex. Its job is to output a clip-space
 * position (the special [[position]] output the rasterizer needs) plus any
 * per-vertex data we want interpolated for the fragment stage (here, colour).
 *
 * Metal/SDL_GPU clip space: x,y in [-1,+1], +y is UP, so the apex (0, 0.5) is
 * the top of the screen and the base sits below it. */
static const char *kVertexShaderMSL = R"msl(
#include <metal_stdlib>
using namespace metal;

struct VSOut {
    float4 position [[position]];   // required: where this vertex lands in clip space
    float4 color;                   // passed down the pipeline; rasterizer interpolates it
};

vertex VSOut vertexMain(uint vid [[vertex_id]])
{
    // Corner positions of the triangle, in clip space.
    const float2 positions[3] = {
        float2( 0.0f,  0.5f),       // vid 0 — top   (red)
        float2(-0.5f, -0.5f),       // vid 1 — left  (green)
        float2( 0.5f, -0.5f)        // vid 2 — right (blue)
    };
    const float3 colors[3] = {
        float3(1.0f, 0.0f, 0.0f),
        float3(0.0f, 1.0f, 0.0f),
        float3(0.0f, 0.0f, 1.0f)
    };

    VSOut out;
    out.position = float4(positions[vid], 0.0f, 1.0f);  // z=0, w=1 (no projection)
    out.color    = float4(colors[vid], 1.0f);
    return out;
}
)msl";

/* FRAGMENT STAGE — runs once per pixel the triangle covers. `in` arrives
 * already interpolated across the three vertices by the rasterizer (so a pixel
 * halfway between the red and green corners receives a yellowish colour for
 * free). We just return it as the final pixel colour. */
static const char *kFragmentShaderMSL = R"msl(
#include <metal_stdlib>
using namespace metal;

struct VSOut {
    float4 position [[position]];
    float4 color;
};

fragment float4 fragmentMain(VSOut in [[stage_in]])
{
    return in.color;
}
)msl";

/* Everything the app needs between frames. Allocated in SDL_AppInit, freed in
 * SDL_AppQuit. SDL stores this pointer for us and threads it back into every
 * callback as `appstate` — so, like callbacks.cpp, there are no globals. */
struct AppState {
    SDL_Window              *window   = nullptr;
    SDL_GPUDevice           *device   = nullptr;  // the abstracted GPU
    SDL_GPUGraphicsPipeline *pipeline = nullptr;  // built once, bound every frame
    long max_frames = -1;                         // -1 = run until quit; set by --frames
    long frame      = 0;
};

/* Small helper: log an SDL error with context. Keeps the init failure paths
 * below short and uniform. */
static SDL_AppResult fail(const char *what)
{
    SDL_Log("%s: %s", what, SDL_GetError());
    return SDL_APP_FAILURE;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata("SDL3 2D - GPU triangle (C++)", "1.0",
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

    /* 1. Create the GPU device. We advertise that we will supply MSL shaders;
     *    on macOS that selects the Metal backend. `debug_mode = true` turns on
     *    the backend's validation/labels — invaluable while learning, and the
     *    cost is irrelevant for a starter. `name = NULL` lets SDL pick the
     *    backend (only Metal is viable on macOS anyway). */
    st->device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL,
                                     /*debug_mode=*/true, /*name=*/nullptr);
    if (!st->device) {
        return fail("SDL_CreateGPUDevice");
    }

    /* 2. A plain window — note SDL_CreateWindow, NOT SDL_CreateWindowAndRenderer.
     *    The SDL_GPU path has no SDL_Renderer; the drawable surface (swapchain)
     *    is attached by claiming the window for the device, next step. No
     *    special window flag is needed for GPU. */
    st->window = SDL_CreateWindow("SDL3 2D - GPU triangle (C++)",
                                  800, 600, SDL_WINDOW_RESIZABLE);
    if (!st->window) {
        return fail("SDL_CreateWindow");
    }

    /* 3. Bind the window to the device. This creates the swapchain — the
     *    rotating set of textures we draw into and the OS shows on screen.
     *    A resize is handled by SDL automatically from here on, so the
     *    pipeline below never needs rebuilding for a resize. */
    if (!SDL_ClaimWindowForGPUDevice(st->device, st->window)) {
        return fail("SDL_ClaimWindowForGPUDevice");
    }

    /* 4. Compile the two shader stages from the MSL source above. The count
     *    fields (samplers / storage / uniform buffers) are all 0 because these
     *    shaders take no resources — they are pure functions of vertex_id. */
    SDL_GPUShaderCreateInfo vsi{};
    vsi.code        = reinterpret_cast<const Uint8 *>(kVertexShaderMSL);
    vsi.code_size   = std::strlen(kVertexShaderMSL);
    vsi.entrypoint  = "vertexMain";             // must match the MSL function name
    vsi.format      = SDL_GPU_SHADERFORMAT_MSL;
    vsi.stage       = SDL_GPU_SHADERSTAGE_VERTEX;
    SDL_GPUShader *vs = SDL_CreateGPUShader(st->device, &vsi);
    if (!vs) {
        return fail("SDL_CreateGPUShader (vertex)");
    }

    SDL_GPUShaderCreateInfo fsi{};
    fsi.code        = reinterpret_cast<const Uint8 *>(kFragmentShaderMSL);
    fsi.code_size   = std::strlen(kFragmentShaderMSL);
    fsi.entrypoint  = "fragmentMain";
    fsi.format      = SDL_GPU_SHADERFORMAT_MSL;
    fsi.stage       = SDL_GPU_SHADERSTAGE_FRAGMENT;
    SDL_GPUShader *fs = SDL_CreateGPUShader(st->device, &fsi);
    if (!fs) {
        SDL_ReleaseGPUShader(st->device, vs);
        return fail("SDL_CreateGPUShader (fragment)");
    }

    /* 5. Build the graphics pipeline — the immutable description of HOW to
     *    draw: which shaders, what primitive, what the colour target looks
     *    like. Created once here, simply bound each frame.
     *
     *    The pipeline's colour-target format MUST match the swapchain's, so we
     *    query it rather than hardcoding (it varies by platform/display).
     *
     *    vertex_input_state is left zero — no vertex buffers, because the
     *    vertices live in the shader (see the MSL note above). Everything
     *    zero-initialized (rasterizer / multisample / depth-stencil) gives the
     *    correct defaults for one opaque triangle: solid fill, no culling, 1
     *    sample, no depth test. */
    SDL_GPUColorTargetDescription color_target{};
    color_target.format = SDL_GetGPUSwapchainTextureFormat(st->device,
                                                           st->window);

    SDL_GPUGraphicsPipelineCreateInfo pci{};
    pci.vertex_shader                       = vs;
    pci.fragment_shader                     = fs;
    pci.primitive_type                      = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pci.target_info.color_target_descriptions = &color_target;
    pci.target_info.num_color_targets       = 1;

    st->pipeline = SDL_CreateGPUGraphicsPipeline(st->device, &pci);

    /* The pipeline now owns its compiled stages, so the standalone shader
     * objects can be released immediately whether or not creation succeeded. */
    SDL_ReleaseGPUShader(st->device, vs);
    SDL_ReleaseGPUShader(st->device, fs);

    if (!st->pipeline) {
        return fail("SDL_CreateGPUGraphicsPipeline");
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *e)
{
    (void)appstate;                             // unused in this callback

    switch (e->type) {
    case SDL_EVENT_QUIT:                        // window close button
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
        SDL_Log("window resized to %dx%d",
                (int)e->window.data1, (int)e->window.data2);
        break;
    default:
        break;
    }
    return SDL_APP_CONTINUE;                    // keep running
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *st = static_cast<AppState *>(appstate);

    /* The per-frame command sequence. Unlike SDL_Renderer (where draw calls
     * execute as you make them), SDL_GPU records commands into a command
     * buffer and submits them to the GPU in one go. */

    /* a. Get a command buffer to record into. */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(st->device);
    if (!cmd) {
        return fail("SDL_AcquireGPUCommandBuffer");
    }

    /* b. Get the swapchain texture to draw into for this frame.
     *
     *    FOOTGUN: this can RETURN SUCCESS YET HAND BACK A NULL texture — the
     *    window is minimized/occluded, or (relevant to `make smoke`) there is
     *    no display at all. NULL is not an error; there is simply nothing to
     *    draw to this frame. We must still submit the (empty) command buffer
     *    and carry on, otherwise the frame leaks and headless smoke runs would
     *    fail. Only a `false` return is a real error. */
    SDL_GPUTexture *swap = nullptr;
    Uint32 sw = 0, sh = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, st->window,
                                               &swap, &sw, &sh)) {
        return fail("SDL_WaitAndAcquireGPUSwapchainTexture");
    }

    if (swap) {
        /* c. Begin a render pass targeting that texture. LOAD_OP_CLEAR wipes
         *    it to the clear colour first; STORE_OP_STORE keeps what we draw
         *    so it can be presented. */
        SDL_GPUColorTargetInfo cti{};
        cti.texture     = swap;
        cti.clear_color = SDL_FColor{ 0.07f, 0.07f, 0.10f, 1.0f };  // dark slate
        cti.load_op     = SDL_GPU_LOADOP_CLEAR;
        cti.store_op    = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &cti, 1, nullptr);

        /* d. Bind the pipeline and draw. 3 vertices, 1 instance: the vertex
         *    shader runs for vid = 0,1,2 and the rasterizer fills the triangle
         *    between them. This is the entire scene. */
        SDL_BindGPUGraphicsPipeline(pass, st->pipeline);
        SDL_DrawGPUPrimitives(pass, /*num_vertices=*/3, /*num_instances=*/1,
                              /*first_vertex=*/0, /*first_instance=*/0);

        SDL_EndGPURenderPass(pass);
    }

    /* e. Submit. With a real swapchain texture this also presents the frame;
     *    with a NULL one (above) it just retires the empty command buffer. */
    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        return fail("SDL_SubmitGPUCommandBuffer");
    }

    /* Optional auto-quit for non-interactive smoke runs (--frames N). */
    if (st->max_frames >= 0 && ++st->frame >= st->max_frames) {
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

    /* Release in reverse order of creation. SDL_GPU objects are released
     * through the device. We do NOT call SDL_Quit(): the
     * SDL_MAIN_USE_CALLBACKS runtime calls it for us after this returns
     * (same contract as examples/cpp/callbacks.cpp). */
    if (st->device) {
        SDL_ReleaseGPUGraphicsPipeline(st->device, st->pipeline);
        if (st->window) {
            SDL_ReleaseWindowFromGPUDevice(st->device, st->window);
        }
        SDL_DestroyGPUDevice(st->device);
    }
    SDL_DestroyWindow(st->window);
    delete st;
}
