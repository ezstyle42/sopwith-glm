/*
 * sopwith.c - clean-room reimplementation of the core mechanics of
 * Sopwith: side-scrolling biplane flight, gun, bombs, tanks, balloons,
 * ox-hunting and destruction of enemy structures.
 */
#include "sopwith.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define GRAVITY      0.035f
#define THRUST       0.05f
#define DRAG         0.995f
#define MAX_SPEED    3.4f
#define BULLET_SPEED 6.0f
#define ENEMY_SPEED  1.1f

static const int8_t cos_t[16] = { 10, 9, 8, 5, 0, -5, -8, -9, -10, -9, -8, -5, 0, 5, 8, 9 };
static const int8_t sin_t[16] = { 0, 4, 7, 9, 10, 9, 7, 4, 0, -4, -7, -9, -10, -9, -7, -4 };

static int rnd(sw_game *g)
{
    g->seed = g->seed * 1103515245u + 12345u;
    return (g->seed >> 16) & 0x7fff;
}


static void msg(sw_game *g, const char *text)
{
    snprintf(g->msg, sizeof g->msg, "%s", text);
    g->msg_timer = 120;
}

static int ground_at(const sw_game *g, int px)
{
    if (px < 0) px = 0;
    if (px >= SW_WORLD_W) px = SW_WORLD_W - 1;
    return g->ground[px];
}

static void spawn_particle(sw_game *g, float x, float y, int color)
{
    for (int i = 0; i < (int)(sizeof g->parts / sizeof g->parts[0]); i++) {
        sw_particle *p = &g->parts[i];
        if (!p->alive) {
            p->x = x; p->y = y;
            p->alive = true;
            p->life = 20 + rnd(g) % 20;
            p->color = color;
            return;
        }
    }
}

static void add_bullet(sw_game *g, float x, float y, float vx, float vy, bool friendly)
{
    (void)friendly;
    for (int i = 0; i < (int)(sizeof g->bullets / sizeof g->bullets[0]); i++) {
        sw_bullet *b = &g->bullets[i];
        if (!b->alive) {
            b->x = x; b->y = y;
            b->vx = vx; b->vy = vy;
            b->ttl = 70;
            b->alive = true;
            return;
        }
    }
}

static void add_bomb(sw_game *g, float x, float y, float vx, float vy)
{
    for (int i = 0; i < (int)(sizeof g->bombs / sizeof g->bombs[0]); i++) {
        sw_bomb *b = &g->bombs[i];
        if (!b->alive) {
            b->x = x; b->y = y;
            b->vx = vx; b->vy = vy;
            b->ttl = 200;
            b->alive = true;
            return;
        }
    }
}

static void build_terrain(sw_game *g)
{
    int y = 170;
    int i = 0;
    while (i < SW_WORLD_W) {
        int run = 40 + rnd(g) % 140;
        int dy = (rnd(g) % 25) - 12;
        for (int k = 0; k < run && i < SW_WORLD_W; k++, i++) {
            int ny = y + dy * k / run;
            g->ground[i] = ny;
        }
        y += dy;
        if (y < 120) y = 120;
        if (y > 185) y = 185;
    }
    /* flatten around home base */
    for (int i = 300; i < 420; i++)
        g->ground[i] = g->base_ground = g->ground[350];
    /* flatten around enemy base at far end */
    for (int i = SW_WORLD_W - 320; i < SW_WORLD_W - 200; i++)
        g->ground[i] = g->ground[SW_WORLD_W - 260];
}

static void add_building(sw_game *g, int x, char kind, bool enemy)
{
    sw_building *b = &g->buildings[g->building_count++];
    b->x = x;
    b->w = (kind == 'F') ? 24 : 32;
    b->h = (kind == 'F') ? 26 : 22;
    b->kind = kind;
    b->enemy = enemy;
    b->destroyed = false;
}

static void build_bases(sw_game *a)
{
    add_building(a, 320, 'H', false);   /* home hangar  */
    add_building(a, 356, 'F', false);   /* home fuel    */
    add_building(a, SW_WORLD_W - 300, 'H', true);
    add_building(a, SW_WORLD_W - 264, 'F', true);
}

static void spawn_enemies(sw_game *g)
{
    g->enemy_count = 0;
    for (int i = 0; i < 3; i++) {
        sw_enemy *e = &g->enemies[g->enemy_count++];
        e->type = SW_ET_PLANE;
        e->x = 800 + i * 700 + rnd(g) % 200;
        e->y = 60 + rnd(g) % 80;
        e->angle = (rnd(g) % 16);
        e->hp = 1;
        e->cooldown = 60;
        e->alive = true;
    }
    for (int i = 0; i < 4; i++) {
        sw_enemy *e = &g->enemies[g->enemy_count++];
        e->type = SW_ET_TANK;
        e->x = 600 + i * 500 + rnd(g) % 200;
        e->y = 0;   /* snapped to ground during update */
        e->dir = (rnd(g) % 2) ? 1 : -1;
        e->hp = 1;
        e->cooldown = 100 + rnd(g) % 100;
        e->alive = true;
    }
    for (int i = 0; i < 2; i++) {
        sw_enemy *e = &g->enemies[g->enemy_count++];
        e->type = SW_ET_BALLOON;
        e->x = 1000 + i * 1000 + rnd(g) % 300;
        e->y = 50 + rnd(g) % 40;
        e->hp = 1;
        e->alive = true;
    }
}

void sw_init(sw_game *g, unsigned seed)
{
    memset(g, 0, sizeof *g);
    g->seed = (int)(seed ? seed : 1u);
    g->state = SW_STATE_PLAY;
    g->pl.x = 340;
    g->pl.y = 100;
    g->pl.angle = 0;              /* flying level, heading east */
    g->pl.vx = 1.0f;
    g->pl.bombs = 5;
    g->pl.bullets = 1000;
    g->pl.lives = 3;
    g->pl.alive = true;
    g->targets_left = 2;          /* enemy structures to destroy */
    build_terrain(g);
    build_bases(g);
    spawn_enemies(g);
    msg(g, "SOPWITH -- Press h for help");
}

static void update_player(sw_game *g, const sw_input *in)
{
    if (!g->pl.alive) {
        if (--g->pl.respawn_timer <= 0) {
            if (g->pl.lives > 0) {
                g->pl.alive = true;
                g->pl.on_ground = false;
                g->pl.x = 340;
                g->pl.y = 80;
                g->pl.vx = 1.0f;
                g->pl.vy = 0;
                g->pl.angle = 0;
                g->pl.bombs = 5;
            } else {
                g->state = SW_STATE_GAME_OVER;
            }
        }
        return;
    }

    if (in->turn_ccw) g->pl.angle = (g->pl.angle + 1) & 15;
    if (in->turn_cw)  g->pl.angle = (g->pl.angle + 15) & 15;

    float ax = cos_t[g->pl.angle] * (THRUST + (in->accelerate ? 0.025f : 0));
    float ay = -sin_t[g->pl.angle] * (THRUST + (in->accelerate ? 0.025f : 0));
    g->pl.vx = (g->pl.vx + ax) * DRAG;
    g->pl.vy = (g->pl.vy + ay + GRAVITY) * DRAG;

    float sp2 = g->pl.vx * g->pl.vx + g->pl.vy * g->pl.vy;
    if (sp2 > MAX_SPEED * MAX_SPEED) {
        float s = MAX_SPEED / sqrtf(sp2);
        g->pl.vx *= s;
        g->pl.vy *= s;
    }

    g->pl.x += g->pl.vx;
    g->pl.y += g->pl.vy;

    if (g->pl.y < 8) { g->pl.y = 8; g->pl.vy = 0; }
    if (g->pl.x < 0) g->pl.x = 0;
    if (g->pl.x > SW_WORLD_W - 4) g->pl.x = SW_WORLD_W - 4;

    int gy = ground_at(g, (int)g->pl.x);
    if (g->pl.y >= gy - 4) {
        bool level = (g->pl.angle == 0 || g->pl.angle == 8);
        bool slow  = (g->pl.vx * g->pl.vx + g->pl.vy * g->pl.vy) < 2.0f;
        if (level && slow && g->pl.vy >= -0.5f) {
            g->pl.on_ground = true;
            g->pl.y = gy - 4;
            g->pl.vy = 0;
        } else {
            g->pl.crashed = true;
        }
    } else {
        g->pl.on_ground = false;
    }

    if (g->pl.crashed) {
        g->pl.alive = false;
        g->pl.crashed = false;
        g->pl.lives--;
        g->pl.respawn_timer = 90;
        g->state = SW_STATE_PLAYER_DOWN;
        for (int i = 0; i < 12; i++)
            spawn_particle(g, g->pl.x, g->pl.y, SW_YELLOW + (i % 3));
        msg(g, "CRASHED!");
        return;
    }

    if (in->fire && g->pl.fire_cooldown <= 0 && g->pl.bullets > 0) {
        float m = BULLET_SPEED;
        add_bullet(g, g->pl.x + cos_t[g->pl.angle], g->pl.y - sin_t[g->pl.angle],
                   g->pl.vx + cos_t[g->pl.angle] * m / 10.0f,
                   g->pl.vy - sin_t[g->pl.angle] * m / 10.0f, true);
        g->pl.fire_cooldown = 10;
        g->pl.bullets--;
    }
    if (in->bomb && g->pl.bombs > 0 && !g->pl.on_ground) {
        add_bomb(g, g->pl.x, g->pl.y, g->pl.vx, g->pl.vy + 0.5f);
        g->pl.bombs--;
    }
    if (g->pl.fire_cooldown > 0) g->pl.fire_cooldown--;
}

static void update_enemies(sw_game *g)
{
    for (int i = 0; i < g->enemy_count; i++) {
        sw_enemy *e = &g->enemies[i];
        if (!e->alive) continue;
        if (e->type == SW_ET_PLANE) {
            e->x += cos_t[e->angle] * 0.11f;
            e->y -= sin_t[e->angle] * 0.11f;
            if (e->y < 30) { e->y = 30; e->angle = (e->angle + 15) & 15; }
            int gy = ground_at(g, (int)e->x);
            if (e->y > gy - 10) { e->angle = (e->angle + 1) & 15; e->y = gy - 10; }
            e->cooldown--;
            float dx = g->pl.x - e->x, dy = g->pl.y - e->y;
            bool in_range = dx * dx + dy * dy < 100 * 100;
            if (in_range && e->cooldown <= 0 && g->pl.alive) {
                /* crude: fire toward player */
                float d = dx * dx + dy * dy;
                d = d > 0 ? d : 1;
                add_bullet(g, e->x, e->y, dx / 20.0f, dy / 20.0f, false);
                e->cooldown = 80 + rnd(g) % 80;
            }
        } else if (e->type == SW_ET_TANK) {
            int gy = ground_at(g, (int)e->x);
            e->y = gy - 6;
            e->x += e->dir * 0.15f;
            if (e->x < 100) e->dir = 1;
            if (e->x > SW_WORLD_W - 100) e->dir = -1;
            e->cooldown--;
            if (e->cooldown <= 0 && g->pl.alive) {
                float dx = g->pl.x - e->x, dy = g->pl.y - e->y;
                if (dx * dx + dy * dy < 150 * 150) {
                    add_bullet(g, e->x, e->y - 6, dx / 25.0f, dy / 25.0f, false);
                    e->cooldown = 140 + rnd(g) % 80;
                } else {
                    e->cooldown = 30;
                }
}
        } else if (e->type == SW_ET_BALLOON) {
            e->y += 0.02f * ((rnd(g) % 3) - 1);
            if (e->y < 30) e->y = 30;
            if (e->y > 100) e->y = 100;
        }
    }
}

static void damage_building(sw_game *g, sw_building *b)
{
    if (b->destroyed) return;
    b->destroyed = true;
    for (int i = 0; i < 10; i++)
        spawn_particle(g, b->x + rnd(g) % b->w, ground_at(g, b->x) - rnd(g) % 20, SW_YELLOW);
    if (b->enemy) {
        g->targets_left--;
        g->pl.score += 100;
        if (g->targets_left <= 0) {
            g->state = SW_STATE_WIN;
            msg(g, "MISSION COMPLETE!");
        } else {
            msg(g, "TARGET DESTROYED");
        }
    } else {
        msg(g, "YOU DESTROYED YOUR OWN BASE");
    }
}

static void update_projectiles(sw_game *g)
{
    for (int i = 0; i < (int)(sizeof g->bullets / sizeof g->bullets[0]); i++) {
        sw_bullet *b = &g->bullets[i];
        if (!b->alive) continue;
        b->x += b->vx; b->y += b->vy;
        if (--b->ttl <= 0 || b->x < 0 || b->x > SW_WORLD_W || b->y < 0) { b->alive = false; continue; }
        if (b->y >= ground_at(g, (int)b->x)) { b->alive = false; spawn_particle(g, b->x, b->y, SW_LIGHTGRAY); continue; }
        for (int j = 0; j < g->enemy_count; j++) {
            sw_enemy *e = &g->enemies[j];
            if (!e->alive) continue;
            if (b->x > e->x - 6 && b->x < e->x + 6 && b->y > e->y - 5 && b->y < e->y + 5) {
                e->alive = false;
                b->alive = false;
                g->pl.score += 50;
                for (int k = 0; k < 8; k++)
                    spawn_particle(g, e->x, e->y, SW_LIGHTRED);
                msg(g, e->type == SW_ET_PLANE ? "PLANE DOWN" :
                           e->type == SW_ET_TANK ? "TANK DOWN" : "BALLOON DOWN");
            }
        }
        if (b->alive && g->pl.alive) {
            if (b->x > g->pl.x - 5 && b->x < g->pl.x + 5 && b->y > g->pl.y - 4 && b->y < g->pl.y + 4) {
                b->alive = false;
                g->pl.alive = false;
                g->pl.lives--;
                g->pl.respawn_timer = 90;
                g->state = SW_STATE_PLAYER_DOWN;
                for (int k = 0; k < 12; k++)
                    spawn_particle(g, g->pl.x, g->pl.y, SW_LIGHTRED);
                msg(g, "YOU'RE HIT!");
            }
        }
    }

    for (int i = 0; i < (int)(sizeof g->bombs / sizeof g->bombs[0]); i++) {
        sw_bomb *b = &g->bombs[i];
        if (!b->alive) continue;
        b->vy += GRAVITY;
        b->x += b->vx; b->y += b->vy;
        if (--b->ttl <= 0) { b->alive = false; continue; }
        if (b->y >= ground_at(g, (int)b->x)) {
            b->alive = false;
            bool hit = false;
            for (int j = 0; j < g->building_count; j++) {
                sw_building *bl = &g->buildings[j];
                if (!bl->destroyed && b->x > bl->x - 4 && b->x < bl->x + bl->w + 4) {
                    damage_building(g, bl);
                    hit = true;
                    break;
                }
            }
            if (!hit)
                for (int k = 0; k < 8; k++)
                    spawn_particle(g, b->x, ground_at(g, (int)b->x), SW_BROWN);
            /* also kill enemies caught in blast */
            for (int j = 0; j < g->enemy_count; j++) {
                sw_enemy *e = &g->enemies[j];
                if (e->alive && b->x > e->x - 12 && b->x < e->x + 12) {
                    e->alive = false;
                    g->pl.score += 50;
                    spawn_particle(g, e->x, e->y, SW_LIGHTRED);
                }
            }
        }
    }

    for (int i = 0; i < (int)(sizeof g->parts / sizeof g->parts[0]); i++) {
        sw_particle *p = &g->parts[i];
        if (!p->alive) continue;
        p->life--;
        p->y += 0.3f;
        if (p->life <= 0) p->alive = false;
    }
}

void sw_update(sw_game *g, const sw_input *in)
{
    if (g->msg_timer > 0) g->msg_timer--;
    if (g->state == SW_STATE_PLAY || g->state == SW_STATE_PLAYER_DOWN) {
        update_player(g, in);
        update_enemies(g);
        update_projectiles(g);
    } else {
        /* end screens: wait for fire to restart */
        if (in->fire) {
            unsigned s = (unsigned)g->seed;
            sw_init(g, s);
        }
    }
    (void)cos_t; (void)sin_t;
}
