/*
 * triangle_shadercross.cpp — SDL3 GPU + SDL_shadercross cross-platform shaders.
 *
 * THE SIBLING `examples/cpp-gpu/triangle.cpp` draws the same gradient triangle,
 * but ships its shaders as Metal Shading Language *source text* — which means
 * that binary only runs on the macOS Metal backend. To make the same code work
 * on Vulkan (Linux) it would need SPIR-V bytecode, and on D3D12 (Windows) it
 * would need DXIL — different formats produced by different toolchains.
 *
 * THIS EXAMPLE does the portable version. It uses SDL's official cross-compiler
 *
 *     SDL_shadercross — https://github.com/libsdl-org/SDL_shadercross
 *
 * to author the two shaders **once** in HLSL and have the right native format
 * appear at the call to SDL_CreateGPUShader on every backend:
 *
 *     HLSL source  ──► CompileSPIRVFromHLSL ──► SPIR-V bytes
 *                                                    │
 *                                                    ▼
 *                                CompileGraphicsShaderFromSPIRV
 *                                                    │
 *                            ┌───────────────────────┼───────────────────────┐
 *                            ▼                       ▼                       ▼
 *                     SPIR-V (Vulkan)         MSL (Metal)            DXIL (D3D12)
 *
 * The library dispatches transparently — your code says "give me a
 * SDL_GPUShader from these SPIR-V bytes" and shadercross transpiles to the
 * format the device actually wants. One source, every platform.
 *
 * TWO MODES, FOR TEACHING THE WHOLE WORKFLOW
 * ------------------------------------------
 * Real engines do shader compilation **offline** (at build time, via a CLI),
 * then ship the bytecode and load it at runtime. The shadercross library also
 * supports compiling HLSL **at runtime** so a quick teaching example can carry
 * the whole pipeline in one binary. This example does both:
 *
 *   --mode spirv  (default)
 *       Loads pre-compiled SPIR-V from shaders/build/triangle.{vert,frag}.spv
 *       (produced by `make shaders`, which uses glslangValidator on the .glsl
 *       sources — no DXC needed) and hands it to shadercross for the
 *       backend-native dispatch. This is what a shipped game does.
 *
 *   --mode runtime
 *       Embeds the HLSL source as a string (see kVertexShaderHLSL below) and
 *       calls SDL_ShaderCross_CompileSPIRVFromHLSL at startup. Single binary,
 *       no extra files at run time — BUT requires shadercross to have been
 *       built with DXC support (HLSL→SPIRV goes through DirectXShaderCompiler).
 *       See ./README.md for the macOS DXC install steps.
 *
 * Either way, both stages then go through
 * SDL_ShaderCross_CompileGraphicsShaderFromSPIRV, which is the one function
 * that handles the cross-platform dispatch. Compare the two modes side by side
 * to see how thin the difference really is — the cross-platform piece is the
 * second function call, not the first.
 *
 * WHY THIS EXAMPLE IS THE ONE EXCEPTION
 * -------------------------------------
 * The other three examples in this repo build on just `brew install sdl3`.
 * SDL_shadercross is *not* in Homebrew (it depends on SPIRV-Cross and
 * optionally DXC, and there is no formula). Building it from source is a small
 * but real extra step — see ./README.md in this folder for the macOS recipe.
 *
 * The top-level Makefile / CMake detect SDL_shadercross at configure time and
 * **skip this target cleanly when it is missing** — the other three demos keep
 * working unchanged. So this folder's extra dependency stays opt-in.
 *
 * Loop and lifecycle mirror examples/cpp-gpu/triangle.cpp deliberately: same
 * SDL_MAIN_USE_CALLBACKS, same AppState shape, same NULL-swapchain handling,
 * same --frames N for smoke runs. The only meaningful diff between the two
 * files is *how shaders enter the pipeline*. Diffing them is a good way to
 * see that addition in isolation.
 *
 * Run `make shaders && ./demo_gpu_shadercross` for the default SPIRV mode
 * (the shaders/build/triangle.vert.spv and triangle.frag.spv files must
 * exist first). Or, if your local shadercross was built with DXC,
 * `./demo_gpu_shadercross --mode runtime` to exercise the HLSL-string path.
 * Add `--frames 120` for a non-interactive smoke run either way.
 */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>                  // real entry point on macOS
#include <SDL3_shadercross/SDL_shadercross.h>

#include <cstring>                          // std::strcmp
#include <cstdlib>                          // std::atol
#include <string>                           // std::string (for SPIR-V path build)

/* ──────────────────────────────────────────────────────────────────────────
 * The two shader stages, authored in HLSL.
 *
 * These strings are byte-for-byte the same as the .hlsl files in shaders/ —
 * shipped twice on purpose. The strings power --mode runtime; the files power
 * --mode spirv (via the shadercross CLI in `make shaders`). Edit both in sync.
 *
 * HLSL semantics (SV_VertexID / SV_Position / SV_Target / COLOR0) are what
 * make this portable: DXC emits SPIR-V with the right `BuiltIn` decorations,
 * SPIRV-Cross then translates those to Metal's [[position]] / [[vertex_id]] /
 * [[color(0)]] or to GLSL's gl_Position. Without semantics, cross-compilation
 * would have to guess intent. See shaders/triangle.vert.hlsl for the longer
 * commentary on each line.
 * ────────────────────────────────────────────────────────────────────────── */

static const char *kVertexShaderHLSL = R"hlsl(
struct VSOut {
    float4 position : SV_Position;
    float4 color    : COLOR0;
};

VSOut main(uint vid : SV_VertexID)
{
    float2 positions[3] = {
        float2( 0.0f,  0.5f),
        float2(-0.5f, -0.5f),
        float2( 0.5f, -0.5f)
    };
    float3 colors[3] = {
        float3(1.0f, 0.0f, 0.0f),
        float3(0.0f, 1.0f, 0.0f),
        float3(0.0f, 0.0f, 1.0f)
    };
    VSOut o;
    o.position = float4(positions[vid], 0.0f, 1.0f);
    o.color    = float4(colors[vid], 1.0f);
    return o;
}
)hlsl";

static const char *kFragmentShaderHLSL = R"hlsl(
struct VSOut {
    float4 position : SV_Position;
    float4 color    : COLOR0;
};

float4 main(VSOut input) : SV_Target
{
    return input.color;
}
)hlsl";

/* Selected by --mode. Default is SPIRV because it does NOT need DXC linked
 * into shadercross — only the SPIR-V → MSL/DXIL transpile path (which uses
 * SPIRV-Cross) runs at startup. The RUNTIME mode also calls into DXC for the
 * HLSL → SPIR-V step, so it only works on a shadercross built with DXC. */
enum class ShaderMode {
    SPIRV,      // load pre-compiled .spv files from shaders/build/ (default)
    RUNTIME     // embedded HLSL → CompileSPIRVFromHLSL at startup (needs DXC)
};

/* Everything the app needs between frames. Allocated in SDL_AppInit, freed in
 * SDL_AppQuit. Identical shape to examples/cpp-gpu/triangle.cpp on purpose. */
struct AppState {
    SDL_Window              *window   = nullptr;
    SDL_GPUDevice           *device   = nullptr;
    SDL_GPUGraphicsPipeline *pipeline = nullptr;
    long       max_frames = -1;                   // -1 = run until quit
    long       frame      = 0;
    ShaderMode mode       = ShaderMode::SPIRV;    // works without DXC
};

/* Small helper: log an SDL error with context, return APP_FAILURE. */
static SDL_AppResult fail(const char *what)
{
    SDL_Log("%s: %s", what, SDL_GetError());
    return SDL_APP_FAILURE;
}

/* Pretty-print the bitmask the device handed back. Lets you SEE which native
 * format the SDL_GPU backend ended up wanting on this machine — the format
 * SDL_ShaderCross_CompileGraphicsShaderFromSPIRV will silently transpile into. */
static void log_device_shader_formats(SDL_GPUShaderFormat f)
{
    SDL_Log("device-accepted shader formats:");
    if (f & SDL_GPU_SHADERFORMAT_SPIRV)    SDL_Log("  - SPIRV    (Vulkan)");
    if (f & SDL_GPU_SHADERFORMAT_MSL)      SDL_Log("  - MSL      (Metal source)");
    if (f & SDL_GPU_SHADERFORMAT_METALLIB) SDL_Log("  - METALLIB (compiled Metal)");
    if (f & SDL_GPU_SHADERFORMAT_DXIL)     SDL_Log("  - DXIL     (D3D12)");
    if (f & SDL_GPU_SHADERFORMAT_DXBC)     SDL_Log("  - DXBC     (D3D11)");
    if (f == 0)                            SDL_Log("  (none — device creation should have failed)");
}

/* ──────────────────────────────────────────────────────────────────────────
 * Shader acquisition — the central point of this whole example.
 *
 * Both modes converge on `SDL_ShaderCross_CompileGraphicsShaderFromSPIRV`,
 * which is the function that returns a backend-native SDL_GPUShader regardless
 * of whether the GPU wants SPIR-V / MSL / DXIL. The only thing that differs
 * is *where the SPIR-V bytes come from*.
 *
 *   RUNTIME mode: HLSL string → SDL_ShaderCross_CompileSPIRVFromHLSL (DXC under
 *                 the hood) → bytes owned by us, freed with SDL_free.
 *
 *   SPIRV mode:   .spv file on disk → SDL_LoadFile → bytes owned by us, freed
 *                 with SDL_free.
 *
 * Either path then hands those bytes to CompileGraphicsShaderFromSPIRV, which
 * may produce the shader directly (Vulkan), or transpile internally to MSL /
 * DXIL (Metal / D3D12) before creating the shader. We never see that step.
 * ────────────────────────────────────────────────────────────────────────── */

/* Build the on-disk path for a precompiled SPIR-V file. Relative to CWD so
 * `./build/demo_gpu_shadercross --mode spirv` from the repo root just works
 * after `make shaders`. */
static std::string spirv_path(const char *basename)
{
    return std::string("examples/cpp-gpu-shadercross/shaders/build/") + basename;
}

/* Produce a SPIR-V byte buffer from one HLSL string (runtime path). The
 * returned pointer is SDL_malloc'd by shadercross; the caller frees with
 * SDL_free once SDL_GPUShader creation has consumed it. */
static void *compile_hlsl_to_spirv(const char            *hlsl_source,
                                   SDL_ShaderCross_ShaderStage stage,
                                   size_t                *out_size)
{
    SDL_ShaderCross_HLSL_Info hi{};
    hi.source       = hlsl_source;
    hi.entrypoint   = "main";          // both HLSL files use main()
    hi.include_dir  = nullptr;         // no #include resolution needed
    hi.defines      = nullptr;         // no preprocessor defines
    hi.shader_stage = stage;
    hi.props        = 0;
    return SDL_ShaderCross_CompileSPIRVFromHLSL(&hi, out_size);
}

/* Read a precompiled .spv file from disk (offline path). SDL_LoadFile returns
 * SDL_malloc'd memory; same SDL_free contract as above. */
static void *load_spirv_file(const char *path, size_t *out_size)
{
    /* SDL_LoadFile is the right API even for binary blobs: it allocates,
     * reads the full size, and reports it. NULL on failure (use SDL_GetError). */
    return SDL_LoadFile(path, out_size);
}

/* Create one SDL_GPUShader from a SPIR-V byte buffer.
 *
 * The 'resource_info' is all zero because these shaders take NO resources
 * (no samplers, no uniform buffers, no storage). If you add a uniform buffer
 * later, bump num_uniform_buffers — or call SDL_ShaderCross_ReflectGraphicsSPIRV
 * to discover the counts from the SPIR-V itself. */
static SDL_GPUShader *spirv_to_shader(SDL_GPUDevice               *device,
                                      const void                  *spirv_bytes,
                                      size_t                       spirv_size,
                                      SDL_ShaderCross_ShaderStage  stage)
{
    SDL_ShaderCross_SPIRV_Info si{};
    si.bytecode      = static_cast<const Uint8 *>(spirv_bytes);
    si.bytecode_size = spirv_size;
    si.entrypoint    = "main";
    si.shader_stage  = stage;
    si.props         = 0;

    SDL_ShaderCross_GraphicsShaderResourceInfo ri{};   // all zero — no bindings
    return SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(device, &si, &ri, /*props=*/0);
}

/* One stage end-to-end: source (HLSL string OR .spv path) → SDL_GPUShader.
 * Mode-agnostic — branches inside, since the only difference is the SPIR-V
 * source. Frees its intermediate buffer before returning. */
static SDL_GPUShader *load_stage(SDL_GPUDevice               *device,
                                 ShaderMode                   mode,
                                 const char                  *hlsl_source,
                                 const char                  *spirv_basename,
                                 SDL_ShaderCross_ShaderStage  stage)
{
    size_t  size  = 0;
    void   *bytes = nullptr;

    if (mode == ShaderMode::RUNTIME) {
        bytes = compile_hlsl_to_spirv(hlsl_source, stage, &size);
        if (!bytes) {
            const char *err = SDL_GetError();
            SDL_Log("CompileSPIRVFromHLSL: %s", err);
            /* Most common failure mode on macOS: shadercross was built with
             * `-DSDLSHADERCROSS_DXC=OFF` and so it cannot do HLSL → SPIR-V.
             * Surface a concrete next step instead of leaving the user
             * staring at a cryptic upstream message. */
            if (err && SDL_strstr(err, "DXC")) {
                SDL_Log("HINT: --mode runtime requires shadercross built with DXC.");
                SDL_Log("      Use the default --mode spirv with `make shaders`,");
                SDL_Log("      or rebuild shadercross with DXC — see %s/README.md.",
                        "examples/cpp-gpu-shadercross");
            }
            return nullptr;
        }
    } else {
        std::string path = spirv_path(spirv_basename);
        bytes = load_spirv_file(path.c_str(), &size);
        if (!bytes) {
            SDL_Log("SDL_LoadFile(%s): %s — did you run `make shaders`?",
                    path.c_str(), SDL_GetError());
            return nullptr;
        }
    }

    SDL_GPUShader *sh = spirv_to_shader(device, bytes, size, stage);
    SDL_free(bytes);                    // shader has consumed the bytes now
    if (!sh) {
        SDL_Log("CompileGraphicsShaderFromSPIRV: %s", SDL_GetError());
    }
    return sh;
}

/* ──────────────────────────────────────────────────────────────────────────
 * Lifecycle — Init / Event / Iterate / Quit. The frame loop (Iterate) is
 * identical to examples/cpp-gpu/triangle.cpp; only Init differs.
 * ────────────────────────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_SetAppMetadata("SDL3 2D - GPU + shadercross (C++)", "1.0",
                       "com.example.sdl3_2d");

    if (!SDL_Init(SDL_INIT_VIDEO)) {            // SDL3: true == success
        return fail("SDL_Init");
    }

    AppState *st = new AppState();
    *appstate = st;                             // store early — Quit can clean up

    /* CLI parsing. --mode picks the shader source; --frames N exits after N
     * frames so `make smoke` (and any future CI) can run this headless. */
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            st->max_frames = std::atol(argv[++i]);
        } else if (std::strcmp(argv[i], "--mode") == 0 && i + 1 < argc) {
            const char *m = argv[++i];
            if      (std::strcmp(m, "runtime") == 0) st->mode = ShaderMode::RUNTIME;
            else if (std::strcmp(m, "spirv")   == 0) st->mode = ShaderMode::SPIRV;
            else {
                SDL_Log("--mode must be 'runtime' or 'spirv', got '%s'", m);
                return SDL_APP_FAILURE;
            }
        }
    }
    SDL_Log("shader mode: %s",
            st->mode == ShaderMode::SPIRV   ? "spirv   (loaded from .spv on disk; default)"
                                            : "runtime (HLSL → SPIR-V at startup; requires DXC)");

    /* 1. Initialize SDL_shadercross. Loads SPIRV-Cross / DXC dynamic libraries
     *    and prepares the cross-compiler. Paired with SDL_ShaderCross_Quit in
     *    SDL_AppQuit. Must be called BEFORE any other SDL_ShaderCross_* call,
     *    but the order vs. SDL_CreateGPUDevice does not matter — they are
     *    independent subsystems. */
    if (!SDL_ShaderCross_Init()) {
        return fail("SDL_ShaderCross_Init");
    }

    /* 2. Create the GPU device. We advertise EVERY format we could conceivably
     *    supply, so this binary stays valid when copied to a Linux or Windows
     *    machine — SDL picks whichever the actual backend wants:
     *
     *      macOS    → MSL  (or METALLIB)
     *      Linux    → SPIRV
     *      Windows  → DXIL (or DXBC)
     *
     *    shadercross will transpile our SPIR-V to whichever was chosen, so
     *    the rest of the code does not need to branch on it. */
    SDL_GPUShaderFormat advertise =
        SDL_GPU_SHADERFORMAT_SPIRV    |
        SDL_GPU_SHADERFORMAT_MSL      |
        SDL_GPU_SHADERFORMAT_METALLIB |
        SDL_GPU_SHADERFORMAT_DXIL     |
        SDL_GPU_SHADERFORMAT_DXBC;
    st->device = SDL_CreateGPUDevice(advertise, /*debug_mode=*/true, /*name=*/nullptr);
    if (!st->device) {
        return fail("SDL_CreateGPUDevice");
    }

    /* Diagnostic: what did SDL actually pick? Educational on any platform. */
    log_device_shader_formats(SDL_GetGPUShaderFormats(st->device));

    /* 3. Window + swapchain (same as the MSL example). */
    st->window = SDL_CreateWindow("SDL3 2D - GPU + shadercross (C++)",
                                  800, 600, SDL_WINDOW_RESIZABLE);
    if (!st->window) {
        return fail("SDL_CreateWindow");
    }
    if (!SDL_ClaimWindowForGPUDevice(st->device, st->window)) {
        return fail("SDL_ClaimWindowForGPUDevice");
    }

    /* 4. The two shader stages, end-to-end. load_stage() encapsulates both
     *    modes; this code stays mode-agnostic. */
    SDL_GPUShader *vs = load_stage(st->device, st->mode,
                                   kVertexShaderHLSL, "triangle.vert.spv",
                                   SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
    if (!vs) {
        return SDL_APP_FAILURE;
    }
    SDL_GPUShader *fs = load_stage(st->device, st->mode,
                                   kFragmentShaderHLSL, "triangle.frag.spv",
                                   SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);
    if (!fs) {
        SDL_ReleaseGPUShader(st->device, vs);
        return SDL_APP_FAILURE;
    }

    /* 5. Build the graphics pipeline. Identical to examples/cpp-gpu/triangle.cpp
     *    — see that file for the per-field commentary. The pipeline does not
     *    care that the shaders came in through shadercross; once compiled they
     *    are ordinary SDL_GPUShader handles. */
    SDL_GPUColorTargetDescription color_target{};
    color_target.format = SDL_GetGPUSwapchainTextureFormat(st->device, st->window);

    SDL_GPUGraphicsPipelineCreateInfo pci{};
    pci.vertex_shader                         = vs;
    pci.fragment_shader                       = fs;
    pci.primitive_type                        = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pci.target_info.color_target_descriptions = &color_target;
    pci.target_info.num_color_targets         = 1;

    st->pipeline = SDL_CreateGPUGraphicsPipeline(st->device, &pci);

    /* The pipeline now owns its compiled stages; release the standalone
     * shader handles unconditionally. */
    SDL_ReleaseGPUShader(st->device, vs);
    SDL_ReleaseGPUShader(st->device, fs);

    if (!st->pipeline) {
        return fail("SDL_CreateGPUGraphicsPipeline");
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *e)
{
    (void)appstate;

    switch (e->type) {
    case SDL_EVENT_QUIT:
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
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *st = static_cast<AppState *>(appstate);

    /* Per-frame command sequence. Same shape as examples/cpp-gpu/triangle.cpp;
     * see that file for the full narration. The NULL-swapchain branch is what
     * keeps headless `make smoke` valid (window minimized / no display). */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(st->device);
    if (!cmd) {
        return fail("SDL_AcquireGPUCommandBuffer");
    }

    SDL_GPUTexture *swap = nullptr;
    Uint32 sw = 0, sh = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, st->window, &swap, &sw, &sh)) {
        return fail("SDL_WaitAndAcquireGPUSwapchainTexture");
    }

    if (swap) {
        SDL_GPUColorTargetInfo cti{};
        cti.texture     = swap;
        cti.clear_color = SDL_FColor{ 0.07f, 0.07f, 0.10f, 1.0f };
        cti.load_op     = SDL_GPU_LOADOP_CLEAR;
        cti.store_op    = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &cti, 1, nullptr);
        SDL_BindGPUGraphicsPipeline(pass, st->pipeline);
        SDL_DrawGPUPrimitives(pass, /*num_vertices=*/3, /*num_instances=*/1,
                              /*first_vertex=*/0, /*first_instance=*/0);
        SDL_EndGPURenderPass(pass);
    }

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        return fail("SDL_SubmitGPUCommandBuffer");
    }

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
        SDL_ShaderCross_Quit();             // matches an unbalanced Init
        return;
    }

    /* Release in reverse order of creation. We do NOT call SDL_Quit(): the
     * SDL_MAIN_USE_CALLBACKS runtime calls it for us after this returns. */
    if (st->device) {
        SDL_ReleaseGPUGraphicsPipeline(st->device, st->pipeline);
        if (st->window) {
            SDL_ReleaseWindowFromGPUDevice(st->device, st->window);
        }
        SDL_DestroyGPUDevice(st->device);
    }
    SDL_DestroyWindow(st->window);

    /* shadercross teardown — releases SPIRV-Cross / DXC. Safe to call after
     * the device is gone since the cross-compiler does not own GPU resources. */
    SDL_ShaderCross_Quit();

    delete st;
}
