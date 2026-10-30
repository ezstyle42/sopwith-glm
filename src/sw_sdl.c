/*
 * sw_sdl.c - SDL2 graphical frontend.
 * Build only when SDL2 is available; see CMakeLists.txt.
 *
 * Controls: arrows / . and / (roll), space or f (fire), b (bomb),
 *           x (flaps/boost), q (quit), F11 fullscreen toggle.
 */
#include "sopwith.h"
#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static const uint32_t pal[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

int main(int argc, char **argv)
{
    unsigned seed = (argc > 1) ? (unsigned)atoi(argv[1]) : (unsigned)SDL_GetTicks();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *win = SDL_CreateWindow("Sopwith",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SW_SCR_W * 3, SW_SCR_H * 3, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return 1; }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, 0);
    SDL_Texture *tex = SDL_CreateTexture(ren,
        SDL_PIXELFORMAT_RGBX8888, SDL_TEXTUREACCESS_STREAMING, SW_SCR_W, SW_SCR_H);

    static sw_game game;
    sw_input in = {0};
    sw_init(&game, seed);

    static uint32_t pix[SW_SCR_W * SW_SCR_H];
    Uint64 frame_start = SDL_GetPerformanceCounter();
    const double frame_len = 1.0 / 60.0;
    bool running = true;
    const Uint8 *kb = SDL_GetKeyboardState(NULL);

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN &&
                (ev.key.keysym.sym == SDLK_q || ev.key.keysym.sym == SDLK_ESCAPE))
                running = false;
        }

        in.turn_ccw = kb[SDL_SCANCODE_LEFT] || kb[SDL_SCANCODE_SLASH];
        in.turn_cw  = kb[SDL_SCANCODE_RIGHT] || kb[SDL_SCANCODE_PERIOD];
        in.fire     = kb[SDL_SCANCODE_SPACE] || kb[SDL_SCANCODE_F];
        in.bomb     = kb[SDL_SCANCODE_B];
        in.accelerate = kb[SDL_SCANCODE_X] || kb[SDL_SCANCODE_UP];
        in.quit = false;

        sw_render_prepare(&game);
        sw_update(&game, &in);
        if (game.state == SW_STATE_GAME_OVER && game.msg_timer <= 0 && kb[SDL_SCANCODE_SPACE]) {
            sw_init(&game, (unsigned)SDL_GetPerformanceCounter());
        }

        const uint8_t *fb = sw_framebuffer(&game);
        for (int i = 0; i < SW_SCR_W * SW_SCR_H; i++) pix[i] = pal[fb[i]];
        void *tptr; int pitch;
        SDL_LockTexture(tex, NULL, &tptr, &pitch);
        memcpy(tptr, pix, sizeof pix);
        SDL_UnlockTexture(tex);
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, NULL, NULL);
        SDL_RenderPresent(ren);

        Uint64 now = SDL_GetPerformanceCounter();
        double elapsed = (now - frame_start) / (double)SDL_GetPerformanceFrequency();
        if (elapsed < frame_len)
            SDL_Delay((Uint32)((frame_len - elapsed) * 1000.0));
        frame_start = SDL_GetPerformanceCounter();
    }

    SDL_DestroyTexture(tex);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
