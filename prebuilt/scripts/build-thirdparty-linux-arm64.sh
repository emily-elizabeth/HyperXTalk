#!/bin/bash
# Build the vendored thirdparty libraries for linux-arm64 and copy their
# .a outputs into prebuilt/lib/linux/arm64/.
#
# Run this from inside an arm64 Linux environment (Docker on Apple Silicon
# or a native arm64 machine) with build-essential, cmake, autoconf,
# automake, libtool, nasm, python3, pkg-config installed.
#
# Usage (from repo root):
#   sh prebuilt/scripts/build-thirdparty-linux-arm64.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

ARCH=arm64
PLATFORM=linux
BUILD_DIR="${REPO_ROOT}/build-linux-arm64/hyperxtalk"
BUILD_OUT="${BUILD_DIR}/out/Release/obj.target/thirdparty"
PREBUILT_LIB="${REPO_ROOT}/prebuilt/lib/linux/arm64"

cd "${REPO_ROOT}"

echo "=== Configuring for linux-arm64 ==="
./config.sh --platform linux-arm64

echo "=== Building thirdparty-prebuilts ==="
export BUILDTYPE=Release
make -C "${BUILD_DIR}" thirdparty-prebuilts

echo "=== Copying outputs into prebuilt/lib/linux/arm64 ==="
mkdir -p "${PREBUILT_LIB}"

LIBS="libffi libpng libz libgif libjpeg libpcre libskia libsqlite libxml libxslt libzip"

FAILED_LIBS=""
for LIB in ${LIBS}; do
    SRC="${BUILD_OUT}/${LIB}/${LIB}.a"
    if [ -f "${SRC}" ]; then
        cp "${SRC}" "${PREBUILT_LIB}/"
        echo "  Copied ${LIB}.a"
    else
        echo "  WARNING: ${SRC} not found — skipping ${LIB}"
        FAILED_LIBS="${FAILED_LIBS} ${LIB}"
    fi
done

# libskia has variant archives (libskia_opt_none.a, libskia_opt_neon.a, etc.)
for F in "${BUILD_OUT}"/libskia/libskia_*.a; do
    [ -f "$F" ] && cp "$F" "${PREBUILT_LIB}/" && echo "  Copied $(basename $F)"
done

if [ -n "${FAILED_LIBS}" ]; then
    echo ""
    echo "NOTE: The following libs were not found (may be expected):${FAILED_LIBS}"
fi

echo ""
echo "=== Done. prebuilt/lib/linux/arm64 now has: ==="
ls "${PREBUILT_LIB}/" | sort
