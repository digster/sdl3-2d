/*
 * gfx.c — implementation of the gfx.h drawing helpers.
 *
 * Compiled as C (see Makefile / CMakeLists.txt). The C++ example links against
 * the object produced here; the `extern "C"` guard in gfx.h is what makes the
 * symbol names line up across the language boundary.
 *
 * Every routine assumes the caller has already chosen a colour via
 * gfx_set_color() (or gfx_clear()); nothing here changes the colour except the
 * two functions whose explicit job is to set it.
 */
#include "gfx.h"

#include <math.h>   /* sqrtf — used by the filled-circle scanline math */

void gfx_clear(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b)
{
    /* Opaque clear. This also leaves the draw colour at (r,g,b,255), which is
     * harmless because callers set their own colour before each shape. */
    SDL_SetRenderDrawColor(renderer, r, g, b, 255);
    SDL_RenderClear(renderer);
}

void gfx_set_color(SDL_Renderer *renderer, Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, a);
}

void gfx_point(SDL_Renderer *renderer, float x, float y)
{
    /* SDL3 point/line primitives are float-based (SDL2 used ints). */
    SDL_RenderPoint(renderer, x, y);
}

void gfx_line(SDL_Renderer *renderer,
              float x1, float y1, float x2, float y2)
{
    SDL_RenderLine(renderer, x1, y1, x2, y2);
}

void gfx_rect(SDL_Renderer *renderer, float x, float y, float w, float h)
{
    /* SDL3 uses SDL_FRect (float) here; SDL2's SDL_Rect was integer-only. */
    SDL_FRect rect = { x, y, w, h };
    SDL_RenderRect(renderer, &rect);          /* outline only */
}

void gfx_fill_rect(SDL_Renderer *renderer, float x, float y, float w, float h)
{
    SDL_FRect rect = { x, y, w, h };
    SDL_RenderFillRect(renderer, &rect);      /* solid fill */
}

void gfx_circle(SDL_Renderer *renderer, float cx, float cy, float radius)
{
    if (radius <= 0.0f) {
        return;                               /* nothing to draw */
    }

    /*
     * Integer midpoint-circle (Bresenham) algorithm.
     *
     * We only ever compute one octant. The circle has 8-way symmetry, so each
     * computed (x, y) is mirrored into the other seven octants. `d` is the
     * decision variable: its sign tells us whether the next pixel steps
     * straight down (y++) or also inward (x--), using only integer adds — no
     * trig, no sqrt in the loop.
     */
    int x = (int)radius;
    int y = 0;
    int d = 1 - x;

    while (x >= y) {
        /* 8 mirrored points of the current (x, y) about the centre. */
        SDL_RenderPoint(renderer, cx + (float)x, cy + (float)y);
        SDL_RenderPoint(renderer, cx - (float)x, cy + (float)y);
        SDL_RenderPoint(renderer, cx + (float)x, cy - (float)y);
        SDL_RenderPoint(renderer, cx - (float)x, cy - (float)y);
        SDL_RenderPoint(renderer, cx + (float)y, cy + (float)x);
        SDL_RenderPoint(renderer, cx - (float)y, cy + (float)x);
        SDL_RenderPoint(renderer, cx + (float)y, cy - (float)x);
        SDL_RenderPoint(renderer, cx - (float)y, cy - (float)x);

        y++;
        if (d <= 0) {
            d += 2 * y + 1;                   /* step straight: y++ only */
        } else {
            x--;
            d += 2 * (y - x) + 1;             /* step diagonally: y++, x-- */
        }
    }
}

void gfx_fill_circle(SDL_Renderer *renderer, float cx, float cy, float radius)
{
    if (radius <= 0.0f) {
        return;
    }

    /*
     * Scanline fill. For every row offset `dy` from the centre, the circle
     * equation x^2 + y^2 = r^2 gives the horizontal half-width at that row as
     * sqrt(r^2 - dy^2). Drawing one horizontal line per row fills the whole
     * disc, needs no extra SDL features, and honours the current colour/alpha
     * exactly (SDL_RenderGeometry would require 0..1 float colours instead).
     */
    int r = (int)radius;
    for (int dy = -r; dy <= r; ++dy) {
        float half_w = sqrtf((float)(r * r - dy * dy));
        SDL_RenderLine(renderer,
                       cx - half_w, cy + (float)dy,
                       cx + half_w, cy + (float)dy);
    }
}

void gfx_triangle(SDL_Renderer *renderer,
                  float x1, float y1,
                  float x2, float y2,
                  float x3, float y3)
{
    /* Outline = the three edges drawn as line segments. */
    SDL_RenderLine(renderer, x1, y1, x2, y2);
    SDL_RenderLine(renderer, x2, y2, x3, y3);
    SDL_RenderLine(renderer, x3, y3, x1, y1);
}

void gfx_fill_triangle(SDL_Renderer *renderer,
                        float x1, float y1,
                        float x2, float y2,
                        float x3, float y3)
{
    /*
     * SDL_RenderGeometry rasterises arbitrary triangles, but its SDL_Vertex
     * stores colour as an SDL_FColor in the 0..1 range — NOT the 0..255 Uint8
     * range used by SDL_SetRenderDrawColor. To keep this helper consistent
     * with the rest of the API ("set colour, then draw"), we read the current
     * draw colour back and rescale it to 0..1 here.
     */
    Uint8 r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);

    SDL_FColor col;
    col.r = (float)r / 255.0f;
    col.g = (float)g / 255.0f;
    col.b = (float)b / 255.0f;
    col.a = (float)a / 255.0f;

    /* tex_coord is unused (no texture), so it can be left at 0. */
    SDL_Vertex verts[3];
    verts[0].position.x = x1; verts[0].position.y = y1;
    verts[1].position.x = x2; verts[1].position.y = y2;
    verts[2].position.x = x3; verts[2].position.y = y3;
    for (int i = 0; i < 3; ++i) {
        verts[i].color = col;
        verts[i].tex_coord.x = 0.0f;
        verts[i].tex_coord.y = 0.0f;
    }

    /* texture = NULL (solid colour), no index buffer (vertices in order). */
    SDL_RenderGeometry(renderer, NULL, verts, 3, NULL, 0);
}
