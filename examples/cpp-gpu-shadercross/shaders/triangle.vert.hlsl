// triangle.vert.hlsl — vertex stage authored ONCE in HLSL, then cross-compiled
// to whatever the target GPU backend needs (SPIR-V for Vulkan, MSL for Metal,
// DXIL for D3D12). The driving idea of SDL_shadercross: write one source, ship
// every platform.
//
// We do not bind any vertex buffer; the three corners of the triangle are baked
// into the shader and selected by the per-vertex semantic SV_VertexID (the HLSL
// equivalent of Metal's [[vertex_id]] / GLSL's gl_VertexIndex). This is the
// leanest pipeline that still exercises every required stage — the focus stays
// on cross-compilation, not on GPU resource upload.
//
// Coordinate space note: clip space is +X right, +Y up, X/Y ∈ [-1,+1]. That
// matches Metal and Vulkan (when Vulkan's negative-Y viewport convention is
// applied, which SDL_GPU does for you). DXC's HLSL→SPIRV path also follows this
// convention out of the box.

struct VSOut {
    float4 position : SV_Position;   // required: clip-space position
    float4 color    : COLOR0;        // interpolated by the rasterizer
};

VSOut main(uint vid : SV_VertexID)
{
    // The three corners — same scene as examples/cpp-gpu/triangle.cpp, so the
    // shadercross binary should render the *identical* image as the MSL one.
    float2 positions[3] = {
        float2( 0.0f,  0.5f),        // vid 0 — top   (red)
        float2(-0.5f, -0.5f),        // vid 1 — left  (green)
        float2( 0.5f, -0.5f)         // vid 2 — right (blue)
    };
    float3 colors[3] = {
        float3(1.0f, 0.0f, 0.0f),
        float3(0.0f, 1.0f, 0.0f),
        float3(0.0f, 0.0f, 1.0f)
    };

    VSOut o;
    o.position = float4(positions[vid], 0.0f, 1.0f);   // z=0, w=1 — no projection
    o.color    = float4(colors[vid], 1.0f);
    return o;
}
