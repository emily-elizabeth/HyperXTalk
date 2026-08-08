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

# --- Vosk speech recognition library ---
#
# lnx-speech.cpp dlopen()s libvosk.so at runtime, so we bundle it next to the
# binary.  Search order:
#   1. $REPO_ROOT/prebuilt/lib/linux/x86_64/libvosk.so  (CI / vendored)
#   2. The libvosk.so that ships inside the 'vosk' Python wheel
#   3. System paths (/usr/lib, /usr/local/lib)
#
VOSK_LIB=""
VOSK_LIB_PREBUILT="$REPO_ROOT/prebuilt/lib/linux/x86_64/libvosk.so"
if [ -f "$VOSK_LIB_PREBUILT" ]; then
    VOSK_LIB="$VOSK_LIB_PREBUILT"
else
    # Try the Python vosk package (pip install vosk bundles libvosk.so)
    VOSK_PY_LIB="$(python3 -c \
        "import vosk, os; p=os.path.join(os.path.dirname(vosk.__file__),'libvosk.so'); print(p) if os.path.isfile(p) else None" \
        2>/dev/null || true)"
    if [ -n "$VOSK_PY_LIB" ] && [ -f "$VOSK_PY_LIB" ]; then
        VOSK_LIB="$VOSK_PY_LIB"
    else
        VOSK_LIB="$(find /usr/lib /usr/local/lib -maxdepth 3 \
            \( -name 'libvosk.so' -o -name 'libvosk.so.0' \) 2>/dev/null | head -1 || true)"
    fi
fi

if [ -n "$VOSK_LIB" ] && [ -f "$VOSK_LIB" ]; then
    echo "Bundling Vosk library: $VOSK_LIB"
    cp "$VOSK_LIB" "$APPBIN/libvosk.so"
    strip --strip-debug "$APPBIN/libvosk.so" 2>/dev/null || true
else
    echo "WARNING: libvosk.so not found — speech recognition will report an error at runtime."
    echo "  To include it: pip install vosk  (or place libvosk.so in prebuilt/lib/linux/x86_64/)"
fi

# --- Vosk model ---
#
# Bundle a small speech recognition model so startListening works out of the
# box.  The bundled model is selected in this order:
#   1. $REPO_ROOT/prebuilt/vosk-model/          (checked-in / CI-cached)
#   2. ~/.local/share/vosk/model                 (developer's local model)
#   3. Download vosk-model-small-en-us-0.15 and cache it in prebuilt/
#
# AppRun sets VOSK_MODEL_PATH to the bundled model at launch, so users can
# still override it by exporting VOSK_MODEL_PATH before starting HyperXTalk.
#
VOSK_MODEL_CACHE="$REPO_ROOT/prebuilt/vosk-model"
VOSK_MODEL_SRC=""

if [ -f "$VOSK_MODEL_CACHE/conf/model.conf" ] || [ -f "$VOSK_MODEL_CACHE/am/final.mdl" ]; then
    VOSK_MODEL_SRC="$VOSK_MODEL_CACHE"
elif [ -f "$HOME/.local/share/vosk/model/conf/model.conf" ] || \
     [ -f "$HOME/.local/share/vosk/model/am/final.mdl" ]; then
    VOSK_MODEL_SRC="$HOME/.local/share/vosk/model"
fi

mkdir -p "$APPBIN/vosk-model"
if [ -n "$VOSK_MODEL_SRC" ]; then
    echo "Bundling Vosk model from: $VOSK_MODEL_SRC"
    cp -a "$VOSK_MODEL_SRC/." "$APPBIN/vosk-model/"
else
    echo "No Vosk model found locally — downloading vosk-model-small-en-us-0.15..."
    MODEL_URL="https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip"
    MODEL_ZIP="$BUILD_DIR/vosk-model-small-en-us-0.15.zip"
    if [ ! -f "$MODEL_ZIP" ]; then
        curl -fsSL --progress-bar -o "$MODEL_ZIP" "$MODEL_URL"
    fi
    TMPDIR_MODEL="$(mktemp -d)"
    unzip -q "$MODEL_ZIP" -d "$TMPDIR_MODEL"
    # The zip extracts to a single versioned subdirectory — move its contents up.
    MODEL_SUBDIR="$(ls "$TMPDIR_MODEL")"
    cp -a "$TMPDIR_MODEL/$MODEL_SUBDIR/." "$APPBIN/vosk-model/"
    rm -rf "$TMPDIR_MODEL"
    # Cache in prebuilt/ so future AppImage builds don't re-download.
    mkdir -p "$VOSK_MODEL_CACHE"
    cp -a "$APPBIN/vosk-model/." "$VOSK_MODEL_CACHE/"
    echo "Model cached at: $VOSK_MODEL_CACHE"
fi
echo "Vosk model bundled: $(du -sh "$APPBIN/vosk-model" | cut -f1)"

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

# --- AppRun ---
cat > "$APPDIR/AppRun" <<'APPRUN'
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/bin${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# Point to the bundled Vosk model unless the user has already set a preference.
if [ -z "$VOSK_MODEL_PATH" ] && [ -d "$HERE/usr/bin/vosk-model" ]; then
    export VOSK_MODEL_PATH="$HERE/usr/bin/vosk-model"
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
OUTPUT="$BUILD_DIR/HyperXTalk-${VERSION}-${ARCH}.AppImage"
ARCH="$ARCH" "$APPIMAGETOOL" "$APPDIR" "$OUTPUT"

echo ""
echo "AppImage created: $OUTPUT"
echo "Size: $(du -h "$OUTPUT" | cut -f1)"
