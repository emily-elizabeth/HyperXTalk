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
# revEnvironmentIsInstalled() checks for Toolset/edition.txt; without it the
# IDE runs in dev mode and can't find revdocsparser, leaving built_api.js empty.
echo "community" > "$APPBIN/Toolset/edition.txt"

# --- ide-support files → Toolset/libraries ---
# Installed-mode code paths expect revdocsparser.livecodescript and
# revsblibrary.livecodescript here.  revdocsparser is required to generate
# built_api.js / built_guide.js; revsblibrary is needed for the standalone
# settings dialog.
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

# --- Runtime/Linux/x86-64 — standalone builder engine and support files ---
# revEnvironmentRuntimePath() = sToolsPath & "/Runtime", where sToolsPath is
# item 1 to -3 of the home-stack path — i.e. usr/bin (parent of Toolset/).
# So the Runtime tree must live at usr/bin/Runtime/, NOT usr/bin/Toolset/Runtime/.
# revEngineCheck("Linux x64") checks for Runtime/Linux/x86-64/Standalone;
# without it the "Linux x64" checkbox is hidden and the builder refuses to run.
RUNTIME_LNX="$APPBIN/Runtime/Linux/x86-64"
mkdir -p "$RUNTIME_LNX/Support" "$RUNTIME_LNX/Externals/Database Drivers"

# Engine binary — must be the standalone-mode binary (compiled with
# mode_standalone.cpp), which contains the .project ELF section the deploy
# engine requires to embed the user's project.  The IDE binary (HyperXTalk)
# is compiled in a different mode and lacks this section entirely.
cp "$OUT_DIR/standalone-community" "$RUNTIME_LNX/Standalone"
strip --strip-debug "$RUNTIME_LNX/Standalone" 2>/dev/null || true
# NOTE: No patchelf call here.  The $ORIGIN/lib RPATH is baked into the binary
# at link time via -Wl,-rpath,$ORIGIN/lib in engine/engine.gyp.  Using patchelf
# --add-rpath after the fact adds a new PT_LOAD segment after the .project section,
# which the deploy engine (deploy_linux.cpp) rejects with "bad section order".

# Support libraries (revsecurity, revpdfprinter, libExternal)
for lib in revsecurity.so revpdfprinter.so libExternal.so; do
    [ -f "$OUT_DIR/$lib" ] && cp "$OUT_DIR/$lib" "$RUNTIME_LNX/Support/"
done

# Externals — same split logic as the APPBIN/Externals section above
for so in "$OUT_DIR"/*.so; do
    [ -f "$so" ] || continue
    name="$(basename "$so")"
    case "$name" in
        server-*) continue ;;
        libExternal.so|revsecurity.so|revpdfprinter.so) continue ;;
        dbmysql.so|dbodbc.so|dbpostgresql.so|dbsqlite.so)
            cp "$so" "$RUNTIME_LNX/Externals/Database Drivers/"
            ;;
        *)
            cp "$so" "$RUNTIME_LNX/Externals/"
            ;;
    esac
done

# Sub-externals from the build output (CEF, etc.)
if [ -d "$OUT_DIR/Externals" ]; then
    cp -a "$OUT_DIR/Externals/"* "$RUNTIME_LNX/Externals/" 2>/dev/null || true
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
# Install the hi-res source icon at all standard hicolor sizes so desktop
# environments (GNOME, KDE, XFCE) pick the best resolution for each context.
ICON_SRC="$REPO_ROOT/Installer/application-1024.png"
for size in 16 32 48 64 128 256 512; do
    mkdir -p "$APPDIR/usr/share/icons/hicolor/${size}x${size}/apps"
    convert "$ICON_SRC" -resize "${size}x${size}" \
        "$APPDIR/usr/share/icons/hicolor/${size}x${size}/apps/hyperxtalk.png"
done
# AppImage root icon (used by file managers and appimagetool for embedding).
# 512px is the conventional sweet spot — large enough for HiDPI, not bloated.
convert "$ICON_SRC" -resize 512x512 "$APPDIR/hyperxtalk.png"

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
    echo "Bundled VLC plugins from $VLC_DIR/plugins -> usr/bin/vlc-plugins/plugins"

    # Remove plugins.dat — it contains absolute paths from the build system that
    # won't match the AppImage mount point (/tmp/.mount_XXXXXX/...).  Without
    # deleting it VLC uses the stale cache and fails to find codecs at runtime.
    rm -f "$APPBIN/vlc-plugins/plugins/plugins.dat"

    # Patch RPATH on every VLC plugin .so so it can find bundled FFmpeg/VLC libs
    # in <standalone_dir>/lib/ at runtime.  The system RPATH baked into these
    # files points to build-system paths (/usr/lib/x86_64-linux-gnu) that won't
    # exist on end-user machines.
    #
    # VLC plugins are typically one directory deep inside plugins/ (e.g.
    # plugins/codec/libavcodec_plugin.so).  From that location:
    #   $ORIGIN/../../../lib  →  <standalone_dir>/lib/     (depth-1 subdir)
    #   $ORIGIN/../../lib     →  <standalone_dir>/lib/     (flat, depth-0)
    # Both entries are set so either layout is covered.
    #
    # In the AppImage itself, LD_LIBRARY_PATH covers usr/lib/, so the patched
    # $ORIGIN-relative paths simply don't match (the directory doesn't exist at
    # that relative position) and the linker falls through to LD_LIBRARY_PATH.
    echo "Patching RPATH on VLC plugin .so files..."
    find "$APPBIN/vlc-plugins/plugins" -type f -name "*.so" | while read -r plugin; do
        patchelf --set-rpath '$ORIGIN/../../../lib:$ORIGIN/../../lib' "$plugin" 2>/dev/null || true
    done
    echo "VLC plugin RPATH patched."
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
                    # cp -P copies the symlink but not its target; copy the
                    # real versioned file too so the symlink isn't broken.
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

# --- Explicitly bundle FFmpeg libs ---
# VLC codec/demux plugins dlopen libavcodec, libavformat, libavutil etc. at
# runtime.  These are not captured by ldd on libvlccore.so alone, so we copy
# them explicitly and then run another recursive pass for their own deps.
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
            real="$(readlink -f "$f" 2>/dev/null || echo "$f")"
            real_name="$(basename "$real")"
            if [ "$real_name" != "$name" ] && [ ! -e "$LIB_DEST/$real_name" ] && [ -f "$real" ]; then
                cp "$real" "$LIB_DEST/" 2>/dev/null || true
            fi
            echo "  bundled $name"
        done
    done
done
# Recursive pass for FFmpeg's own deps (libx264, libx265, etc.)
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

# --- Bundle WebKit2GTK into Externals/ ---
#
# The engine loads webkit via LoadBundled(exedir, name) which resolves to
# $exedir/Externals/<name>.  In the AppImage, $exedir is usr/bin/, so all
# WebKit libs and their transitive deps go in usr/bin/Externals/.
#
# After loading the bundled WebKit, C++ code (libbrowser_webkitgtk.cpp):
#   - Pre-loads GLib/GTK from Externals/ with RTLD_DEEPBIND (avoids symbol
#     conflicts with the engine's own GLib/GTK linkage)
#   - Sets GIO_MODULE_DIR to $exedir/Externals/gio/modules
#   - Prepends $exedir/Externals to LD_LIBRARY_PATH (inherited by WebKit
#     subprocess helpers so they can find bundled libs)
#   - Sets WEBKIT_SUBPROCESS_PATH to $exedir/Externals/webkit-subprocess
#
# Skip pattern for WebKit deps: exclude only kernel ABI, OpenGL, and X11.
# We KEEP GLib/GTK so the RTLD_DEEPBIND pre-load works properly.
WK_SKIP="linux-vdso|ld-linux|libpthread|libdl|librt|libc\\.so|libm\\.so\
|libGL\\.so|libEGL\\.so|libGLdispatch|libGLX\
|libX11|libXext|libXfixes|libXrender|libXi|libxcb|libXau|libXdmcp\
|libgcc_s|libstdc++"

WEBKIT_EXTERNALS="$APPBIN/Externals"
WEBKIT_SUBPROCESS_DIR="$WEBKIT_EXTERNALS/webkit-subprocess"
WEBKIT_GIO_MODULES="$WEBKIT_EXTERNALS/gio/modules"
mkdir -p "$WEBKIT_SUBPROCESS_DIR" "$WEBKIT_GIO_MODULES"

# Locate the WebKit and JSC shared libraries on the build machine.
WK_LIB=""
JSC_LIB=""
for candidate in \
    /usr/lib/x86_64-linux-gnu/libwebkit2gtk-4.1.so.0 \
    /usr/lib/x86_64-linux-gnu/libwebkit2gtk-4.0.so.37 \
    /usr/lib/libwebkit2gtk-4.1.so.0 \
    /usr/lib/libwebkit2gtk-4.0.so.37; do
    [ -e "$candidate" ] || continue
    WK_LIB="$candidate"; break
done
for candidate in \
    /usr/lib/x86_64-linux-gnu/libjavascriptcoregtk-4.1.so.0 \
    /usr/lib/x86_64-linux-gnu/libjavascriptcoregtk-4.0.so.18 \
    /usr/lib/libjavascriptcoregtk-4.1.so.0 \
    /usr/lib/libjavascriptcoregtk-4.0.so.18; do
    [ -e "$candidate" ] || continue
    JSC_LIB="$candidate"; break
done

if [ -z "$WK_LIB" ]; then
    echo "WARNING: webkit2gtk not found on build machine — browser widget disabled in AppImage." >&2
else
    echo "Bundling WebKit2GTK from $WK_LIB ..."

    # Determine the versioned subprocess helper directory.
    if echo "$WK_LIB" | grep -q "4\.1"; then
        WK_SUBPROC_SYS="/usr/lib/x86_64-linux-gnu/webkit2gtk-4.1"
    else
        WK_SUBPROC_SYS="/usr/lib/x86_64-linux-gnu/webkit2gtk-4.0"
    fi
    [ -d "$WK_SUBPROC_SYS" ] || WK_SUBPROC_SYS=""

    # Copy the main WebKit and JSC shared libraries (real file + any symlink).
    _copy_lib_with_real() {
        local f="$1" dest="$2"
        [ -e "$f" ] || return 0
        cp -P "$f" "$dest/" 2>/dev/null || true
        local real; real="$(readlink -f "$f" 2>/dev/null || echo "$f")"
        local rname; rname="$(basename "$real")"
        local fname; fname="$(basename "$f")"
        if [ "$rname" != "$fname" ] && [ -f "$real" ] && [ ! -e "$dest/$rname" ]; then
            cp "$real" "$dest/" 2>/dev/null || true
        fi
        echo "  bundled $(basename "$f")"
    }
    _copy_lib_with_real "$WK_LIB"  "$WEBKIT_EXTERNALS"
    _copy_lib_with_real "$JSC_LIB" "$WEBKIT_EXTERNALS"

    # Binary-patch PKGLIBEXECDIR in the bundled libwebkit2gtk .so.
    #
    # Ubuntu's release builds of WebKit do NOT honour any WEBKIT_EXEC_PATH /
    # WEBKIT_SUBPROCESS_PATH env var — that code path is behind
    # #if ENABLE(DEVELOPER_MODE) and is stripped in distribution packages.
    # The only way to redirect WebKit's subprocess helper lookup is to patch
    # the hardcoded PKGLIBEXECDIR string in the .so itself.
    #
    # We replace: /usr/lib/x86_64-linux-gnu/webkit2gtk-4.1\0  (41 bytes)
    # with:       /tmp/.hxt-wk/webkit2gtk-4.1\0 + NUL padding (41 bytes)
    #
    # AppRun creates symlinks at /tmp/.hxt-wk/webkit2gtk-4.1/ pointing to
    # the bundled helpers in Externals/webkit-subprocess/ at launch time.
    echo "  Patching PKGLIBEXECDIR in bundled libwebkit2gtk..."
    python3 - "$WEBKIT_EXTERNALS" <<'PYEOF'
import sys, os, glob

dest = sys.argv[1]
old = b'/usr/lib/x86_64-linux-gnu/webkit2gtk-4.1\x00'
new = b'/tmp/.hxt-wk/webkit2gtk-4.1\x00'
assert len(new) <= len(old), f"replacement string too long ({len(new)} > {len(old)})"
new_padded = new + b'\x00' * (len(old) - len(new))

patched = 0
for pattern in ('libwebkit2gtk-4.1.so*', 'libwebkit2gtk-4.0.so*'):
    for path in sorted(glob.glob(os.path.join(dest, pattern))):
        if os.path.islink(path):
            continue  # patch real files only, not symlinks
        with open(path, 'rb') as f:
            data = f.read()
        n = data.count(old)
        if n:
            data = data.replace(old, new_padded)
            with open(path, 'wb') as f:
                f.write(data)
            print(f'    patched {n} PKGLIBEXECDIR ref(s) in {os.path.basename(path)}')
            patched += 1
if not patched:
    print('    WARNING: PKGLIBEXECDIR string not found — subprocess helpers may not load', file=sys.stderr)
PYEOF

    # Copy WebKit subprocess helpers (WebKitWebProcess, WebKitNetworkProcess).
    # They are ELF executables that WebKit spawns as child processes; they need
    # WEBKIT_SUBPROCESS_PATH set so WebKit can find them.
    if [ -n "$WK_SUBPROC_SYS" ]; then
        for helper in WebKitWebProcess WebKitNetworkProcess; do
            if [ -f "$WK_SUBPROC_SYS/$helper" ]; then
                cp "$WK_SUBPROC_SYS/$helper" "$WEBKIT_SUBPROCESS_DIR/"
                echo "  bundled subprocess helper: $helper"
            fi
        done
    fi

    # Copy GIO modules (needed for TLS/GVFS support inside the AppImage).
    for gio_dir in \
        /usr/lib/x86_64-linux-gnu/gio/modules \
        /usr/lib/gio/modules; do
        [ -d "$gio_dir" ] || continue
        cp "$gio_dir"/*.so "$WEBKIT_GIO_MODULES/" 2>/dev/null || true
        echo "  bundled GIO modules from $gio_dir"
        break
    done

    # Recursive dep bundler — copies into an arbitrary destination directory.
    bundle_to_dir() {
        local dest="$1"; shift
        local worklist=("$@")
        local changed=1
        while [ "$changed" -eq 1 ]; do
            changed=0
            local next_worklist=()
            for target in "${worklist[@]}"; do
                [ -f "$target" ] || continue
                while IFS= read -r lib; do
                    local name; name="$(basename "$lib")"
                    echo "$lib" | grep -qE "$WK_SKIP" && continue
                    if [ ! -e "$dest/$name" ]; then
                        cp -P "$lib" "$dest/" 2>/dev/null || true
                        local real; real="$(readlink -f "$lib" 2>/dev/null || echo "$lib")"
                        local rname; rname="$(basename "$real")"
                        if [ "$rname" != "$name" ] && [ -f "$real" ] && [ ! -e "$dest/$rname" ]; then
                            cp "$real" "$dest/" 2>/dev/null || true
                        fi
                        next_worklist+=("$real")
                        changed=1
                    fi
                done < <(ldd "$target" 2>/dev/null | awk '{print $3}' | grep "^/")
            done
            worklist=("${next_worklist[@]}")
        done
    }

    # Seed the recursive pass with WebKit, JSC, subprocess helpers, and GIO modules.
    wk_seed=()
    for f in "$WEBKIT_EXTERNALS"/*.so "$WEBKIT_EXTERNALS"/*.so.*; do
        [ -f "$f" ] || continue
        real="$(readlink -f "$f" 2>/dev/null || echo "$f")"
        [ -f "$real" ] && wk_seed+=("$real")
    done
    for f in "$WEBKIT_SUBPROCESS_DIR"/*; do
        [ -f "$f" ] && wk_seed+=("$f")
    done
    for f in "$WEBKIT_GIO_MODULES"/*.so; do
        [ -f "$f" ] && wk_seed+=("$f")
    done
    if [ "${#wk_seed[@]}" -gt 0 ]; then
        mapfile -t wk_seed < <(printf '%s\n' "${wk_seed[@]}" | sort -u)
        echo "  Resolving transitive deps for WebKit (${#wk_seed[@]} seed files)..."
        bundle_to_dir "$WEBKIT_EXTERNALS" "${wk_seed[@]}"
    fi

    # Patch RPATH on all real libs in Externals/ so they find sibling deps.
    echo "  Patching RPATH on Externals/ libs..."
    find "$WEBKIT_EXTERNALS" -maxdepth 1 -type f -name "*.so*" | while read -r lib; do
        patchelf --set-rpath '$ORIGIN' "$lib" 2>/dev/null || true
    done
    # GIO module helpers: one level deeper, so RPATH points up to Externals/.
    find "$WEBKIT_GIO_MODULES" -type f -name "*.so" | while read -r lib; do
        patchelf --set-rpath '$ORIGIN/..' "$lib" 2>/dev/null || true
    done
    # Subprocess helpers: also one level deeper.
    for f in "$WEBKIT_SUBPROCESS_DIR"/*; do
        [ -f "$f" ] || continue
        patchelf --set-rpath '$ORIGIN/..' "$f" 2>/dev/null || true
    done

    echo "WebKit2GTK bundled ($(find "$WEBKIT_EXTERNALS" -maxdepth 1 -name "*.so*" | wc -l) libs in Externals/)"
fi

# --- Patch RPATH on every bundled lib so transitive deps resolve ---
# Each bundled .so in LIB_DEST may have its original DT_RUNPATH pointing at the
# build-system's /usr/lib/... paths.  DT_RUNPATH is NOT inherited, so libvlc.so.5
# cannot find libvlccore.so.9 via the standalone binary's $ORIGIN/lib RPATH.
# Fix: set DT_RUNPATH to $ORIGIN on every real (non-symlink) .so file so each
# lib finds its siblings in the same directory.
echo "Patching RPATH on bundled libs..."
find "$LIB_DEST" -maxdepth 1 -type f -name "*.so*" | while read -r lib; do
    patchelf --set-rpath '$ORIGIN' "$lib" 2>/dev/null || true
done
echo "RPATH patched on bundled libs."

# --- Runtime/Linux/x86-64/vlc — pre-staged VLC tree for the standalone builder ---
# revSBCopyVLCToStandalone looks for Runtime/Linux/x86-64/vlc/{lib,plugins} when
# building a Linux standalone from an AppImage, so it doesn't rely on the host
# having VLC installed.  We populate it from the libs already bundled above
# (which now have $ORIGIN RPATH so they find each other in the standalone's lib/).
VLC_RUNTIME_VLC="$APPBIN/Runtime/Linux/x86-64/vlc"
mkdir -p "$VLC_RUNTIME_VLC/lib" "$VLC_RUNTIME_VLC/plugins"

# Copy every lib from LIB_DEST — these are all the VLC, FFmpeg, and transitive
# deps that the standalone needs alongside its binary.
cp -a "$LIB_DEST/." "$VLC_RUNTIME_VLC/lib/" 2>/dev/null || true

# Copy the VLC plugin tree (plugins.dat is already absent — deleted above).
if [ -d "$APPBIN/vlc-plugins/plugins" ]; then
    cp -a "$APPBIN/vlc-plugins/plugins/." "$VLC_RUNTIME_VLC/plugins/"
fi
echo "Staged VLC runtime for standalone builder -> Runtime/Linux/x86-64/vlc/"

# --- Bundle patchelf for the standalone builder ---
# The standalone builder calls patchelf on the deployed output binary to bake
# $ORIGIN/lib into its RPATH so bundled VLC libs in lib/ are found at runtime.
# patchelf is called on the OUTPUT binary (post-deploy), NOT on the Standalone
# engine template — so "bad section order" does not apply here.
# patchelf must be installed on this build machine; it is bundled into the AppImage
# so the end-user/test machine does not need it separately.
#   sudo apt install patchelf      (Ubuntu/Debian)
#   sudo dnf install patchelf      (Fedora/RHEL)
if ! command -v patchelf >/dev/null 2>&1; then
    echo "ERROR: patchelf is required to build the AppImage." >&2
    echo "       Install it with:  sudo apt install patchelf" >&2
    exit 1
fi
PATCHELF_DEST="$APPBIN/patchelf"
cp "$(command -v patchelf)" "$PATCHELF_DEST"
chmod +x "$PATCHELF_DEST"
echo "patchelf bundled -> usr/bin/patchelf  ($(patchelf --version 2>&1 | head -1))"

# --- AppRun ---
cat > "$APPDIR/AppRun" <<'APPRUN'
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"

# --- Desktop integration ---
# Install the icon and a .desktop file pointing at this AppImage into the
# user's local share directories so GNOME/KDE can match the running window
# to the correct icon (taskbar, alt-tab switcher, app launcher).
# This is a one-time install; subsequent runs skip it if the files are current.
# $APPIMAGE is set by the AppImage runtime to the path of the .AppImage file.
# Fall back to $0 (the AppRun script itself) only if not set.
_APPIMAGE_PATH="${APPIMAGE:-$0}"
_ICON_SRC="$HERE/usr/share/icons/hicolor/256x256/apps/hyperxtalk.png"
_ICON_DEST="$HOME/.local/share/icons/hicolor/256x256/apps/hyperxtalk.png"
_DESKTOP_DEST="$HOME/.local/share/applications/hyperxtalk-appimage.desktop"

if [ ! -f "$_ICON_DEST" ] || [ "$_ICON_SRC" -nt "$_ICON_DEST" ]; then
    mkdir -p "$HOME/.local/share/icons/hicolor/256x256/apps"
    cp "$_ICON_SRC" "$_ICON_DEST"
    gtk-update-icon-cache --force --ignore-theme-index \
        "$HOME/.local/share/icons/hicolor" 2>/dev/null || true
fi

if [ ! -f "$_DESKTOP_DEST" ] || [ "$_APPIMAGE_PATH" != "$(grep '^Exec=' "$_DESKTOP_DEST" 2>/dev/null | sed 's/^Exec=//;s/ %U//')" ]; then
    mkdir -p "$HOME/.local/share/applications"
    sed "s|Exec=HyperXTalk %U|Exec=$_APPIMAGE_PATH %U|g" \
        "$HERE/usr/share/applications/HyperXTalk.desktop" > "$_DESKTOP_DEST"
    update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
fi

# Tell Ubuntu's BAMF window-matcher which .desktop file owns this process so
# the taskbar icon appears immediately without a logout/login cycle.
export BAMF_DESKTOP_FILE_HINT="$_DESKTOP_DEST"

export LD_LIBRARY_PATH="$HERE/usr/lib:$HERE/usr/bin${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# Fallback in case the engine's own probe doesn't run first.
export VLC_PLUGIN_PATH="$HERE/usr/bin/vlc-plugins/plugins"

# WebKit subprocess helper symlinks.
# The bundled libwebkit2gtk has its PKGLIBEXECDIR patched to /tmp/.hxt-wk/webkit2gtk-4.1
# at AppImage build time (env-var overrides are not available in Ubuntu release builds).
# We create symlinks here so the helpers are found at that stable /tmp path.
_WK_SUBPROC_LINK="/tmp/.hxt-wk/webkit2gtk-4.1"
mkdir -p "$_WK_SUBPROC_LINK"
for _helper in WebKitWebProcess WebKitNetworkProcess WebKitWebDriver; do
    [ -f "$HERE/usr/bin/Externals/webkit-subprocess/$_helper" ] && \
        ln -sfn "$HERE/usr/bin/Externals/webkit-subprocess/$_helper" \
                "$_WK_SUBPROC_LINK/$_helper" 2>/dev/null || true
done

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
