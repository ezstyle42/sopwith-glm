/*
 * render.c - software renderer into a 320x200 CGA-palette framebuffer.
 * The camera horizontally follows the player, as in the original game.
 */
#include "sopwith.h"
#include <string.h>
#include <stdio.h>

static uint8_t fb[SW_SCR_W * SW_SCR_H];

static inline void px(int x, int y, uint8_t c)
{
    if (x < 0 || x >= SW_SCR_W || y < 0 || y >= SW_SCR_H) return;
    fb[y * SW_SCR_W + x] = c;
}

static void hline(int x0, int x1, int y, uint8_t c)
{
    if (y < 0 || y >= SW_SCR_H) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= SW_SCR_W) x1 = SW_SCR_W - 1;
    for (int x = x0; x <= x1; x++) fb[y * SW_SCR_W + x] = c;
}

static void rect(int x, int y, int w, int h, uint8_t c)
{
    for (int j = 0; j < h; j++) hline(x, x + w - 1, y + j, c);
}

static void frect(int x, int y, int w, int h, uint8_t c)
{
    rect(x, y, w, h, c);
}

static void circle(int cx, int cy, int r, uint8_t c)
{
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r) px(cx + x, cy + y, c);
}

static void text5x(int x, int y, const char *s, uint8_t c)
{
    /* tiny 3x5 uppercase font */
    static const uint8_t glyphs[26][5] = {
        {0x7,0x5,0x7,0x5,0x5}, /* A */ {0x7,0x5,0x6,0x5,0x7},
        {0x7,0x4,0x4,0x4,0x7}, /* C */ {0x6,0x5,0x5,0x5,0x6},
        {0x7,0x4,0x6,0x4,0x7}, /* E */ {0x7,0x4,0x6,0x4,0x4},
        {0x5,0x5,0x7,0x5,0x5}, /* G */ {0x5,0x5,0x7,0x5,0x5},
        {0x7,0x1,0x1,0x1,0x7}, /* I */ {0x1,0x1,0x1,0x5,0x7},
        {0x5,0x6,0x4,0x6,0x5}, /* K */ {0x4,0x4,0x4,0x4,0x7},
        {0x7,0x5,0x5,0x5,0x5}, /* M */ {0x7,0x5,0x5,0x5,0x7},
        {0x7,0x5,0x5,0x5,0x7}, /* O */ {0x7,0x5,0x7,0x4,0x4},
        {0x7,0x5,0x7,0x1,0x1}, /* Q */ {0x7,0x5,0x7,0x5,0x5},
        {0x7,0x5,0x6,0x5,0x5}, /* S */ {0x7,0x2,0x2,0x2,0x7},
        {0x4,0x4,0x4,0x4,0x4}, /* U */ {0x5,0x5,0x5,0x5,0x7},
        {0x5,0x5,0x5,0x7,0x5}, /* W */ {0x5,0x5,0x2,0x5,0x5},
        {0x5,0x5,0x5,0x5,0x7}, /* Y */ {0x7,0x1,0x2,0x4,0x7},
    };
    static const uint8_t digits[10][5] = {
        {0x7,0x5,0x5,0x5,0x7}, {0x2,0x6,0x2,0x2,0x7},
        {0x7,0x1,0x7,0x4,0x7}, {0x7,0x1,0x7,0x1,0x7},
        {0x5,0x5,0x7,0x1,0x1}, {0x7,0x4,0x7,0x1,0x7},
        {0x7,0x4,0x7,0x5,0x7}, {0x7,0x1,0x1,0x1,0x1},
        {0x7,0x5,0x7,0x5,0x7}, {0x7,0x5,0x7,0x1,0x7},
    };
    static const uint8_t punct[8][5] = {
        {0,0,0,0,0x2}, /* . */ {0,0,0x7,0,0}, /* + */
        {0,0,0,0x2,0}, {0x4,0x2,0x1,0,0}, /* / */
        {0x2,0x2,0,0,0}, {0x0,0,0,0,0x7}, /* - */
        {0,0x2,0x2,0,0}, {0x7,0,0,0,0},
    };
    for (; *s; s++, x += 4) {
        const uint8_t *g = NULL;
        char ch = *s;
        if (ch >= 'A' && ch <= 'Z') g = glyphs[ch - 'A'];
        else if (ch >= 'a' && ch <= 'z') g = glyphs[ch - 'a'];
        else if (ch >= '0' && ch <= '9') g = digits[ch - '0'];
        else if (ch == '.') g = punct[0];
        else if (ch == '+') g = punct[1];
        else if (ch == '-') g = punct[5];
        else if (ch == '!') g = punct[6];
        else if (ch == '/') g = punct[3];
        if (!g) continue;
        for (int r = 0; r < 5; r++)
            for (int cbit = 0; cbit < 3; cbit++)
                if (g[r] & (4 >> cbit)) px(x + cbit, y + r, c);
    }
}

static int scr_x(const sw_game *g, float wx) { return (int)wx - g->cam_x; }

static void draw_terrain(const sw_game *g)
{
    for (int sx = 0; sx < SW_SCR_W; sx++) {
        int wx = g->cam_x + sx;
        int gy = g->ground[wx];
        hline(sx, sx, gy, SW_BROWN);
        for (int y = gy + 1; y < SW_SCR_H; y++)
            px(sx, y, SW_GREEN);
        px(sx, gy - 1, SW_GREEN);
    }
}

static void draw_buildings(const sw_game *g)
{
    for (int i = 0; i < g->building_count; i++) {
        const sw_building *b = &g->buildings[i];
        int sx = scr_x(g, (float)b->x);
        if (sx + b->w < 0 || sx >= SW_SCR_W) continue;
        if (b->destroyed) {
            for (int j = 0; j < b->h / 3; j++)
                hline(sx, sx + b->w - 1, g->ground[b->x] - j, SW_DARKGRAY);
            continue;
        }
        uint8_t col = b->enemy ? SW_LIGHTRED : SW_LIGHTGRAY;
        frect(sx, g->ground[b->x] - b->h, b->w, b->h, col);
        if (b->kind == 'H') {
            frect(sx + b->w / 2 - 2, g->ground[b->x] - b->h - 3, 5, 3, SW_WHITE);
        } else if (b->kind == 'F') {
            frect(sx + 3, g->ground[b->x] - b->h + 4, 4, 5, SW_RED);
            frect(sx + b->w - 7, g->ground[b->x] - b->h + 4, 4, 5, SW_RED);
        }
    }
}

static void draw_plane(int sx, int sy, int angle, uint8_t body)
{
    /* fuselage: line of pixels along heading; wings and tail stubs */
    int dx = (angle == 0 || angle == 8) ? 0 : (angle < 8 ? 1 : -1);
    int dy = (angle == 4 || angle == 12) ? 0 : (angle < 4 || angle > 12 ? -1 : 1);
    for (int i = -5; i <= 5; i++) {
        int x = sx + dx * i, y = sy + dy * i;
        px(x, y, body);
        if (i == 0) { px(x, y - 2, body); px(x, y + 2, body); }
        if (i == 3 || i == -4) { px(x, y - 1, body); px(x, y + 1, body); }
    }
    px(sx - dx * 5, sy - dy * 5 + 1, body);
}

static void draw_tank(int sx, int sy, int dir, uint8_t c)
{
    frect(sx - 5, sy, 11, 4, c);
    frect(sx + dir * 2, sy - 3, 6, 3, c);
}

static void draw_balloon(int sx, int sy)
{
    circle(sx, sy, 6, SW_LIGHTGRAY);
    circle(sx, sy + 1, 4, SW_WHITE);
    for (int y = sy + 7; y < sy + 16 && y < SW_SCR_H; y++)
        px(sx, y, SW_DARKGRAY);
}

static void draw_hud(const sw_game *g)
{
    char buf[24];
    text5x(4, 4, "SCORE", SW_WHITE);
    snprintf(buf, sizeof buf, "%d", g->pl.score);
    text5x(4, 11, buf, SW_YELLOW);
    text5x(40, 4, "AMMO", SW_WHITE);
    snprintf(buf, sizeof buf, "%d", g->pl.bullets);
    text5x(40, 11, buf, SW_YELLOW);
    text5x(70, 4, "BOMBS", SW_WHITE);
    snprintf(buf, sizeof buf, "%d", g->pl.bombs);
    text5x(70, 11, buf, SW_YELLOW);
    if (g->msg_timer > 0)
        text5x(SW_SCR_W / 2 - (int)strlen(g->msg) * 2, 40, g->msg, SW_WHITE);
}

static void draw_actors(const sw_game *g)
{
    for (int i = 0; i < g->enemy_count; i++) {
        const sw_enemy *e = &g->enemies[i];
        if (!e->alive) continue;
        int sx = scr_x(g, e->x);
        if (sx < -20 || sx > SW_SCR_W + 20) continue;
        if (e->type == SW_ET_PLANE)      draw_plane(sx, (int)e->y, e->angle, SW_RED);
        else if (e->type == SW_ET_TANK)  draw_tank(sx, (int)e->y, e->dir, SW_DARKGRAY);
        else                             draw_balloon(sx, (int)e->y);
    }
    if (g->pl.alive)
        draw_plane(scr_x(g, g->pl.x), (int)g->pl.y, g->pl.angle, SW_YELLOW);
}

void sw_render_prepare(sw_game *g)
{
    int target = (int)g->pl.x - SW_SCR_W / 2;
    if (target < 0) target = 0;
    if (target > SW_WORLD_W - SW_SCR_W) target = SW_WORLD_W - SW_SCR_W;
    g->cam_x = target;
}

const uint8_t *sw_framebuffer(const sw_game *g)
{
    memset(fb, SW_CYAN, sizeof fb);              /* sky */
    rect(0, 0, SW_SCR_W, 18, SW_BLACK);          /* HUD band */
    draw_terrain(g);
    draw_buildings(g);
    draw_actors(g);
    for (int i = 0; i < (int)(sizeof g->bullets / sizeof g->bullets[0]); i++) {
        const sw_bullet *b = &g->bullets[i];
        if (b->alive) px(scr_x(g, b->x), (int)b->y, SW_WHITE);
    }
    for (int i = 0; i < (int)(sizeof g->bombs / sizeof g->bombs[0]); i++) {
        const sw_bomb *b = &g->bombs[i];
        if (b->alive) px(scr_x(g, b->x), (int)b->y, SW_LIGHTGRAY);
    }
    for (int i = 0; i < (int)(sizeof g->parts / sizeof g->parts[0]); i++) {
        const sw_particle *p = &g->parts[i];
        if (p->alive) px(scr_x(g, p->x), (int)p->y, (uint8_t)p->color);
    }
    draw_hud(g);
    return fb;
}
