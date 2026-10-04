#!/usr/bin/env bash

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

STAGE_DIR="${STAGE_DIR:-$BUILD_DIR/web-assets}"
rm -rf "$STAGE_DIR"
mkdir -p "$STAGE_DIR"

cp assets/bg.png "$STAGE_DIR/" 2>/dev/null || true
if [ -d "assets/animation pics" ]; then
    cp -R "assets/animation pics" "$STAGE_DIR/"
fi
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

for f in i_forgor.js i_forgor.wasm i_forgor.data; do
    [ -e "$BUILD_DIR/bin/$f" ] && cp "$BUILD_DIR/bin/$f" "$DIST_DIR/"
done

[ -e "$BUILD_DIR/bin/i_forgor.html" ] && cp "$BUILD_DIR/bin/i_forgor.html" "$DIST_DIR/index.html"

( cd "$DIST_DIR" && zip -q -r -9 "../$(basename "$DIST_DIR").zip" . )

echo
echo "built $DIST_DIR"
echo "packed ${DIST_DIR}.zip   <- this is the file to upload to itch.io (HTML5)"

if command -v emrun >/dev/null 2>&1; then
    echo "try it with:  emrun $BUILD_DIR/bin/i_forgor.html"
fi
