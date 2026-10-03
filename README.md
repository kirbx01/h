# i forgor

A monochrome game about temporary things, built with [raylib](https://www.raylib.com) and
[raygui](https://github.com/raysan5/raygui).

Five passages through the same eroding board. Hard impacts strip pips off the tiles until
they are gone, and gone stays gone: damage carries across every passage, every retry and
every restart. The only thing you build is the trace of where you have just been, and that
fades too.

The game opens fullscreen on the desktop it is running on, and every layout is computed from
whatever the window currently is.

The game is drawn in pure `#000000` on `#E0E1E4`, including in the menus. The only artwork
is `assets/bg.png`, and it is used on the main screen alone.

## Controls

| Input | Action |
| --- | --- |
| `W` `A` `S` `D` or arrow keys | Move the ball |
| `R` | Begin the passage again |
| `Esc` | Menu |
| `F11` | Leave or re-enter fullscreen |
| `Tab` / `Enter` | Move between and press buttons on the menus |
| Mouse | Clickable buttons and links |

Two icons sit in the top right while you play: the sliders open the settings, and the
speaker mutes and unmutes the music without leaving the passage.

## Building

`external/` is not tracked. Pull the pinned engine and UI kit in once:

```sh
scripts/fetch_deps.sh
```

Then build, test and package:

```sh
scripts/build_linux.sh
scripts/build_windows.sh      # needs x86_64-w64-mingw32-g++ on PATH
scripts/build_web.sh          # needs the emsdk on PATH
```

`build_linux.sh` leaves a folder and a tarball in `dist/` holding the executable, its
assets and nothing else. `build_windows.sh` does the same for Windows and checks that the
result is a PE binary before it calls itself done.

`build_web.sh` writes a folder that any static host will serve. Only the compressed audio
goes into it; if there were no compressed copy to begin with, the WAV is used instead.

Verified here: the Linux build, the smoke test and the emscripten build. The Windows
cross build has not been run on this machine, because the MinGW-w64 archive it needs did
not finish downloading; nothing in the tree is Windows-specific beyond the toolchain
choice, but treat that first `.exe` as unproven.

A plain CMake build works too, and is what the scripts drive:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The Makefile is kept for the fast loop and needs nothing but a C++17 compiler:

```sh
make -j      # ./i_forgor
make test    # headless smoke test
make capture # writes PNGs of every screen to build-capture/
```

## Music

One track runs under the whole game: the opening, the passages, the menu, the credits and
the settings screen. It fades in over two and a half seconds and fades out over the last
one and a half before the window closes. Every button press also plays a short pop.

| File | Where it plays |
| --- | --- |
| `assets/openingandclosing.ogg` | Everywhere |
| `assets/button_pop.wav` | Button clicks |

Filenames are probed rather than hardcoded, along with `.wav`, `.mp3`, `.qoa`, `.xm` and
`.mod` versions of the same name, and the track can be pointed elsewhere with the
`IFG_AUDIO` environment variable. A missing track is never silent by accident: the game logs
which file it looked for and carries on without music.

Only the compressed copies are tracked, because a 43 MB pair of WAV files is a heavy thing
to ask anyone to clone and an even heavier thing to download in a browser. The loader
prefers `.ogg`.

## The itch.io link

The credits will not invent a link. They read `assets/itch_url.txt` if it exists, and
otherwise the value baked in at build time:

```sh
cmake -S . -B build -DIFG_ITCH_URL="https://itch.io/your-page"
make IFG_ITCH_URL="https://itch.io/your-page"
```

With neither set, the credits say the page is not configured instead of showing a link
that goes nowhere.

## Layout

```
src/game/       board, ball, trail, story, screen flow, save file, responsive layout
src/sound/      track and pop loading, fades, mute
src/ui/         raygui menus, settings, credits, the clickable link
src/input/      per-screen keyboard and mouse handling
src/platform/   save locations, file access, opening a URL, localStorage on web
tests/smoke.cpp headless check of the whole progression
tools/capture.cpp screenshot tool, used to actually look at the screens
```

`src/ui/ui.cpp` is the only translation unit that defines `RAYGUI_IMPLEMENTATION`, and
`make test` links the simulation without it, so the smoke test needs no window, no input
device and no sound card.
## Typefaces

`assets/fonts/seratonin` sets the body text and `assets/fonts/dosmic` sets the title. Both
ship with their own licence files, and **both are personal-use only and forbid commercial
use**. Redistribution of this repository, or of a build of it, needs a commercial licence
from each author or a different typeface. Check `assets/fonts/*/` before publishing.
