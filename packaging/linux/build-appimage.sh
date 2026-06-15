#!/bin/bash
# Build an AppImage for HyperXTalk from a completed Linux build.
#
# Usage:  ./packaging/linux/build-appimage.sh [BUILD_DIR] [BUILDTYPE]
#
# BUILD_DIR  defaults to build-linux-x86_64/hyperxtalk
# BUILDTYPE  defaults to Debug
#
# Requires: appimagetool (downloaded automatically if not on PATH)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BUILD_DIR="${1:-$REPO_ROOT/build-linux-x86_64/hyperxtalk}"
BUILDTYPE="${2:-Debug}"
OUT_DIR="$BUILD_DIR/out/$BUILDTYPE"

if [ ! -x "$OUT_DIR/HyperXTalk" ]; then
    echo "ERROR: $OUT_DIR/HyperXTalk not found — build first." >&2
    exit 1
fi

# Read version from the version file (format: KEY = VALUE)
VERSION="$(grep '^BUILD_SHORT_VERSION' "$REPO_ROOT/version" | sed 's/.*= *//')"
VERSION="${VERSION:-0.0.0}"

APPDIR="$BUILD_DIR/HyperXTalk.AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/48x48/apps"

# The HyperXTalk engine resolves its tools path relative to its own binary
# location.  It then looks for Toolset/home.livecodescript (the IDE entry
# point), plus Resources/, Externals/, Documentation/, etc.  We lay out the
# AppDir so that usr/bin/ contains the engine *and* all the IDE content it
# expects to find as siblings.

APPBIN="$APPDIR/usr/bin"
IDE_DIR="$REPO_ROOT/ide"

# --- Main binary ---
cp "$OUT_DIR/HyperXTalk" "$APPBIN/"
strip --strip-debug "$APPBIN/HyperXTalk" 2>/dev/null || true

# --- IDE content (Toolset, Resources, Documentation, Plugins, etc.) ---
for subdir in Toolset Resources Documentation Plugins Externals; do
    if [ -d "$IDE_DIR/$subdir" ]; then
        cp -a "$IDE_DIR/$subdir" "$APPBIN/"
    fi
done

# --- Externals (.so plugins from the build) ---
# Create the expected directory structure for externals and database drivers.
mkdir -p "$APPBIN/Externals/Database Drivers"

# Copy libExternal.so, revsecurity.so, and revpdfprinter.so to the main binary directory.
# They are shared libraries or support libraries rather than standard loadable externals.
for lib in libExternal.so revsecurity.so revpdfprinter.so; do
    if [ -f "$OUT_DIR/$lib" ]; then
        cp "$OUT_DIR/$lib" "$APPBIN/"
        strip --strip-debug "$APPBIN/$lib" 2>/dev/null || true
    fi
done

# Copy standard externals and database drivers to their respective directories inside APPBIN/Externals/
for so in "$OUT_DIR"/*.so; do
    [ -f "$so" ] || continue
    name="$(basename "$so")"
    case "$name" in
        server-*) continue ;;
        libExternal.so|revsecurity.so|revpdfprinter.so) continue ;;
        dbmysql.so|dbodbc.so|dbpostgresql.so|dbsqlite.so)
            cp "$so" "$APPBIN/Externals/Database Drivers/"
            strip --strip-debug "$APPBIN/Externals/Database Drivers/$name" 2>/dev/null || true
            ;;
        *)
            cp "$so" "$APPBIN/Externals/"
            strip --strip-debug "$APPBIN/Externals/$name" 2>/dev/null || true
            ;;
    esac
done

# --- Externals subdirectory from build (CEF etc) ---
if [ -d "$OUT_DIR/Externals" ]; then
    cp -a "$OUT_DIR/Externals/"* "$APPBIN/Externals/" 2>/dev/null || true
fi

# --- Packaged extensions (widgets and libraries) ---
# When packaged/installed, the IDE looks in "Extensions" rather than "packaged_extensions"
if [ -d "$OUT_DIR/packaged_extensions" ]; then
    mkdir -p "$APPBIN/Extensions"
    cp -a "$OUT_DIR/packaged_extensions/"* "$APPBIN/Extensions/" 2>/dev/null || true
fi

# --- LCI modules ---
if [ -d "$OUT_DIR/modules" ]; then
    mkdir -p "$APPBIN/modules"
    cp -a "$OUT_DIR/modules/"* "$APPBIN/modules/" 2>/dev/null || true
fi

# --- Desktop file ---
cat > "$APPDIR/usr/share/applications/HyperXTalk.desktop" <<'DESKTOP'
[Desktop Entry]
Version=1.0
Type=Application
Name=HyperXTalk
Comment=IDE for creating cross-platform applications
Icon=hyperxtalk
Exec=HyperXTalk %U
Categories=Development;IDE;
StartupWMClass=hyperxtalk
DESKTOP

# Symlink desktop file to AppDir root (required by AppImage)
cp "$APPDIR/usr/share/applications/HyperXTalk.desktop" "$APPDIR/HyperXTalk.desktop"

# --- Icon ---
cp "$REPO_ROOT/Installer/application.png" \
   "$APPDIR/usr/share/icons/hicolor/48x48/apps/hyperxtalk.png"
# AppImage also needs an icon at the root
cp "$REPO_ROOT/Installer/application.png" "$APPDIR/hyperxtalk.png"

# --- Bundle libvlc, its support libs, plugins, and all transitive deps ---
#
# VLC has three layers:
#   1. libvlc.so / libvlccore.so — the main shared libraries
#   2. $VLC_DIR/*.so             — support modules (xcb events, pulse, vdpau…)
#   3. $VLC_DIR/plugins/**/*.so  — codec/demux/output plugins loaded at runtime
#
# All of (1) and (2) need to be in LD_LIBRARY_PATH.
# (3) needs VLC_PLUGIN_PATH set so libvlccore can find them.
# Dependencies of all three layers are bundled by the recursive ldd pass below.

# Libraries that must come from the host (core OS ABI).
SKIP_PATTERN="linux-vdso|ld-linux|libpthread|libdl|librt|libc\\.so|libm\\.so\
|libGL\\.so|libEGL\\.so|libGLdispatch|libGLX\
|libX11|libXext|libXfixes|libXrender|libXi|libxcb|libXau|libXdmcp\
|libgcc_s|libstdc++"

LIB_DEST="$APPDIR/usr/lib"
mkdir -p "$LIB_DEST"

# Find VLC's directory (contains plugins/ and support *.so files).
VLC_DIR=""
for candidate in /usr/lib/x86_64-linux-gnu/vlc \
                 /usr/lib/vlc \
                 /usr/local/lib/vlc; do
    [ -d "$candidate/plugins" ] && VLC_DIR="$candidate" && break
done

# Copy libvlc.so.* / libvlccore.so.* and symlinks.
for pattern in libvlc.so* libvlccore.so*; do
    for f in /usr/lib/x86_64-linux-gnu/$pattern \
             /usr/lib/$pattern \
             /usr/local/lib/$pattern; do
        [ -e "$f" ] || continue
        cp -P "$f" "$LIB_DEST/" 2>/dev/null || true
    done
done

if [ -n "$VLC_DIR" ]; then
    # Copy VLC support libs (libvlc_pulse.so, libvlc_xcb_events.so, etc.)
    # They live in $VLC_DIR alongside the plugins/ subdirectory and are
    # loaded by libvlccore; they must be on LD_LIBRARY_PATH.
    for f in "$VLC_DIR"/*.so "$VLC_DIR"/*.so.*; do
        [ -f "$f" ] || continue
        cp -P "$f" "$LIB_DEST/" 2>/dev/null || true
    done

    # Copy the full plugin tree.
    mkdir -p "$APPDIR/usr/lib/vlc"
    cp -a "$VLC_DIR/plugins" "$APPDIR/usr/lib/vlc/"
    echo "Bundled VLC plugins from $VLC_DIR/plugins"
else
    echo "WARNING: VLC plugin directory not found — video playback may not work." >&2
fi

# Recursively bundle all shared-library dependencies.
# We keep a worklist and process it until no new libraries are added.
bundle_libs_recursive() {
    local worklist=("$@")
    local changed=1

    while [ "$changed" -eq 1 ]; do
        changed=0
        local next_worklist=()
        for target in "${worklist[@]}"; do
            [ -f "$target" ] || continue
            while IFS= read -r lib; do
                name="$(basename "$lib")"
                echo "$lib" | grep -qE "$SKIP_PATTERN" && continue
                dest="$LIB_DEST/$name"
                if [ ! -e "$dest" ]; then
                    cp -P "$lib" "$LIB_DEST/" 2>/dev/null || true
                    # Resolve symlink to the real file for ldd.
                    real="$(readlink -f "$lib" 2>/dev/null || echo "$lib")"
                    next_worklist+=("$real")
                    changed=1
                fi
            done < <(ldd "$target" 2>/dev/null | awk '{print $3}' | grep "^/")
        done
        worklist=("${next_worklist[@]}")
    done
}

# Seed with the main binary and the top-level VLC libs only.
# Plugin .so files are NOT included — their deps are already captured by
# libvlccore.so, and ldd-ing hundreds of plugin files makes the build very slow.
seed=("$APPBIN/HyperXTalk")
for f in "$LIB_DEST"/*.so "$LIB_DEST"/*.so.*; do
    [ -f "$f" ] || continue
    real="$(readlink -f "$f" 2>/dev/null || echo "$f")"
    [ -f "$real" ] && seed+=("$real")
done
# Deduplicate seed.
mapfile -t seed < <(printf '%s\n' "${seed[@]}" | sort -u)
bundle_libs_recursive "${seed[@]}"

# --- AppRun ---
cat > "$APPDIR/AppRun" <<'APPRUN'
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib:$HERE/usr/bin${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# Point libvlccore at the bundled plugin tree.
export VLC_PLUGIN_PATH="$HERE/usr/lib/vlc/plugins"
exec "$HERE/usr/bin/HyperXTalk" "$@"
APPRUN
chmod +x "$APPDIR/AppRun"

# --- Obtain appimagetool ---
ARCH="$(uname -m)"
APPIMAGETOOL=""

if command -v appimagetool >/dev/null 2>&1; then
    APPIMAGETOOL="appimagetool"
else
    TOOL_PATH="$BUILD_DIR/appimagetool"
    if [ ! -x "$TOOL_PATH" ]; then
        echo "Downloading appimagetool..."
        curl -fsSL -o "$TOOL_PATH" \
            "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${ARCH}.AppImage"
        chmod +x "$TOOL_PATH"
    fi
    APPIMAGETOOL="$TOOL_PATH"
fi

# --- Build AppImage ---
OUTPUT="$BUILD_DIR/HyperXTalk-${VERSION}-${ARCH}.AppImage"
ARCH="$ARCH" "$APPIMAGETOOL" "$APPDIR" "$OUTPUT"

echo ""
echo "AppImage created: $OUTPUT"
echo "Size: $(du -h "$OUTPUT" | cut -f1)"
