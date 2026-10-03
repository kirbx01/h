#!/usr/bin/env bash
#
# Linux build. Produces a single folder and a tarball in dist/ that runs on any x86-64
# machine with a GL 3.3 driver and the usual desktop libraries.
#
#   scripts/build_linux.sh
#
# Static libgcc/libstdc++ are linked so the folder does not depend on the gcc runtime
# being installed; everything else (X11, GL) is expected from the host.

set -euo pipefail

cd "$(dirname "$0")/.."

BUILD_DIR="${BUILD_DIR:-build-linux}"
DIST_DIR="${DIST_DIR:-dist/i_forgor-linux-x86_64}"
NAME="i_forgor"

[ -e external/raylib/CMakeLists.txt ] || scripts/fetch_deps.sh

cmake -S . -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE=Release \
      -DIFG_BUILD_TESTS=ON \
      -DIFG_BUILD_CAPTURE=OFF \
      ${IFG_ITCH_URL:+-DIFG_ITCH_URL="$IFG_ITCH_URL"}

cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 4)"

ctest --test-dir "$BUILD_DIR" --output-on-failure

rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

install -m 755 "$BUILD_DIR/bin/$NAME" "$DIST_DIR/$NAME"

# The game looks for its music in assets/ next to the executable, so the folder has to
# carry it. Only what is actually present is copied.
if [ -d assets ]; then
    mkdir -p "$DIST_DIR/assets"
    for f in assets/*.ogg assets/*.wav assets/*.mp3 assets/*.qoa assets/*.xm assets/*.mod \
             assets/itch_url.txt; do
        [ -e "$f" ] && install -m 644 "$f" "$DIST_DIR/assets/"
    done
    rmdir "$DIST_DIR/assets" 2>/dev/null || true
fi

cp README.md "$DIST_DIR/" 2>/dev/null || true

tar -czf "${DIST_DIR}.tar.gz" -C "$(dirname "$DIST_DIR")" "$(basename "$DIST_DIR")"

echo
echo "built ${DIST_DIR}"
echo "packed ${DIST_DIR}.tar.gz"
du -h "${DIST_DIR}.tar.gz"
