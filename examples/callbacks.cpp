/*
 * callbacks.cpp — SDL3 starter using the callback app model, in C++.
 *
 * Same window/input/drawing as examples/traditional.c, but instead of writing
 * our own main() + while-loop we hand control to SDL: define
 * SDL_MAIN_USE_CALLBACKS and implement four functions. SDL calls them:
 *
 *     SDL_AppInit     once, at startup
 *     SDL_AppEvent    once per incoming event
 *     SDL_AppIterate  once per frame
 *     SDL_AppQuit     once, at shutdown
 *
 * Each returns SDL_APP_CONTINUE to keep running, or SDL_APP_SUCCESS /
 * SDL_APP_FAILURE to stop. This model is what makes the same code portable to
 * web (Emscripten) and mobile later, where you can't own the main loop.
 *
 * Per-app state lives in a heap C++ object whose pointer SDL stores for us and
 * passes back into every callback (the `appstate` parameter) — no globals.
 *
 * Run `./demo_callbacks` for a window, or `./demo_callbacks --frames 120`
 * to render 120 frames then exit (used by `make smoke`).
 */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>     /* provides the real entry point (also on macOS) */
#include "gfx.h"               /* plain-C API; usable here thanks to extern "C" */

#include <vector>
#include <cstring>             /* std::strcmp */
#include <cstdlib>             /* std::atol   */

/* A tiny C++ value type — we keep a std::vector of these to show a C++
 * container driving the C drawing API. */
struct ColoredDot {
    float x, y, r;
    Uint8 cr, cg, cb;
};

/* Everything the app needs between frames. Allocated in SDL_AppInit,
 * destroyed in SDL_AppQuit. */
struct AppState {
    SDL_Window   *window   = nullptr;
    SDL_Renderer *renderer = nullptr;
    Uint64 prev_ns = 0;
    long   max_frames = -1;       /* -1 = run until quit; set by --frames */
    long   frame      = 0;
    float  ball_x = 140.0f, ball_y = 180.0f;
    float  ball_vx = 210.0f, ball_vy = 240.0f;  /* px/sec */
    float  ball_r = 26.0f;
    std::vector<ColoredDot> dots;
};

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    /* Window-manager/about metadata. Optional but good practice. */
    SDL_SetAppMetadata("SDL3 2D - callbacks (C++)", "1.0",
                       "com.example.sdl3_2d");

    if (!SDL_Init(SDL_INIT_VIDEO)) {            /* SDL3: true == success */
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    AppState *st = new AppState();              /* C++ object behind appstate */

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            st->max_frames = std::atol(argv[++i]);
        }
    }

    if (!SDL_CreateWindowAndRenderer("SDL3 2D - callbacks (C++)",
                                     800, 600, SDL_WINDOW_RESIZABLE,
                                     &st->window, &st->renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        delete st;
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderVSync(st->renderer, 1);        /* pace to display refresh */

    /* A little real C++: build a row of dots in a std::vector. They get fed
     * into the C gfx_* API every frame in SDL_AppIterate. */
    for (int i = 0; i < 8; ++i) {
        st->dots.push_back(ColoredDot{
            60.0f + (float)i * 28.0f, 60.0f, 10.0f,
            (Uint8)(40 + i * 26), (Uint8)180, (Uint8)(255 - i * 20)
        });
    }

    st->prev_ns = SDL_GetTicksNS();
    *appstate = st;                             /* SDL hands this back to us */
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *e)
{
    (void)appstate;                             /* unused in this callback */

    switch (e->type) {
    case SDL_EVENT_QUIT:                        /* window close button */
        return SDL_APP_SUCCESS;                 /* stop, success */
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
    return SDL_APP_CONTINUE;                    /* keep running */
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *st = static_cast<AppState *>(appstate);

    /* dt = seconds since the previous iteration. */
    Uint64 now_ns = SDL_GetTicksNS();
    float  dt     = (float)(now_ns - st->prev_ns) / 1.0e9f;
    st->prev_ns = now_ns;

    int win_w = 0, win_h = 0;
    SDL_GetWindowSize(st->window, &win_w, &win_h);

    /* >>> your game logic goes here <<< */
    st->ball_x += st->ball_vx * dt;
    st->ball_y += st->ball_vy * dt;
    if (st->ball_x - st->ball_r < 0)            { st->ball_x = st->ball_r;                 st->ball_vx = -st->ball_vx; }
    if (st->ball_x + st->ball_r > (float)win_w) { st->ball_x = (float)win_w - st->ball_r;  st->ball_vx = -st->ball_vx; }
    if (st->ball_y - st->ball_r < 0)            { st->ball_y = st->ball_r;                 st->ball_vy = -st->ball_vy; }
    if (st->ball_y + st->ball_r > (float)win_h) { st->ball_y = (float)win_h - st->ball_r;  st->ball_vy = -st->ball_vy; }

    /* clear -> draw -> present */
    gfx_clear(st->renderer, 18, 18, 24);

    /* C++ std::vector feeding the C drawing API. */
    for (const ColoredDot &d : st->dots) {
        gfx_set_color(st->renderer, d.cr, d.cg, d.cb, 255);
        gfx_fill_circle(st->renderer, d.x, d.y, d.r);
    }

    gfx_set_color(st->renderer, 80, 200, 255, 255);
    gfx_line(st->renderer, 280, 50, 460, 110);

    gfx_set_color(st->renderer, 255, 200, 60, 255);
    gfx_rect(st->renderer, 500, 40, 120, 80);

    gfx_set_color(st->renderer, 60, 200, 120, 255);
    gfx_fill_rect(st->renderer, 650, 40, 110, 80);

    gfx_set_color(st->renderer, 255, 120, 200, 255);
    gfx_circle(st->renderer, 130, 270, 55);

    gfx_set_color(st->renderer, 120, 160, 255, 255);
    gfx_fill_circle(st->renderer, 320, 270, 55);

    gfx_set_color(st->renderer, 255, 240, 120, 255);
    gfx_triangle(st->renderer, 470, 330, 550, 200, 630, 330);

    gfx_set_color(st->renderer, 255, 110, 90, 255);
    gfx_fill_triangle(st->renderer, 660, 330, 720, 200, 780, 330);

    gfx_set_color(st->renderer, 255, 255, 255, 255);
    gfx_fill_circle(st->renderer, st->ball_x, st->ball_y, st->ball_r);

    SDL_RenderPresent(st->renderer);

    /* Optional auto-quit for non-interactive smoke runs. */
    if (st->max_frames >= 0 && ++st->frame >= st->max_frames) {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    AppState *st = static_cast<AppState *>(appstate);
    if (st) {
        /* We own these — destroy them explicitly. We do NOT call SDL_Quit():
         * the SDL_MAIN_USE_CALLBACKS runtime calls it for us after this
         * function returns. */
        SDL_DestroyRenderer(st->renderer);
        SDL_DestroyWindow(st->window);
        delete st;
    }
}
