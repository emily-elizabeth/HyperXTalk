#!/bin/sh
# Build a static arm64 libffi.a for macOS from the sources already in the repo
# and install it into prebuilt/lib/mac/libffi.a
#
# Run from any directory; the script resolves paths relative to itself.
# Usage:  sh prebuilt/scripts/build-libffi-mac-arm64.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

LIBFFI_DARWIN_COMMON="${REPO_ROOT}/thirdparty/libffi/git_master/darwin_common"
LIBFFI_DARWIN_IOS="${REPO_ROOT}/thirdparty/libffi/git_master/darwin_ios"
LIBFFI_INCLUDE_DARWIN="${REPO_ROOT}/thirdparty/libffi/include_darwin"
OUT_DIR="${REPO_ROOT}/prebuilt/lib/mac"
OUT_LIB="${OUT_DIR}/libffi.a"

WORK_DIR="$(mktemp -d /tmp/build-libffi-arm64.XXXXXX)"
trap 'rm -rf "${WORK_DIR}"' EXIT

echo "Building arm64 libffi for macOS..."
echo "  Source : ${LIBFFI_DARWIN_COMMON}/src + ${LIBFFI_DARWIN_IOS}/src/aarch64"
echo "  Output : ${OUT_LIB}"

# Detect SDK path
SDK_PATH="$(xcrun --sdk macosx --show-sdk-path)"
CLANG="$(xcrun -find clang)"

LIBFFI_AARCH64_SRC="${REPO_ROOT}/thirdparty/libffi/git_master/src/aarch64"

# FFI_TRAMPOLINE_CLOSURE_OFFSET is defined in darwin_ios/include/ffitarget_arm64.h,
# but that file is shadowed by include_darwin/ffitarget_arm64.h in our -I order.
# Our include_darwin version only defines FFI_TRAMPOLINE_SIZE (now 40), not the
# offset, so we supply it explicitly.  40 is 8-byte aligned as required by ldp.
CFLAGS="-arch arm64 -mmacosx-version-min=11.0 -isysroot ${SDK_PATH} -O2 -fPIC \
        -DFFI_TRAMPOLINE_CLOSURE_OFFSET=40"
INCLUDES="-I${LIBFFI_INCLUDE_DARWIN} \
          -I${LIBFFI_DARWIN_IOS}/include \
          -I${LIBFFI_DARWIN_COMMON}/include \
          -I${LIBFFI_AARCH64_SRC}"

OBJECTS=""

# ── Compile common C sources ─────────────────────────────────────────────────
for SRC in \
    "${LIBFFI_DARWIN_COMMON}/src/prep_cif.c" \
    "${LIBFFI_DARWIN_COMMON}/src/types.c" \
    "${LIBFFI_DARWIN_COMMON}/src/raw_api.c" \
    "${LIBFFI_DARWIN_COMMON}/src/java_raw_api.c" \
    "${LIBFFI_DARWIN_COMMON}/src/closures.c" \
    "${LIBFFI_DARWIN_COMMON}/src/debug.c"
do
    BASE="$(basename "${SRC}" .c)"
    OBJ="${WORK_DIR}/${BASE}.o"
    echo "  CC  ${BASE}.c"
    ${CLANG} ${CFLAGS} ${INCLUDES} -c "${SRC}" -o "${OBJ}"
    OBJECTS="${OBJECTS} ${OBJ}"
done

# ── Compile arm64-specific C source ──────────────────────────────────────────
SRC="${LIBFFI_DARWIN_IOS}/src/aarch64/ffi_arm64.c"
OBJ="${WORK_DIR}/ffi_arm64.o"
echo "  CC  ffi_arm64.c"
${CLANG} ${CFLAGS} ${INCLUDES} -c "${SRC}" -o "${OBJ}"
OBJECTS="${OBJECTS} ${OBJ}"

# ── Assemble arm64 trampoline ─────────────────────────────────────────────────
# Apple's assembler rejects:
#   .cfi_def_cfa <reg>, <imm>   (register form)
#   .cfi_adjust_cfa_offset <expr>  (arithmetic expression)
# CFI / unwind info is not required for LiveCode's FFI use, so we preprocess
# the .S file, strip all .cfi_* directives, then assemble the cleaned source.
ASM="${LIBFFI_DARWIN_IOS}/src/aarch64/sysv_arm64.S"
ASM_PP="${WORK_DIR}/sysv_arm64.s"
OBJ="${WORK_DIR}/sysv_arm64.o"
echo "  CPP sysv_arm64.S"
${CLANG} ${CFLAGS} ${INCLUDES} -E -x assembler-with-cpp "${ASM}" -o "${ASM_PP}"
echo "  SED strip .cfi_* directives"
sed -i '' '/^[[:space:]]*\.cfi_/d' "${ASM_PP}"
echo "  AS  sysv_arm64.s"
${CLANG} -arch arm64 -mmacosx-version-min=11.0 -isysroot "${SDK_PATH}" \
         -x assembler -c "${ASM_PP}" -o "${OBJ}"
OBJECTS="${OBJECTS} ${OBJ}"

# ── Archive ───────────────────────────────────────────────────────────────────
mkdir -p "${OUT_DIR}"
rm -f "${OUT_LIB}"
echo "  AR  libffi.a"
ar rcs "${OUT_LIB}" ${OBJECTS}

echo ""
echo "Done — installed arm64 libffi.a:"
file "${OUT_LIB}"
echo "Symbols:"
nm -g "${OUT_LIB}" | grep " T _ffi_" | head -10

# ── Also copy to _build/mac/Debug/ if it already exists ─────────────────────
# The Xcode linker for lc-bootstrap-compile expects libffi.a in that directory
# (it's listed as an explicit absolute path in the link command, not via -lffi).
BUILD_DEBUG="${REPO_ROOT}/_build/mac/Debug"
if [ -d "${BUILD_DEBUG}" ]; then
    cp "${OUT_LIB}" "${BUILD_DEBUG}/libffi.a"
    echo "  CP  -> _build/mac/Debug/libffi.a"
fi
