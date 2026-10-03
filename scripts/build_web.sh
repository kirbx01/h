#!/usr/bin/env bash
#
# Web build through emscripten. Emits a folder that can be served from any static host,
# or opened from disk with a local server.
#
#   scripts/build_web.sh
#
# Needs the emsdk on PATH (source emsdk/emsdk_env.sh, or point EMSDK at it).

set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-build-web}"
DIST_DIR="${DIST_DIR:-dist/i_forgor-web}"

if ! command -v emcmake >/dev/null 2>&1; then
    echo "emcmake is not on PATH." >&2
    echo "Install the emsdk and source its emsdk_env.sh first." >&2
    exit 1
fi

[ -e external/raylib/CMakeLists.txt ] || scripts/fetch_deps.sh

# Only the compressed audio goes into the browser build. The source WAVs are the same two
# recordings, only forty times the bytes, and the loader prefers .ogg anyway. If there is
# no compressed copy at all then the WAV is all there is, so it is used instead.
STAGE_DIR="${STAGE_DIR:-$BUILD_DIR/web-assets}"
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"

cp assets/bg.png "$STAGE_DIR/" 2>/dev/null || true
if [ -d assets/fonts ]; then
    cp -R assets/fonts "$STAGE_DIR/"
fi

shopt -s nullglob
compressed=(assets/*.ogg assets/*.mp3 assets/*.qoa assets/*.xm assets/*.mod)
raw=(assets/*.wav assets/*.flac)

if [ ${#compressed[@]} -gt 0 ]; then
    cp "${compressed[@]}" "$STAGE_DIR/"
else
    cp "${raw[@]}" "$STAGE_DIR/"
fi
cp assets/itch_url.txt "$STAGE_DIR/" 2>/dev/null || true
shopt -u nullglob

echo "preloading $(du -sh "$STAGE_DIR" | cut -f1) of audio into the web build"

emcmake cmake -S . -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE=Release \
      -DPLATFORM=Web \
      -DIFG_WEB_ASSET_DIR="$STAGE_DIR" \
      -DIFG_BUILD_TESTS=OFF \
      ${IFG_ITCH_URL:+-DIFG_ITCH_URL="$IFG_ITCH_URL"}

cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 4)"

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

# The preloaded folder, the page, the code and the data all have to travel together.
for f in i_forgor.js i_forgor.wasm i_forgor.data; do
    [ -e "$BUILD_DIR/bin/$f" ] && cp "$BUILD_DIR/bin/$f" "$DIST_DIR/"
done

# itch.io serves whatever the zip unpacks into, and it looks for index.html at the root, so
# the page is renamed on the way out. Everything else has to keep the name the .js expects,
# which is why only the html moves.
[ -e "$BUILD_DIR/bin/i_forgor.html" ] && cp "$BUILD_DIR/bin/i_forgor.html" "$DIST_DIR/index.html"

# A zip with index.html at the root is exactly what the itch.io HTML5 upload wants.
( cd "$DIST_DIR" && zip -q -r -9 "../$(basename "$DIST_DIR").zip" . )

echo
echo "built $DIST_DIR"
echo "packed ${DIST_DIR}.zip   <- this is the file to upload to itch.io (HTML5)"

# Serve it: emscripten's own server, or anything else that hands out the right types.
if command -v emrun >/dev/null 2>&1; then
    echo "try it with:  emrun $BUILD_DIR/bin/i_forgor.html"
fi
