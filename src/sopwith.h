/*
 * sopwith.h - shared core of a modern, cross-platform reimplementation
 * of the classic 1984 side-scrolling biplane game "Sopwith"
 * (originally sw.exe by David L. Clark / BMB Compuscience, 1984-1996).
 *
 * The core is pure C99 with no I/O or platform dependencies. It renders
 * into a 320x200 CGA-style framebuffer (16-color palette) which the
 * frontends (ANSI terminal, SDL2) blit to the screen, and it consumes
 * an abstract input state. This keeps game logic testable and portable.
 */
#ifndef SOPWITH_CORE_H
#define SOPWITH_CORE_H

#include <stdint.h>
#include <stdbool.h>

#define SW_SCR_W   320
#define SW_SCR_H   200
#define SW_WORLD_W 3200

/* CGA-ish 16 color palette indices */
enum {
    SW_BLACK = 0, SW_BLUE, SW_GREEN, SW_CYAN,
    SW_RED, SW_MAGENTA, SW_BROWN, SW_LIGHTGRAY,
    SW_DARKGRAY, SW_LIGHTBLUE, SW_LIGHTGREEN, SW_LIGHTCYAN,
    SW_LIGHTRED, SW_LIGHTMAGENTA, SW_YELLOW, SW_WHITE
};

typedef struct {
    bool turn_ccw;   /* roll counter-clockwise / nose up   */
    bool turn_cw;    /* roll clockwise / nose down          */
    bool fire;       /* machine gun                         */
    bool bomb;       /* drop bomb                           */
    bool accelerate; /* extra engine power                  */
    bool quit;
} sw_input;

typedef enum {
    SW_STATE_TITLE,
    SW_STATE_PLAY,
    SW_STATE_PLAYER_DOWN,
    SW_STATE_GAME_OVER,
    SW_STATE_WIN
} sw_state;

typedef struct {
    float x, y;
    float vx, vy;
} sw_vec;

typedef enum {
    SW_ET_PLANE, SW_ET_TANK, SW_ET_BALLOON, SW_ET_SHELL
} sw_enemy_type;

typedef struct {
    sw_enemy_type type;
    float x, y, vx, vy;
    int   angle;      /* planes: 0..15 heading */
    int   cooldown;
    int   hp;
    int   dir;        /* tanks: travel direction */
    bool  alive;
} sw_enemy;

typedef struct {
    float x, y, vx, vy;
    int   ttl;
    bool  alive;
} sw_bullet;

typedef struct {
    float x, y, vx, vy;
    int   ttl;
    bool  alive;
} sw_bomb;

typedef struct {
    float x, y;
    int   life;
    bool  alive;
    int   color;
} sw_particle;

typedef struct {
    int  x, w, h;
    bool enemy;       /* enemy target vs home structure */
    bool destroyed;
    char kind;        /* 'B' building, 'F' fuel, 'H' hangar, 'P' power */
} sw_building;

typedef struct {
    int   state;
    int   frame;
    int   seed;
    int   ground[SW_WORLD_W];
    int   base_ground;

    struct {           /* player */
        float x, y, vx, vy;
        int   angle;   /* 0..15, 0 = east, 4 = north */
        int   bombs, bullets, lives, score;
        int   fire_cooldown, respawn_timer, hit_flash;
        bool  alive, on_ground, crashed;
        int   throttle;
    } pl;

    sw_enemy    enemies[12];
    int         enemy_count;
    sw_bullet   bullets[24];
    sw_bomb     bombs[8];
    sw_particle parts[64];
    sw_building buildings[16];
    int         building_count;

    int   cam_x;
    int   msg_timer;
    char  msg[48];
    int   targets_left;
    int   home_left;
} sw_game;

void sw_init(sw_game *g, unsigned seed);
void sw_update(sw_game *g, const sw_input *in);
void sw_render_prepare(sw_game *g);               /* update camera */
const uint8_t *sw_framebuffer(const sw_game *g);   /* SW_SCR_W*SW_SCR_H bytes */

#endif /* SOPWITH_CORE_H */
