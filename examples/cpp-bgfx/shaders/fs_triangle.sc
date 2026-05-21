// fs_triangle.sc — bgfx fragment shader for the hello triangle.
//
// Runs once per pixel the triangle covers. v_color arrives already
// interpolated by the rasterizer (so a pixel halfway between a red corner
// and a green corner receives a yellowish colour for free) — we just emit
// it as the final fragment colour.
//
// $input mirrors the vertex shader's $output. shaderc cross-references the
// two and the central varying.def.sc to ensure the interface lines up.
$input v_color

#include <bgfx_shader.sh>

void main()
{
    // gl_FragColor is the bgfx-portable name for the final fragment-shader
    // output. On Metal it becomes [[color(0)]], on GLSL it becomes
    // gl_FragColor, on D3D11 it becomes SV_Target — all from one source.
    gl_FragColor = v_color;
}
