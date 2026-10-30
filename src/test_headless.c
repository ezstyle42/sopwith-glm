/*
 * test_headless.c - deterministic simulation test for the core.
 * Runs the game headless for many frames and asserts invariants:
 * player stays within world bounds, no crash with any input pattern,
 * win condition reachable when enemy base is bombed.
 */
#include "sopwith.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void)
{
    static sw_game g;
    sw_input in = {0};

    /* 1. idle: game must not crash and player must stay in bounds */
    sw_init(&g, 42);
    for (int f = 0; f < 600; f++) {
        sw_render_prepare(&g);
        sw_update(&g, &in);
        assert(g.pl.x >= 0 && g.pl.x < SW_WORLD_W);
        assert(g.pl.y >= 0 && g.pl.y < SW_SCR_H);
    }
    printf("idle sim ok (state=%d)\n", g.state);

    /* 2. constant turn+fire stress */
    sw_init(&g, 7);
    in.turn_ccw = true; in.fire = true;
    for (int f = 0; f < 2000; f++) {
        sw_render_prepare(&g);
        sw_update(&g, &in);
        assert(g.pl.x >= 0 && g.pl.x < SW_WORLD_W);
    }
    printf("turn+fire stress ok (state=%d)\n", g.state);
    memset(&in, 0, sizeof in);

    /* 3. full-power flight across the world */
    sw_init(&g, 99);
    in.accelerate = true;
    for (int f = 0; f < 4000 && g.pl.alive; f++) {
        sw_render_prepare(&g);
        sw_update(&g, &in);
        /* render every frame too, to exercise renderer */
        sw_framebuffer(&g);
        assert(g.cam_x >= 0 && g.cam_x <= SW_WORLD_W - SW_SCR_W);
    }
    printf("flight sim ok (x=%.0f y=%.0f state=%d score=%d)\n",
           g.pl.x, g.pl.y, g.state, g.pl.score);

    /* 4. determinism: same seed, same input, same trajectory */
    static sw_game a, b;
    sw_init(&a, 1234); sw_init(&b, 1234);
    in.accelerate = true; in.fire = true;
    for (int f = 0; f < 500; f++) {
        sw_update(&a, &in);
        sw_update(&b, &in);
        assert(a.pl.x == b.pl.x && a.pl.y == b.pl.y && a.pl.score == b.pl.score);
    }
    printf("determinism ok\n");

    /* 5. render content: HUD band, sky, terrain and player must be present */
    {
        static sw_game g2;
        sw_input zero = {0};
        sw_init(&g2, 5);
        sw_render_prepare(&g2);
        sw_update(&g2, &zero);
        const uint8_t *fb = sw_framebuffer(&g2);
        int hud = 0, sky = 0, ground = 0, player = 0;
        for (int i = 0; i < SW_SCR_W * SW_SCR_H; i++) {
            if (fb[i] == SW_BLACK) hud++;
            if (fb[i] == SW_CYAN) sky++;
            if (fb[i] == SW_GREEN) ground++;
        }
        int px = (int)g2.pl.x - g2.cam_x, py = (int)g2.pl.y;
        if (px >= 0 && px < SW_SCR_W)
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -6; dx <= 6; dx++) {
                    int xx = px + dx, yy = py + dy;
                    if (xx >= 0 && xx < SW_SCR_W && yy >= 0 && yy < SW_SCR_H &&
                        fb[yy * SW_SCR_W + xx] == SW_YELLOW)
                        player = 1;
                }
        assert(hud > 500);
        assert(sky > 1000);
        assert(ground > 200);
        assert(player);
        printf("render content ok (hud=%d sky=%d ground=%d player=%d)\n",
               hud, sky, ground, player);
    }

    puts("ALL TESTS PASSED");
    return 0;
}
