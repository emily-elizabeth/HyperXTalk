#!/bin/bash
# Assemble the flatpak-sources/ directory that the Flatpak manifest's
# "type: dir" source entry points at.  Run this on your Linux build machine
# after a successful Release build, then run flatpak-builder from the
# packaging/linux/ directory.
#
# Usage:  ./packaging/linux/export-flatpak-sources.sh [BUILD_DIR]
#
# BUILD_DIR  defaults to build-linux-x86_64/hyperxtalk/out/Release

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BUILD_OUT="${1:-$REPO_ROOT/build-linux-x86_64/hyperxtalk/out/Release}"

if [ ! -x "$BUILD_OUT/HyperXTalk" ]; then
    echo "ERROR: $BUILD_OUT/HyperXTalk not found — build first." >&2
    exit 1
fi

DEST="$SCRIPT_DIR/flatpak-sources"
rm -rf "$DEST"
mkdir -p "$DEST/bin" "$DEST/lib" "$DEST/ide" \
         "$DEST/packaged_extensions" "$DEST/modules"

echo "Exporting from $BUILD_OUT ..."

# ── Main binary ───────────────────────────────────────────────────────────────
cp "$BUILD_OUT/HyperXTalk" "$DEST/bin/"
strip --strip-debug "$DEST/bin/HyperXTalk" 2>/dev/null || true

# ── IDE content ───────────────────────────────────────────────────────────────
IDE_DIR="$REPO_ROOT/ide"
for subdir in Toolset Resources Documentation Plugins Externals; do
    if [ -d "$IDE_DIR/$subdir" ]; then
        cp -a "$IDE_DIR/$subdir" "$DEST/ide/"
    fi
done

# ── Shared library externals ──────────────────────────────────────────────────
for lib in libExternal.so revsecurity.so revpdfprinter.so; do
    [ -f "$BUILD_OUT/$lib" ] && cp "$BUILD_OUT/$lib" "$DEST/lib/" || true
done

for so in "$BUILD_OUT"/*.so; do
    [ -f "$so" ] || continue
    name="$(basename "$so")"
    case "$name" in
        server-*) continue ;;
        libExternal.so|revsecurity.so|revpdfprinter.so) continue ;;
    esac
    cp "$so" "$DEST/lib/$name"
    strip --strip-debug "$DEST/lib/$name" 2>/dev/null || true
done

if [ -d "$BUILD_OUT/Externals" ]; then
    cp -a "$BUILD_OUT/Externals/"* "$DEST/lib/" 2>/dev/null || true
fi

# ── Packaged extensions ───────────────────────────────────────────────────────
if [ -d "$BUILD_OUT/packaged_extensions" ]; then
    cp -a "$BUILD_OUT/packaged_extensions/"* "$DEST/packaged_extensions/" 2>/dev/null || true
fi

# ── LCI modules ───────────────────────────────────────────────────────────────
if [ -d "$BUILD_OUT/modules" ]; then
    cp -a "$BUILD_OUT/modules/"* "$DEST/modules/" 2>/dev/null || true
fi

# ── Desktop integration files ─────────────────────────────────────────────────
cp "$SCRIPT_DIR/com.hyperxtalk.HyperXTalk.desktop"      "$DEST/"
cp "$SCRIPT_DIR/com.hyperxtalk.HyperXTalk.metainfo.xml" "$DEST/"
cp "$REPO_ROOT/Installer/application.png" \
   "$DEST/com.hyperxtalk.HyperXTalk.png"

echo ""
echo "flatpak-sources/ ready at: $DEST"
echo "Now run:"
echo "  flatpak-builder --force-clean build-flatpak packaging/linux/com.hyperxtalk.HyperXTalk.yml"
