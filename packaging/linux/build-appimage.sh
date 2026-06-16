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

# --- edition.txt — marks this as an installed (non-dev) build ---
# revEnvironmentIsInstalled() in home.livecodescript checks for the presence of
# Toolset/edition.txt to distinguish installed builds from git-repo dev builds.
# Without it the IDE falls into dev mode: docs use repo-relative paths that
# don't exist in the AppImage, revdocsparser is never loaded, and documentation
# and the standalone settings dialog both fail silently.
# The installer emits the edition name (e.g. "community") here; we use the
# same value so installed-mode code paths activate correctly.
echo "community" > "$APPBIN/Toolset/edition.txt"

# --- ide-support files → Toolset/libraries ---
# The installer places these files into Toolset/libraries/ for installed builds.
# Without them, revidelibrary fails to initialize (revdocsparser not found →
# EE_DISPATCH_BADTARGET), documentation can't display, and the standalone
# settings dialog never opens (revsblibrary/revsaveasstandalone missing).
IDE_SUPPORT_DIR="$REPO_ROOT/ide-support"
if [ -d "$IDE_SUPPORT_DIR" ]; then
    mkdir -p "$APPBIN/Toolset/libraries"
    for f in "$IDE_SUPPORT_DIR"/*.livecodescript; do
        [ -f "$f" ] || continue
        cp "$f" "$APPBIN/Toolset/libraries/"
    done
fi

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

# --- revbrowser + CEF (required for the Dictionary documentation widget) ---
# The browser widget (com.livecode.widget.browser) uses libbrowser which wraps
# CEF.  Three things must be in place for it to work:
#
#   1. revbrowser.so        — the classic LiveCode browser external
#   2. libbrowser-cefprocess — the CEF renderer subprocess binary; must sit
#                              alongside the main HyperXTalk executable because
#                              __MCCefPlatformGetExecutableFolder() (reads
#                              /proc/self/exe) is used for browser_subprocess_path
#   3. Externals/CEF/       — libcef.so, .pak files, locales, libEGL/libGLESv2
#
# $LIVECODE_USE_CEF is an OS environment variable read by revidelibrary; if it is
# not set the IDE assumes the browser widget is available on Linux and tries to use
# CEF.  The AppRun wrapper exports it based on whether libbrowser-cefprocess is
# present so that revIDEBrowserWidgetUnavailable() returns the correct value.
PREBUILT_BIN="$REPO_ROOT/linux-x86_64-bin"
if [ -f "$PREBUILT_BIN/revbrowser.so" ]; then
    cp "$PREBUILT_BIN/revbrowser.so" "$APPBIN/"
    echo "Bundled revbrowser.so from linux-x86_64-bin"
else
    echo "WARNING: linux-x86_64-bin/revbrowser.so not found — classic browser external missing." >&2
fi
if [ -f "$PREBUILT_BIN/libbrowser-cefprocess" ]; then
    cp "$PREBUILT_BIN/libbrowser-cefprocess" "$APPBIN/"
    echo "Bundled libbrowser-cefprocess from linux-x86_64-bin"
else
    echo "WARNING: linux-x86_64-bin/libbrowser-cefprocess not found — CEF will not work." >&2
fi
if [ -d "$PREBUILT_BIN/Externals/CEF" ]; then
    mkdir -p "$APPBIN/Externals/CEF"
    cp -a "$PREBUILT_BIN/Externals/CEF/"* "$APPBIN/Externals/CEF/"
    echo "Bundled CEF from linux-x86_64-bin/Externals/CEF"
else
    echo "WARNING: linux-x86_64-bin/Externals/CEF not found — CEF will not work." >&2
fi

# --- Packaged extensions (widgets and libraries) ---
# When packaged/installed, the IDE looks in "Extensions" rather than "packaged_extensions"
mkdir -p "$APPBIN/Extensions"
if [ -d "$OUT_DIR/packaged_extensions" ]; then
    cp -a "$OUT_DIR/packaged_extensions/"* "$APPBIN/Extensions/" 2>/dev/null || true
fi
# The browser widget module must come from the prebuilt binaries since the build
# output packaged_extensions/ may not contain it.  It is the LCB module that
# implements com.livecode.widget.browser and is required for the Dictionary
# palette's embedded browser widget.
if [ -d "$PREBUILT_BIN/packaged_extensions/com.livecode.widget.browser" ]; then
    cp -a "$PREBUILT_BIN/packaged_extensions/com.livecode.widget.browser" \
          "$APPBIN/Extensions/"
    echo "Bundled com.livecode.widget.browser from linux-x86_64-bin"
else
    echo "WARNING: linux-x86_64-bin/packaged_extensions/com.livecode.widget.browser not found — Dictionary widget may not load." >&2
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

    # Copy the full plugin tree to the path the engine probes at startup:
    #   <exe_dir>/vlc-plugins/plugins
    # (see vlc-player.cpp Linux init block).  The engine sets VLC_PLUGIN_PATH
    # to this path when it exists, so libvlccore finds codecs without relying
    # on the environment or system paths.
    mkdir -p "$APPBIN/vlc-plugins"
    cp -a "$VLC_DIR/plugins" "$APPBIN/vlc-plugins/"
    # Remove the plugin cache — it contains absolute paths from the build
    # machine that won't match the AppImage mount point.  VLC rescans
    # using VLC_PLUGIN_PATH at first launch instead.
    rm -f "$APPBIN/vlc-plugins/plugins/plugins.dat"
    echo "Bundled VLC plugins from $VLC_DIR/plugins -> usr/bin/vlc-plugins/plugins"
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
                    # Also copy the actual versioned file if the lib is a symlink.
                    # cp -P copies the symlink but not its target, leaving a broken
                    # symlink in LIB_DEST when the real file lives elsewhere.
                    real_name="$(basename "$real")"
                    if [ "$real_name" != "$name" ] && [ ! -e "$LIB_DEST/$real_name" ] && [ -f "$real" ]; then
                        cp "$real" "$LIB_DEST/" 2>/dev/null || true
                    fi
                    next_worklist+=("$real")
                    changed=1
                fi
            done < <(ldd "$target" 2>/dev/null | awk '{print $3}' | grep "^/")
        done
        worklist=("${next_worklist[@]}")
    done
}

# Seed bundle_libs_recursive with the main binary and top-level VLC libs.
# We do NOT seed with plugin .so files — ldd-ing them can stall (some plugins
# try to open a display connection when loaded by the dynamic linker).
#
# VLC codec/demux plugins dlopen FFmpeg libs at runtime via libavcodec etc.
# Those are not captured by libvlccore's own ldd, so we copy them explicitly
# below rather than discovering them recursively.
seed=("$APPBIN/HyperXTalk")
for f in "$LIB_DEST"/*.so "$LIB_DEST"/*.so.*; do
    [ -f "$f" ] || continue
    real="$(readlink -f "$f" 2>/dev/null || echo "$f")"
    [ -f "$real" ] && seed+=("$real")
done
# Deduplicate seed.
mapfile -t seed < <(printf '%s\n' "${seed[@]}" | sort -u)
bundle_libs_recursive "${seed[@]}"

# --- Explicitly bundle FFmpeg libs (deps of VLC codec/demux plugins) ---
# These are dlopen'd at runtime by the codec plugins and are not captured by
# ldd on libvlccore.so.  Copy every versioned soname we find; the recursive
# bundler already handles their own transitive deps via the seed above.
echo "Bundling FFmpeg libs..."
for pattern in \
    libavcodec.so* libavformat.so* libavutil.so* \
    libswscale.so* libswresample.so* libpostproc.so* \
    libavfilter.so*; do
    for search_dir in /usr/lib/x86_64-linux-gnu /usr/lib /usr/local/lib; do
        for f in "$search_dir"/$pattern; do
            [ -e "$f" ] || continue
            name="$(basename "$f")"
            echo "$f" | grep -qE "$SKIP_PATTERN" && continue
            [ -e "$LIB_DEST/$name" ] && continue
            cp -P "$f" "$LIB_DEST/" 2>/dev/null || true
            # Also copy the real file if f is a symlink (same broken-symlink fix).
            real="$(readlink -f "$f" 2>/dev/null || echo "$f")"
            real_name="$(basename "$real")"
            if [ "$real_name" != "$name" ] && [ ! -e "$LIB_DEST/$real_name" ] && [ -f "$real" ]; then
                cp "$real" "$LIB_DEST/" 2>/dev/null || true
            fi
            echo "  bundled $name"
        done
    done
done
# Run one more recursive pass to pick up FFmpeg's own deps (e.g. libx264, libx265).
ffmpeg_seed=()
for f in "$LIB_DEST"/libav*.so.* "$LIB_DEST"/libsw*.so.* "$LIB_DEST"/libpost*.so.*; do
    [ -f "$f" ] || continue
    real="$(readlink -f "$f" 2>/dev/null || echo "$f")"
    [ -f "$real" ] && ffmpeg_seed+=("$real")
done
if [ "${#ffmpeg_seed[@]}" -gt 0 ]; then
    mapfile -t ffmpeg_seed < <(printf '%s\n' "${ffmpeg_seed[@]}" | sort -u)
    bundle_libs_recursive "${ffmpeg_seed[@]}"
fi

# --- Bundle libcef.so transitive deps ---
# libcef.so is loaded at runtime by libbrowser (CEF-based browser widget) via
# dlopen.  Its deps are not captured by the main binary's ldd pass above, so we
# run a dedicated pass now that libcef.so has been copied to Externals/CEF/.
libcef_real=""
if [ -f "$APPBIN/Externals/CEF/libcef.so" ]; then
    libcef_real="$(readlink -f "$APPBIN/Externals/CEF/libcef.so" 2>/dev/null || echo "$APPBIN/Externals/CEF/libcef.so")"
fi
if [ -n "$libcef_real" ] && [ -f "$libcef_real" ]; then
    echo "Bundling libcef.so transitive deps..."
    bundle_libs_recursive "$libcef_real"
fi

# --- AppRun ---
cat > "$APPDIR/AppRun" <<'APPRUN'
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"
# usr/lib:      bundled shared-library deps (VLC, codec libs, etc.)
# usr/bin:      main binary siblings (revsecurity.so, revpdfprinter.so, etc.)
# Externals/CEF: libEGL.so and libGLESv2.so required by libcef at runtime
export LD_LIBRARY_PATH="$HERE/usr/lib:$HERE/usr/bin:$HERE/usr/bin/Externals/CEF${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# Fallback in case the engine's own VLC probe doesn't run first.
export VLC_PLUGIN_PATH="$HERE/usr/bin/vlc-plugins/plugins"
# $LIVECODE_USE_CEF is read by revIDEBrowserWidgetUnavailable() in revidelibrary.
# If the CEF subprocess binary is present, set it to 1 so the IDE attempts to use
# the built-in browser widget; otherwise set to 0 to force the system-browser
# fallback, which requires only that api.html has been pre-generated at startup.
if [ -x "$HERE/usr/bin/libbrowser-cefprocess" ] && \
   [ -f "$HERE/usr/bin/Externals/CEF/libcef.so" ]; then
    export LIVECODE_USE_CEF=1
else
    export LIVECODE_USE_CEF=0
fi
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
# Use gzip compression (--comp gzip) — significantly faster than the default
# xz at a modest size cost.  Switch back to xz for release builds if size matters.
OUTPUT="$BUILD_DIR/HyperXTalk-${VERSION}-${ARCH}.AppImage"
ARCH="$ARCH" "$APPIMAGETOOL" --comp zstd "$APPDIR" "$OUTPUT"

echo ""
echo "AppImage created: $OUTPUT"
echo "Size: $(du -h "$OUTPUT" | cut -f1)"
