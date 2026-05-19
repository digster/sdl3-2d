/*
 * gfx.hpp — tiny 2D shape-drawing helpers on top of the SDL3 renderer (C++).
 *
 * This header is the public API *and* the API reference: every function is
 * documented inline. It belongs to the **C++** example only. The C example
 * under examples/c/ ships its own copy (gfx.h); the two are intentionally
 * independent so either folder can be copied out and built on its own, with
 * no shared source and no cross-language linking.
 *
 * Because nothing links these symbols from C, there is no `extern "C"`: the
 * API lives in a `gfx` namespace, which is C++'s native way to scope names
 * (the equivalent of the C copy's `gfx_` prefix). Usage:
 *
 *     gfx::set_color(ren, 255, 80, 0, 255);   // pick a colour
 *     gfx::fill_circle(ren, 400, 300, 50);    // draw with it
 *
 * Design rule shared by every draw function: shapes are drawn using the
 * renderer's *current* draw colour, so the pattern is always "set, then draw".
 *
 * All coordinates are `float`. This is deliberate and SDL3-native: unlike SDL2
 * (which used integer SDL_Rect / SDL_RenderDrawLine), SDL3's 2D render API is
 * float-based (SDL_FRect, SDL_RenderLine, SDL_RenderPoint). Passing floats
 * straight through avoids a lossy int<->float round-trip.
 */
#ifndef GFX_HPP
#define GFX_HPP

#include <SDL3/SDL.h>

namespace gfx {

/*
 * Clear the entire render target to an opaque RGB colour.
 *
 * Convenience wrapper around SDL_SetRenderDrawColor + SDL_RenderClear. Call
 * this once at the top of every frame before drawing anything else. Note it
 * leaves the draw colour set to (r,g,b,255) as a side effect.
 */
void clear(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b);

/*
 * Set the colour used by every subsequent gfx:: draw call.
 *
 * `a` is alpha (0 = fully transparent, 255 = fully opaque). For alpha to
 * actually blend you need the renderer in blend mode; SDL3 renderers default
 * to SDL_BLENDMODE_BLEND so translucent draws work out of the box.
 */
void set_color(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a);

/* Draw a single pixel at (x, y) in the current colour. */
void point(SDL_Renderer *renderer, float x, float y);

/* Draw a 1px line segment from (x1, y1) to (x2, y2) in the current colour. */
void line(SDL_Renderer *renderer,
          float x1, float y1, float x2, float y2);

/*
 * Draw the OUTLINE of an axis-aligned rectangle.
 * (x, y) is the top-left corner; w/h are width/height in pixels.
 */
void rect(SDL_Renderer *renderer, float x, float y, float w, float h);

/* Same rectangle, but filled solid. Backed by SDL_RenderFillRect. */
void fill_rect(SDL_Renderer *renderer, float x, float y, float w, float h);

/*
 * Draw the OUTLINE of a circle centred at (cx, cy) with the given radius,
 * using the integer midpoint-circle algorithm. A radius <= 0 draws nothing.
 */
void circle(SDL_Renderer *renderer, float cx, float cy, float radius);

/*
 * Draw a FILLED disc centred at (cx, cy). Implemented as a stack of
 * horizontal scanlines (one SDL_RenderLine per row), which keeps it
 * dependency-free and makes it honour the current draw colour/alpha exactly.
 * A radius <= 0 draws nothing.
 */
void fill_circle(SDL_Renderer *renderer, float cx, float cy, float radius);

/* Draw the OUTLINE of a triangle (three connected line segments). */
void triangle(SDL_Renderer *renderer,
              float x1, float y1,
              float x2, float y2,
              float x3, float y3);

/*
 * Draw a FILLED triangle via SDL_RenderGeometry. Note: SDL3's SDL_Vertex
 * carries an SDL_FColor with components in the 0..1 range (not 0..255), so the
 * implementation reads back the current draw colour and converts it — that is
 * what keeps this consistent with the "set colour, then draw" model used by
 * every other helper here. Degenerate (zero-area) triangles are tolerated by
 * SDL and simply produce nothing visible.
 */
void fill_triangle(SDL_Renderer *renderer,
                    float x1, float y1,
                    float x2, float y2,
                    float x3, float y3);

}  // namespace gfx

#endif  // GFX_HPP
