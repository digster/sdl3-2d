// vs_triangle.sc — bgfx vertex shader for the hello triangle.
//
// Runs once per vertex submitted by the CPU-side vertex buffer. Writes
// gl_Position (the required clip-space position) and forwards the per-vertex
// colour through to the fragment stage via v_color (declared in varying.def.sc).
//
// $input  — per-vertex inputs we read from the bgfx::VertexLayout. shaderc
//           expands this into platform-appropriate attribute declarations.
// $output — interpolated varyings we hand off to the rasterizer.
$input  a_position, a_color0
$output v_color

// bgfx_shader.sh provides the platform-portable macros and built-ins
// (vec4, gl_Position, mul, …). It lives under the bgfx source tree in
// bgfx/src/ — the Makefile's bgfx-shaders rule points shaderc at it via -i.
#include <bgfx_shader.sh>

void main()
{
    // No transform here: a_position is already in clip space (the CPU-side
    // vertex buffer writes coords in the [-1, +1] cube). A real scene would
    // multiply by the model-view-projection matrix via mul(u_modelViewProj,
    // vec4(a_position, 1.0)).
    gl_Position = vec4(a_position, 1.0);

    // Forward the per-vertex colour. The rasterizer interpolates this across
    // the triangle's interior so the fragment shader sees a smooth gradient
    // for free — same trick as the MSL example in ../cpp-gpu/triangle.cpp.
    v_color = a_color0;
}
