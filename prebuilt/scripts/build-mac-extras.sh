#!/bin/bash
#
# build-mac-extras.sh — populate the macOS prebuilt/ tree with the bits
# that BUILD1.md + the existing prebuilt/scripts scripts don't cover.
#
# Run this AFTER build-libffi-mac-arm64.sh and build-libz-mac-arm64.sh, and
# BEFORE build-libcairo-mac-arm64.sh — cairo is configured to link the
# libpng.a that this script produces.  See the prebuilt-mac step order in
# Makefile.Mac.
#
# Then this script:
#   - Builds libgif/libjpeg/libpng/libpcre via xcodebuild and installs them
#   - Supplies libcustomcrypto.a / libcustomssl.a from Homebrew openssl@3
#     (engine now targets the 3.x symbol names directly; no compat shim)
#   - Writes empty stub libpq.a and libmysql.a so the dbpostgresql /
#     dbmysql externals can link via -undefined dynamic_lookup
#
# Requirements: Xcode command line tools, Homebrew, and an installed
# openssl@3 (`brew install openssl@3`).
#
# All of this is a workaround for the broken fetch-mac step in the
# generated Xcode project. Once gyp is usable again or the prebuilt
# tarballs are re-hosted, this script can go away.

set -e

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "${REPO_ROOT}"

PREBUILT_LIB="${REPO_ROOT}/prebuilt/lib/mac"
DEBUG_OUT="${REPO_ROOT}/_build/mac/Debug"

mkdir -p "${PREBUILT_LIB}"

# ── 1. Extra thirdparty libs via xcodebuild ──────────────────────────────────
echo "=== Building libgif libjpeg libpng libpcre ==="
for LIB in libgif libjpeg libpng libpcre; do
    xcodebuild \
        -project "${REPO_ROOT}/build-mac/hyperxtalk/thirdparty/${LIB}/${LIB}.xcodeproj" \
        -configuration Debug \
        -arch arm64 \
        SOLUTION_DIR="${REPO_ROOT}" \
        2>&1 | grep -E "BUILD (SUCCEEDED|FAILED)|error:" || true
    if [ ! -f "${DEBUG_OUT}/${LIB}.a" ]; then
        echo "ERROR: ${LIB}.a missing after xcodebuild"
        exit 1
    fi
    cp "${DEBUG_OUT}/${LIB}.a" "${PREBUILT_LIB}/${LIB}.a"
done

# ── 2. libcustomcrypto / libcustomssl from Homebrew openssl@3 ────────────────
echo "=== Supplying libcustomcrypto/libcustomssl from Homebrew openssl@3 ==="
OPENSSL_PREFIX="$(brew --prefix openssl@3 2>/dev/null || true)"
if [ -z "${OPENSSL_PREFIX}" ] || [ ! -f "${OPENSSL_PREFIX}/lib/libcrypto.a" ]; then
    echo "ERROR: Homebrew openssl@3 not installed. Run: brew install openssl@3"
    exit 1
fi
# Homebrew installs these archives read-only. A plain `cp` copies the
# source's permission bits, so the destination also becomes read-only and
# the *next* `make prebuilt-mac` run fails with "Permission denied" here
# (set -e then aborts the whole script). `cp -f` replaces a read-only
# destination; the chmod keeps the installed copies writable thereafter.
cp -f "${OPENSSL_PREFIX}/lib/libcrypto.a" "${PREBUILT_LIB}/libcustomcrypto.a"
cp -f "${OPENSSL_PREFIX}/lib/libssl.a"    "${PREBUILT_LIB}/libcustomssl.a"
chmod u+w "${PREBUILT_LIB}/libcustomcrypto.a" "${PREBUILT_LIB}/libcustomssl.a"

# ── 3. Stub libpq.a and libmysql.a ───────────────────────────────────────────
# dbpostgresql.bundle and dbmysql.dylib link with -undefined dynamic_lookup
# so unresolved symbols are deferred to load time. The linker still insists
# on being able to find -lpq and -lmysql as files, so provide minimal
# archives with a single dummy object. Runtime will fail if you actually
# use these database drivers — install real libpq / libmysqlclient and
# replace these if you need that functionality.
echo "=== Creating stub libpq.a and libmysql.a ==="
STUB_DIR="/tmp/hxt-mac-extras"
mkdir -p "${STUB_DIR}"
STUB_C="${STUB_DIR}/db-driver-stub.c"
cat > "${STUB_C}" <<'EOF'
/* Stub archive marker for dbpostgresql / dbmysql link-time satisfaction.
 * These drivers actually resolve symbols at dlopen time; see
 * prebuilt/scripts/build-mac-extras.sh for context. */
int _hxt_db_driver_stub(void) { return 0; }
EOF
STUB_O="${STUB_DIR}/db-driver-stub.o"
clang -arch arm64 -c "${STUB_C}" -o "${STUB_O}"
ar rcs "${PREBUILT_LIB}/libpq.a"    "${STUB_O}"
ar rcs "${PREBUILT_LIB}/libmysql.a" "${STUB_O}"

echo ""
echo "=== Done. prebuilt/lib/mac now has: ==="
ls "${PREBUILT_LIB}/" | sort
