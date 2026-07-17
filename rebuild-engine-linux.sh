#!/bin/bash
# Rebuild just the changed engine translation unit and relink HyperXTalk.
# Run this from the HyperXTalk repo root on your Linux dev machine.
#
# What it does:
#   1. Recompiles engine/src/native-layer-x11.cpp
#   2. Updates out/Release/obj.target/engine/libkernel.a
#   3. Relinks out/Release/HyperXTalk (development binary)
#   4. Copies result to linux-x86_64-bin/HyperXTalk

set -e
REPO="$(cd "$(dirname "$0")" && pwd)"
BD="$REPO/build-linux-x86_64/hyperxtalk"
OUT="$BD/out/Release"
OBJ="$OUT/obj.target"

echo "=== Compiling native-layer-x11.cpp ==="

# Flags extracted from build-linux-x86_64/hyperxtalk/engine/kernel.target.mk
DEFS=(
    -DHAVE_CONFIG_H=1
    -DPCRE_STATIC=1
    -DPCRE2_CODE_UNIT_WIDTH=16
    -DHAVE___THREAD
    -D_FILE_OFFSET_BITS=64
    -DPANGO_ENABLE_BACKEND
    -DPANGO_ENABLE_ENGINE
    -DU_STATIC_IMPLEMENTATION=1
    -DSK_RELEASE
    -DOPENSSL_API_COMPAT=0x30400000L
    -DCROSS_COMPILE_TARGET
    -DTARGET_PLATFORM_LINUX
    -DTARGET_PLATFORM_POSIX
    -DGTKTHEME
    -DLINUX
    -D_LINUX
    -DX11
    -D_RELEASE
    -DNDEBUG
)

GTK_CFLAGS=$(pkg-config --cflags gtk+-3.0 2>/dev/null || echo \
    "-I/usr/include/gtk-3.0 -I/usr/include/glib-2.0 \
     -I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
     -I/usr/include/pango-1.0 -I/usr/include/cairo \
     -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/harfbuzz \
     -I/usr/include/freetype2 -I/usr/include/fribidi \
     -I/usr/include/x86_64-linux-gnu -I/usr/include/gio-unix-2.0 \
     -I/usr/include/atk-1.0")

INCS=(
    $GTK_CFLAGS
    -I"$REPO/thirdparty/headers/linux/include"
    -I"$REPO/engine/include"
    -I"$REPO/engine/src"
    -I"$REPO/hxtlib"
    -I"$REPO/libfoundation/include"
    -I"$REPO/libgraphics/include"
    -I"$REPO/libscript/include"
    -I"$REPO/libbrowser/include"
    -I"$REPO/thirdparty/libpcre/include"
    -I"$REPO/thirdparty/libjpeg/include"
    -I"$REPO/thirdparty/libpng/include"
    -I"$REPO/prebuilt/include"
    -I"$OBJ/kernel/geni/src"
    -I"$OBJ/kernel/geni/include"
    -I"$OUT/obj/gen/src"
    -I"$OUT/obj/gen/include"
)

COMPILE_SRCS=(
    "engine/src/native-layer-x11.cpp"
    "engine/src/native-layer.cpp"
    "engine/src/widget.cpp"
    "engine/src/widget-ref.cpp"
    "engine/src/lnxdclnx.cpp"
    "engine/src/card.cpp"
    "engine/src/field.cpp"
    "engine/src/stack2.cpp"
)

COMPILE_OBJS=()
for SRC_REL in "${COMPILE_SRCS[@]}"; do
    OBJ_FILE="$OBJ/kernel/${SRC_REL%.cpp}.o"
    mkdir -p "$(dirname "$OBJ_FILE")"
    g++ -std=c++11 -fno-exceptions -fno-rtti -fPIC \
        -fstrict-aliasing -fvisibility=hidden \
        -Wall -Wextra -Wno-unused-parameter \
        -O2 \
        "${DEFS[@]}" \
        "${INCS[@]}" \
        -c "$REPO/$SRC_REL" \
        -o "$OBJ_FILE"
    COMPILE_OBJS+=("$OBJ_FILE")
    echo "  Compiled $SRC_REL"
done

echo "  All compiled OK"

echo "=== Compiling libbrowser_webkitgtk.cpp ==="
LB_INCS=(
    $(pkg-config --cflags gtk+-3.0 2>/dev/null || echo "-I/usr/include/gtk-3.0 -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/pango-1.0 -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0")
    -I"$REPO/libbrowser/include"
    -I"$REPO/thirdparty/headers/linux/include"
    -I"$REPO/libcore/include"
)
LB_OBJ="$OBJ/libbrowser/libbrowser/src/libbrowser_webkitgtk.o"
mkdir -p "$(dirname "$LB_OBJ")"
g++ -std=c++11 -fno-exceptions -fno-rtti -fPIC \
    -fstrict-aliasing -fvisibility=hidden \
    -Wall -Wextra -Wno-unused-parameter \
    -O2 \
    "${LB_INCS[@]}" \
    -c "$REPO/libbrowser/src/libbrowser_webkitgtk.cpp" \
    -o "$LB_OBJ"
ar r "$OBJ/libbrowser/libbrowser.a" "$LB_OBJ"
echo "  Updated libbrowser.a OK"

echo "=== Updating libkernel.a ==="
ar r "$OBJ/engine/libkernel.a" "${COMPILE_OBJS[@]}"
echo "  Updated OK"

echo "=== Relinking HyperXTalk ==="
# development.target.mk OBJS (just the two standalone-ish .o files)
DEV_OBJS=(
    "$OBJ/development/gen/engine_lcb_modules.o"
    "$OBJ/development/gen/src/startupstack.o"
    "$OBJ/development/engine/src/main.o"
)

LDFLAGS=(
    -rdynamic
    "-Wl,-rpath=\$ORIGIN/lib.target/"
    "-Wl,-rpath-link=$OUT/lib.target/"
    "-L$REPO/prebuilt/lib/linux/x86_64"
)

LIBS=(
    -lgtk-3 -lgdk-3 -lz
    -lpangocairo-1.0 -lpango-1.0 -lharfbuzz -latk-1.0
    -lcairo-gobject -lcairo -lgdk_pixbuf-2.0
    -lgio-2.0 -lgobject-2.0 -lglib-2.0
    -lvlc -ldl -lpthread -lcups -ldbus-1 -lX11
    -lpangoft2-1.0 -lfontconfig -lfreetype
    -licui18n -licuio -licutu -licuuc -licudata
    -lffi
)

c++ "${LDFLAGS[@]}" \
    -o "$OUT/HyperXTalk" \
    -Wl,--start-group \
    "${DEV_OBJS[@]}" \
    "$OBJ/engine/libkernel-development.a" \
    "$OBJ/engine/libsecurity-community.a" \
    "$OBJ/engine/libkernel.a" \
    "$OBJ/libfoundation/libFoundation.a" \
    "$OBJ/thirdparty/libz/libz.a" \
    "$OBJ/thirdparty/libffi/libffi.a" \
    "$OBJ/libgraphics/libGraphics.a" \
    "$OBJ/thirdparty/libgif/libgif.a" \
    "$OBJ/thirdparty/libpng/libpng.a" \
    "$OBJ/thirdparty/libjpeg/libjpeg.a" \
    "$OBJ/thirdparty/libskia/libskia.a" \
    "$OBJ/thirdparty/libskia/libskia_opt_none.a" \
    "$OBJ/thirdparty/libskia/libskia_opt_arm.a" \
    "$OBJ/thirdparty/libskia/libskia_opt_sse2.a" \
    "$OBJ/thirdparty/libskia/libskia_opt_sse3.a" \
    "$OBJ/thirdparty/libskia/libskia_opt_sse41.a" \
    "$OBJ/thirdparty/libskia/libskia_opt_sse42.a" \
    "$OBJ/thirdparty/libskia/libskia_opt_avx.a" \
    "$OBJ/thirdparty/libskia/libskia_opt_hsw.a" \
    "$OBJ/libscript/libScript.a" \
    "$OBJ/libscript/stdscript.a" \
    "$OBJ/libbrowser/libbrowser.a" \
    "$OBJ/thirdparty/libpcre/libpcre.a" \
    "$OBJ/thirdparty/libopenssl/libopenssl_stubs.a" \
    -Wl,--end-group \
    "${LIBS[@]}"

echo "  Linked OK"

echo "=== Deploying to linux-x86_64-bin/ ==="
cp "$OUT/HyperXTalk" "$REPO/linux-x86_64-bin/HyperXTalk"
echo "  Deployed to linux-x86_64-bin/HyperXTalk"
echo "=== Done ==="
