// triangle.frag.hlsl — fragment stage. Runs once per pixel the triangle covers.
//
// `color` arrives already interpolated across the three vertices by the
// rasterizer (so a pixel halfway between the red and green corners is a smooth
// yellow), and we just emit it as the final pixel colour.
//
// SV_Target is the standard HLSL semantic for "the colour render target."
// shadercross will translate it to Metal's [[color(0)]] or SPIR-V's location 0
// automatically.

struct VSOut {
    float4 position : SV_Position;
    float4 color    : COLOR0;
};

float4 main(VSOut input) : SV_Target
{
    return input.color;
}
