// triangle.vert.glsl — the GLSL twin of triangle.vert.hlsl.
//
// Ships in parallel with the HLSL version so the example works for both
// toolchains:
//
//   GLSL + glslangValidator (brew install glslang)      — always available, no DXC
//   HLSL + shadercross CLI  (needs DXC linked into shadercross) — advanced path
//
// `make shaders` (in the repo Makefile) compiles whichever sources match the
// tool you actually have. Whichever path you take, you end up with the same
// `triangle.vert.spv` that the C++ example loads via
// `SDL_ShaderCross_CompileGraphicsShaderFromSPIRV` — that function is the
// piece doing the cross-platform dispatch, not the front-end language.

#version 450

// Per-vertex outputs interpolated by the rasterizer. Slot 0 is matched at the
// fragment-shader inputs by the same `location = 0`.
layout(location = 0) out vec4 v_color;

void main()
{
    // Three baked corners (same scene as triangle.cpp). Selected by
    // gl_VertexIndex — the GLSL equivalent of HLSL's SV_VertexID and
    // Metal's [[vertex_id]]. No vertex buffer required.
    vec2 positions[3] = vec2[](
        vec2( 0.0,  0.5),   // top   (red)
        vec2(-0.5, -0.5),   // left  (green)
        vec2( 0.5, -0.5)    // right (blue)
    );
    vec3 colors[3] = vec3[](
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, 0.0, 1.0)
    );

    // gl_Position is the GLSL clip-space output every vertex shader writes.
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    v_color     = vec4(colors[gl_VertexIndex], 1.0);
}
