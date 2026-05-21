# bgfx shader sources

Three files drive `make bgfx-shaders`:

| File | Role |
|---|---|
| `varying.def.sc` | Central registry mapping shader names → GPU pipeline semantics. **Read the gotcha below before editing.** |
| `vs_triangle.sc` | Vertex shader — outputs `gl_Position`, forwards `a_color0` to the rasterizer. |
| `fs_triangle.sc` | Fragment shader — emits the interpolated colour as `gl_FragColor`. |

`make bgfx-shaders` invokes `shaderc` (bgfx's compiler) on these and writes
per-renderer bytecode under `build/<renderer>/` (Metal on macOS). The
sibling [`triangle_bgfx.cpp`](../triangle_bgfx.cpp) loads those `.bin` files at runtime
via `bgfx::createShader(bgfx::copy(bytes, size))`.

## Gotcha — keep `varying.def.sc` comment-free

bgfx's `varying.def.sc` parser **silently drops declarations that follow
`//` comments**. There is no error message; affected attributes just stop
appearing in the generated shader, and your draws come out blank or
shaderc errors complaining about "unknown variable a_position" deep inside
the generated HLSL intermediate.

Keep that file mechanical — declarations only, blank lines fine. Put all
narrative comments in the `.sc` files (which use a real C preprocessor and
handle `//` properly) or this README.

## Re-targeting other backends

`shaderc -p <platform>` flags for cross-compilation:

```sh
shaderc=$BGFX_PREFIX/bin/shaderc
inc=$BGFX_PREFIX/share/bgfx/shaders

# Metal (default for macOS)
$shaderc -f vs_triangle.sc --varyingdef varying.def.sc -i $inc \
    --platform osx -p metal --type vertex -o build/metal/vs_triangle.bin

# SPIR-V (Vulkan, also bgfx's internal intermediate)
$shaderc -f vs_triangle.sc --varyingdef varying.def.sc -i $inc \
    --platform linux -p spirv --type vertex -o build/spirv/vs_triangle.bin

# DXBC (D3D11)
$shaderc -f vs_triangle.sc --varyingdef varying.def.sc -i $inc \
    --platform windows -p s_5_0 --type vertex -o build/d3d11/vs_triangle.bin
```

The host C++ would then pick `build/<renderer>/` at runtime based on
`bgfx::getRendererType()` and load the matching .bin. The single-platform
default keeps this example focused on the SDL3↔bgfx wiring rather than the
build matrix.
