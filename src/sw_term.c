/*
 * sw_term.c - dependency-free ANSI terminal frontend.
 * Renders the 320x200 framebuffer as half-block characters at 160x100
 * "pixels" using 24-bit color escape sequences. Works on any POSIX or
 * Windows-10+ console with VT processing enabled.
 *
 * Controls: , (roll ccw), . (roll cw), space (fire), b (bomb),
 *           x (accelerate), q (quit).
 */
#include "sopwith.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#endif

static const int pal[16][3] = {
    {0,0,0},{0,0,170},{0,170,0},{0,170,170},
    {170,0,0},{170,0,170},{170,85,0},{170,170,170},
    {85,85,85},{85,85,255},{85,255,85},{85,255,255},
    {255,85,85},{255,85,255},{255,255,85},{255,255,255}
};

#ifdef _WIN32
static int win_kbhit(void) { return _kbhit(); }
static int win_getch(void) { return _getch(); }
static void term_init(void)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD m = 0;
    if (GetConsoleMode(h, &m))
        SetConsoleMode(h, m | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
#else
static struct termios orig;
static void term_init(void)
{
    tcgetattr(0, &orig);
    struct termios t = orig;
    t.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(0, TCSANOW, &t);
    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
}
static void term_restore(void)
{
    tcsetattr(0, TCSANOW, &orig);
    fcntl(0, F_SETFL, fcntl(0, F_GETFL) & ~O_NONBLOCK);
}
#endif

static void msleep(int ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
#endif
}

static char readkey(void)
{
#ifdef _WIN32
    return win_kbhit() ? (char)win_getch() : 0;
#else
    char c; ssize_t n = read(0, &c, 1);
    return n == 1 ? c : 0;
#endif
}

int main(int argc, char **argv)
{
    static sw_game game;
    sw_input in = {0};
    unsigned seed = (argc > 1) ? (unsigned)atoi(argv[1]) : 1;
    sw_init(&game, seed);
    sw_render_prepare(&game);

    term_init();
    printf("\033[2J\033[?25l");

    for (;;) {
        char c;
        while ((c = readkey()) != 0) {
            switch (c) {
            case ',': in.turn_ccw = true; break;
            case '.': in.turn_cw  = true; break;
            case ' ': in.fire = true; break;
            case 'b': in.bomb = true; break;
            case 'x': in.accelerate = true; break;
            case 'q': goto out;
            default: break;
            }
        }

        sw_render_prepare(&game);
        sw_update(&game, &in);
        memset(&in, 0, sizeof in);

        const uint8_t *fb = sw_framebuffer(&game);

        /* two scanlines per character row (half-block). Colors are
           emitted only when they change, keeping frames small. */
        printf("\033[H");
        for (int y = 0; y < SW_SCR_H; y += 2) {
            int last_t = -1, last_b = -1;
            for (int x = 0; x < SW_SCR_W; x += 2) {
                int t = fb[y * SW_SCR_W + x];
                int b = fb[(y + 1) * SW_SCR_W + x + 1];
                if (t != last_t)
                    printf("\033[38;2;%d;%d;%dm", pal[t][0], pal[t][1], pal[t][2]);
                if (b != last_b)
                    printf("\033[48;2;%d;%d;%dm", pal[b][0], pal[b][1], pal[b][2]);
                fputs("\xE2\x96\x80", stdout);
                last_t = t; last_b = b;
            }
            fputs("\033[0m\n", stdout);
        }
        fflush(stdout);
        msleep(33);
    }
out:
    printf("\033[0m\033[?25h\033[2J");
#ifndef _WIN32
    term_restore();
#endif
    return 0;
}
