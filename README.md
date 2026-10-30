# Sopwith — modern cross-platform reimplementation

A clean-room, cross-platform reimplementation of the classic 1984
side-scrolling WWI biplane game **Sopwith** (`sw.exe`, "SOPWITH, Version
7.F15" — (c) 1984/85/87 BMB Compuscience Canada Ltd., (c) 1994-96
David L. Clark). This project was created by studying a raw disassembly
of the original DOS executable and reimplementing its mechanics with
modern, portable tools. No original code or assets are included.

## What it is

You fly a Sopwith Camel over a side-scrolling landscape of fields,
trees and bases. Bomb enemy structures, shoot down enemy planes, tanks
and observation balloons, avoid crashing — and don't hit the ox.

* Scrollable 3200-unit world with procedurally generated terrain
* 16-direction flight model with gravity, thrust and drag
* Machine gun and bombs
* Enemy fighters, tanks and balloons
* Home and enemy bases with hangars and fuel depots
* Scoring, lives, respawn, win/lose states

## Architecture

The game core (`src/sopwith.c`, `src/render.c`) is pure C99 with zero
platform dependencies: it renders into a 320×200 CGA-palette framebuffer
and consumes an abstract input state. Frontends blit that framebuffer:

| Frontend | deps | notes |
|----------|------|-------|
| `sw_sdl`  | SDL2 | windowed 3×-scaled 960×600 view, vsync |
| `sw_term` | none | ANSI true-color terminal, half-block 160×100 view |

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build            # run headless simulation tests
./build/sw_sdl                    # graphical (needs SDL2)
./build/sw_term                   # terminal, no dependencies
```

If SDL2 is not installed the SDL frontend is skipped automatically and
only `sw_term` plus the test binary are built.

## Controls

| key | action |
|-----|--------|
| `,` / `←` | roll counter-clockwise (nose up) |
| `.` / `→` | roll clockwise (nose down) |
| `space` / `f` | fire machine gun |
| `b` | drop bomb |
| `x` / `↑` | extra engine power |
| `q` / `Esc` | quit |

## Testing

`test_headless` runs deterministic multi-thousand-frame simulations and
asserts world-bounds invariants, determinism for identical seeds, and
framebuffer content (HUD, sky, terrain, player sprite). It is run by
`ctest`.

## License

This reimplementation is distributed under the MIT license (see
LICENSE). "Sopwith" is a 1984 game by David L. Clark / BMB
Compuscience; this project is an independent tribute and is not
affiliated with the original authors.
