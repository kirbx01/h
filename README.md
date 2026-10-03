# AFTER THE FALL

A monochrome game about temporary things, built with [raylib](https://www.raylib.com) and
[raygui](https://github.com/raysan5/raygui).

Five passages through the same eroding board. Hard impacts strip pips off the tiles until
they are gone, and gone stays gone: damage carries across every passage, every retry and
every restart. The only thing you build is the trace of where you have just been, and that
fades too.

There is no colour anywhere in the game, including in the menus.

## Controls

| Input | Action |
| --- | --- |
| `W` `A` `S` `D` or arrow keys | Move the ball |
| `R` | Begin the passage again |
| `Esc` | Menu |
| `Tab` / `Enter` | Move between and press buttons on the menus |
| Mouse | Clickable buttons and links |

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

A plain CMake build works too, and is what the scripts drive:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The Makefile is kept for the fast loop and needs nothing but a C++17 compiler:

```sh
make -j      # ./after_the_fall
make test    # headless smoke test
make capture # writes PNGs of every screen to build-capture/
```

## Music

Two tracks, and each is used where it belongs:

| File | Where it plays |
| --- | --- |
| `assets/baseorignal.wav` | The passages |
| `assets/openingandclosing.wav` | The opening, the ending, the menu, the credits and the settings screen, including when settings is opened from a pause |

Both filenames are probed rather than hardcoded, along with `.ogg`, `.mp3`, `.qoa`, `.xm`
and `.mod` versions of the same names, and either can be pointed elsewhere with the
`ATF_AUDIO` and `ATF_AUDIO_FRAME` environment variables. A missing track is never silent
by accident: the game logs which file it looked for and carries on without music.

Switching between the two crossfades, and a pause holds whatever was already playing.

The passage track is deliberately degraded as the world deteriorates: each passage turns
it down, closes a low-pass filter across its top end and starts dropping out of it, which
is the only sound effect in the game.

`.ogg` copies of both tracks are what the repository carries, because a 43 MB pair of WAV
files is a heavy thing to ask anyone to clone and an even heavier thing to download in a
browser. The loader prefers `.ogg`, so if you drop a `.wav` in beside it nothing changes
for the desktop builds.

## The itch.io link

The credits will not invent a link. They read `assets/itch_url.txt` if it exists, and
otherwise the value baked in at build time:

```sh
cmake -S . -B build -DATF_ITCH_URL="https://itch.io/your-page"
make ATF_ITCH_URL="https://itch.io/your-page"
```

With neither set, the credits say the page is not configured instead of showing a link
that goes nowhere.

## Layout

```
src/game/       board, ball, trail, story, screen flow, save file, rendering
src/sound/      track loading, per-passage degradation, crossfades
src/ui/         raygui menus, settings, credits, the clickable link
src/input/      per-screen keyboard and mouse handling
src/platform/   save locations, file access, opening a URL, localStorage on web
tests/smoke.cpp headless check of the whole progression
tools/capture.cpp screenshot tool, used to actually look at the screens
```

`src/ui/ui.cpp` is the only translation unit that defines `RAYGUI_IMPLEMENTATION`, and
`make test` links the simulation without it, so the smoke test needs no window, no input
device and no sound card.