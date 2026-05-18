/*
 * traditional.c — SDL3 starter using a hand-written game loop, in C.
 *
 * This is the "classic" structure: you own main(), you own the while-loop, and
 * the lifecycle is fully visible top to bottom:
 *
 *     init -> create window+renderer -> enable vsync
 *       loop: pump events -> update (with dt) -> clear -> draw -> present
 *     teardown
 *
 * The companion example examples/callbacks.cpp does the exact same thing using
 * SDL3's newer SDL_AppInit/Iterate/Event/Quit model in C++ — compare the two.
 *
 * Run `./demo_traditional` for an interactive window, or
 * `./demo_traditional --frames 120` to render 120 frames and exit (used by
 * `make smoke` for non-interactive verification).
 */
#include <SDL3/SDL.h>
#include "gfx.h"

#include <stdbool.h>
#include <string.h>   /* strcmp */
#include <stdlib.h>   /* atol   */

int main(int argc, char **argv)
{
    /* --- optional `--frames N`: auto-quit after N frames (smoke runs) --- */
    long max_frames = -1;            /* -1 means "run until the user quits" */
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            max_frames = atol(argv[++i]);
        }
    }

    /* --- 1. Initialise SDL. NOTE: SDL3 returns true on success. --- */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    /* --- 2. One call gives us both the window and a 2D renderer. --- */
    SDL_Window   *window   = NULL;
    SDL_Renderer *renderer = NULL;
    if (!SDL_CreateWindowAndRenderer("SDL3 2D - traditional loop (C)",
                                     800, 600, SDL_WINDOW_RESIZABLE,
                                     &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    /* --- 3. Vsync caps us to the display refresh so the loop doesn't spin
     *        the CPU at thousands of fps. Pass 1 = sync every refresh. --- */
    SDL_SetRenderVSync(renderer, 1);

    /* Animation state for the dt-driven bouncing ball. */
    float ball_x = 140.0f, ball_y = 420.0f;
    float ball_vx = 240.0f, ball_vy = 190.0f;   /* pixels per second */
    const float ball_r = 26.0f;

    Uint64 prev_ns = SDL_GetTicksNS();           /* high-res timer (ns) */
    long   frame   = 0;
    bool   running = true;

    while (running) {
        /* --- 4. Event pump: drain EVERY pending event each frame. --- */
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_EVENT_QUIT:                  /* window close button */
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                SDL_Log("key down: scancode=%d", (int)e.key.scancode);
                if (e.key.scancode == SDL_SCANCODE_ESCAPE) {
                    running = false;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:     /* x/y are floats in SDL3 */
                SDL_Log("mouse button %d at (%.0f, %.0f)",
                        (int)e.button.button, e.button.x, e.button.y);
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                SDL_Log("window resized to %dx%d",
                        (int)e.window.data1, (int)e.window.data2);
                break;
            default:
                break;
            }
        }

        /* --- 5. Update. dt = seconds since last frame. --- */
        Uint64 now_ns = SDL_GetTicksNS();
        float  dt     = (float)(now_ns - prev_ns) / 1.0e9f;
        prev_ns = now_ns;

        /* Query the live window size so the ball still bounces correctly
         * after a resize, instead of hardcoding 800x600. */
        int win_w = 0, win_h = 0;
        SDL_GetWindowSize(window, &win_w, &win_h);

        /* >>> your game logic goes here <<< */
        ball_x += ball_vx * dt;
        ball_y += ball_vy * dt;
        if (ball_x - ball_r < 0)            { ball_x = ball_r;                 ball_vx = -ball_vx; }
        if (ball_x + ball_r > (float)win_w) { ball_x = (float)win_w - ball_r;  ball_vx = -ball_vx; }
        if (ball_y - ball_r < 0)            { ball_y = ball_r;                 ball_vy = -ball_vy; }
        if (ball_y + ball_r > (float)win_h) { ball_y = (float)win_h - ball_r;  ball_vy = -ball_vy; }

        /* --- 6. Render: always clear -> draw -> present. --- */
        gfx_clear(renderer, 18, 18, 24);          /* dark background */

        /* A grid of points. */
        gfx_set_color(renderer, 90, 90, 115, 255);
        for (float gx = 40.0f; gx < 240.0f; gx += 16.0f)
            for (float gy = 40.0f; gy < 120.0f; gy += 16.0f)
                gfx_point(renderer, gx, gy);

        gfx_set_color(renderer, 80, 200, 255, 255);
        gfx_line(renderer, 280, 50, 460, 110);

        gfx_set_color(renderer, 255, 200, 60, 255);
        gfx_rect(renderer, 500, 40, 120, 80);            /* outline */

        gfx_set_color(renderer, 60, 200, 120, 255);
        gfx_fill_rect(renderer, 650, 40, 110, 80);       /* filled  */

        gfx_set_color(renderer, 255, 120, 200, 255);
        gfx_circle(renderer, 130, 270, 55);              /* outline */

        gfx_set_color(renderer, 120, 160, 255, 255);
        gfx_fill_circle(renderer, 320, 270, 55);         /* filled  */

        gfx_set_color(renderer, 255, 240, 120, 255);
        gfx_triangle(renderer, 470, 330, 550, 200, 630, 330);   /* outline */

        gfx_set_color(renderer, 255, 110, 90, 255);
        gfx_fill_triangle(renderer, 660, 330, 720, 200, 780, 330); /* filled */

        /* The dt-animated shape proves the loop is alive. */
        gfx_set_color(renderer, 255, 255, 255, 255);
        gfx_fill_circle(renderer, ball_x, ball_y, ball_r);

        SDL_RenderPresent(renderer);              /* swap the back buffer */

        /* --- 7. Optional auto-quit for non-interactive smoke runs. --- */
        if (max_frames >= 0 && ++frame >= max_frames) {
            running = false;
        }
    }

    /* --- 8. Tear down in reverse order of creation. --- */
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
