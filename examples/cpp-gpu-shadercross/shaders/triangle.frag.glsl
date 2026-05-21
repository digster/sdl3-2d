// triangle.frag.glsl — the GLSL twin of triangle.frag.hlsl.
//
// Same idea as the vertex shader's twin file: ships in parallel with the HLSL
// so you can pick whichever toolchain you have installed.

#version 450

layout(location = 0) in  vec4 v_color;   // interpolated by the rasterizer
layout(location = 0) out vec4 o_color;   // bound to colour target 0

void main()
{
    o_color = v_color;
}
