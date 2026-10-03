#!/usr/bin/env bash
#
# Pulls the engine and its UI kit into external/.
#
# external/ is not tracked, so a fresh clone has nothing to build against. This fetches
# the two pinned versions the game is written for. Existing checkouts are left alone.

set -euo pipefail

cd "$(dirname "$0")/.."

RAYLIB_VERSION="${RAYLIB_VERSION:-5.5}"
RAYGUI_VERSION="${RAYGUI_VERSION:-5.1}"

mkdir -p external

fetch() {
    # $1 = url, $2 = destination directory, $3 = expected marker file
    local url="$1" dest="$2" marker="$3"

    if [ -e "$marker" ]; then
        echo "already there: $dest"
        return 0
    fi

    echo "fetching $(basename "$dest") ..."
    local tmp
    tmp="$(mktemp)"

    if command -v curl >/dev/null 2>&1; then
        curl -fL --retry 3 -o "$tmp" "$url"
    elif command -v wget >/dev/null 2>&1; then
        wget -q -O "$tmp" "$url"
    else
        echo "need curl or wget" >&2
        exit 1
    fi

    rm -rf "$dest"
    mkdir -p "$dest"
    tar -xzf "$tmp" -C "$dest" --strip-components=1
    rm -f "$tmp"

    if [ ! -e "$marker" ]; then
        echo "expected $marker to exist after unpacking $dest" >&2
        exit 1
    fi
}

fetch "https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz" \
      "external/raylib" "external/raylib/src/raylib.h"

fetch "https://github.com/raysan5/raygui/archive/refs/tags/${RAYGUI_VERSION}.tar.gz" \
      "external/raygui" "external/raygui/src/raygui.h"

echo
echo "ready. Build with:"
echo "  cmake -S . -B build && cmake --build build -j"