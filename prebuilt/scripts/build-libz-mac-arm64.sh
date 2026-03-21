#!/bin/bash
# Build libz (zlib) for arm64 macOS, bypassing the xcodeproj
set -e

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
SRC="${REPO_ROOT}/thirdparty/libz/src"
INC="${REPO_ROOT}/thirdparty/libz/include"
BUILD_DIR="/tmp/libz_arm64_build"
OUT_LIB="${REPO_ROOT}/prebuilt/lib/mac/libz.a"
BUILD_OUT="${REPO_ROOT}/_build/mac/Debug/libz.a"

SDK_PATH="$(xcrun --sdk macosx --show-sdk-path)"
CLANG="$(xcrun -find clang)"

CFLAGS="-arch arm64 -mmacosx-version-min=11.0 -isysroot ${SDK_PATH} -O2"
INCLUDES="-I${SRC} -I${INC}"

SOURCES="adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c gzwrite.c \
         infback.c inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c"

echo "Building libz for arm64 macOS..."
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

cd "${BUILD_DIR}"
for F in $SOURCES; do
    echo "  Compiling $F"
    ${CLANG} ${CFLAGS} ${INCLUDES} -c "${SRC}/${F}" -o "${F%.c}.o"
done

echo "  Archiving libz.a"
rm -f "${OUT_LIB}"
ar rcs "${OUT_LIB}" *.o

# Also copy to _build/mac/Debug/ if it exists
if [ -d "${REPO_ROOT}/_build/mac/Debug" ]; then
    cp "${OUT_LIB}" "${BUILD_OUT}"
    echo "  Copied to ${BUILD_OUT}"
fi

echo "Done: ${OUT_LIB}"
file "${OUT_LIB}"
