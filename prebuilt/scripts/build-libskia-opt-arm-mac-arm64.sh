#!/bin/sh
# Build the ARM NEON + CRC32 Skia optimisation library for macOS arm64.
# Replaces the GYP-generated stub libskia_opt_arm.a (which only contains
# opts_dummy.o because the target_conditions arch check fails at GYP time)
# with a real archive containing all NEON and CRC32 implementations.
#
# Run from any directory; the script resolves paths relative to itself.
# Called automatically by build-thirdparty-mac-arm64.sh.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

SKIA_DIR="${REPO_ROOT}/thirdparty/libskia"
OUT_DIR="${REPO_ROOT}/prebuilt/lib/mac"
OUT_LIB="${OUT_DIR}/libskia_opt_arm.a"
OUT_LIB_HOST="${OUT_DIR}/libskia_opt_arm (host).a"

WORK_DIR="$(mktemp -d /tmp/build-libskia-opt-arm.XXXXXX)"
trap 'rm -rf "${WORK_DIR}"' EXIT

echo "Building arm64 libskia_opt_arm for macOS..."
echo "  Source : ${SKIA_DIR}/src/opts/{neon,crc32}"
echo "  Output : ${OUT_LIB}"

SDK_PATH="$(xcrun --sdk macosx --show-sdk-path)"
CLANG="$(xcrun -find clang)"

# Include dirs mirror skia_include_dirs in libskia.gyp
INCLUDES=""
for d in \
    include/android include/c include/codec include/config include/core \
    include/effects include/images include/pathops include/ports \
    include/private include/svg include/utils include/utils/ios \
    include/utils/mac include/utils/win include/views include/xml \
    src/android src/c src/codec src/core src/effects src/fonts \
    src/image src/images src/lazy src/opts src/pathops src/pdf \
    src/pipe src/ports src/sfnt src/svg src/utils src/views \
    src/xml src/xps
do
    INCLUDES="${INCLUDES} -I${SKIA_DIR}/${d}"
done
# libjpeg
INCLUDES="${INCLUDES} -I${REPO_ROOT}/thirdparty/libjpeg/src"

CFLAGS="-arch arm64 -mmacosx-version-min=12.0 -isysroot ${SDK_PATH} \
        -O2 -fPIC -std=c++11 \
        -DSK_BUILD_FOR_MAC \
        -x c++"

OBJECTS=""

compile_src() {
    local SRC="$1"
    local BASE
    BASE="$(basename "${SRC}" .cpp)"
    local OBJ="${WORK_DIR}/${BASE}.o"
    echo "  CXX ${BASE}.cpp"
    ${CLANG} ${CFLAGS} ${INCLUDES} -c "${SRC}" -o "${OBJ}"
    OBJECTS="${OBJECTS} ${OBJ}"
}

# opts_arm_srcs
compile_src "${SKIA_DIR}/src/opts/SkBitmapProcState_opts_none.cpp"
compile_src "${SKIA_DIR}/src/opts/SkBlitMask_opts_arm.cpp"
compile_src "${SKIA_DIR}/src/opts/SkBlitRow_opts_arm.cpp"

# opts_armv7_arm64_srcs  (NEON — always available on arm64)
compile_src "${SKIA_DIR}/src/opts/SkBitmapProcState_arm_neon.cpp"
compile_src "${SKIA_DIR}/src/opts/SkBitmapProcState_matrixProcs_neon.cpp"
compile_src "${SKIA_DIR}/src/opts/SkBlitMask_opts_arm_neon.cpp"
compile_src "${SKIA_DIR}/src/opts/SkBlitRow_opts_arm_neon.cpp"

# opts_crc32_srcs  (ARMv8 CRC32 hardware instruction)
compile_src "${SKIA_DIR}/src/opts/SkOpts_crc32.cpp"

mkdir -p "${OUT_DIR}"
rm -f "${OUT_LIB}"
echo "  AR  libskia_opt_arm.a"
ar rcs "${OUT_LIB}" ${OBJECTS}

# The host toolset copy is identical for a native (non-cross) arm64 build
cp "${OUT_LIB}" "${OUT_LIB_HOST}"

echo ""
echo "Done — installed arm64 libskia_opt_arm.a:"
file "${OUT_LIB}"
ar -t "${OUT_LIB}"
