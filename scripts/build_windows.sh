#!/usr/bin/env bash
#
# Windows build, cross compiled from Linux with MinGW-w64. Produces a folder holding
# i_forgor.exe and its assets, ready to be zipped and handed to someone.
#
#   PATH=/path/to/mingw64/bin:$PATH scripts/build_windows.sh
#
# Any MinGW-w64 g++ works: a distro package, a winlibs unpack, or the archive from
# niXman/mingw-builds-binaries (the Linux hosted one, since this script runs on Linux).

set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-build-windows}"
DIST_DIR="${DIST_DIR:-dist/i_forgor-windows-x86_64}"
NAME="i_forgor.exe"

if ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
    echo "x86_64-w64-mingw32-g++ is not on PATH." >&2
    echo "Install mingw-w64, or unpack a Linux hosted build and add its bin/ to PATH." >&2
    exit 1
fi

[ -e external/raylib/CMakeLists.txt ] || scripts/fetch_deps.sh

cmake -S . -B "$BUILD_DIR" \
      -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DIFG_BUILD_TESTS=OFF \
      -DIFG_BUILD_CAPTURE=OFF \
      ${IFG_ITCH_URL:+-DIFG_ITCH_URL="$IFG_ITCH_URL"}

cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 4)"

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

install -m 755 "$BUILD_DIR/bin/$NAME" "$DIST_DIR/$NAME"

if [ -d assets ]; then
    mkdir -p "$DIST_DIR/assets"
    for f in assets/*.ogg assets/*.wav assets/*.mp3 assets/*.qoa assets/*.xm assets/*.mod \
             assets/itch_url.txt; do
        [ -e "$f" ] && install -m 644 "$f" "$DIST_DIR/assets/"
    done
    rmdir "$DIST_DIR/assets" 2>/dev/null || true
fi

cp README.md "$DIST_DIR/" 2>/dev/null || true

cat > "$DIST_DIR/run.bat" <<'BAT'
@echo off
rem Keeps the console window from appearing behind the game.
start "" "%~dp0i_forgor.exe"
BAT

# Sanity check that the file really is a PE binary before it is called done.
file "$DIST_DIR/$NAME" | grep -q "PE32" || {
    echo "the built file is not a PE executable" >&2
    exit 1
}

echo
echo "built $DIST_DIR/$NAME"
echo "zip it and hand it over; the folder has to stay together"
