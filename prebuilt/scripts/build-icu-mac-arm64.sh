#!/bin/bash
# Build ICU 58.2 for arm64 macOS.
#
# Produces in one step:
#   - prebuilt/bin/mac/icupkg           (host tool used by minimal_icu_data)
#   - prebuilt/lib/mac/libicudata.a
#   - prebuilt/lib/mac/libicui18n.a
#   - prebuilt/lib/mac/libicuio.a
#   - prebuilt/lib/mac/libicutu.a
#   - prebuilt/lib/mac/libicuuc.a
#
# Source is downloaded to prebuilt/build/icu-58-mac-arm64/ (gitignored under
# prebuilt/build/). That directory is durable across runs — previous versions
# of this script built into /tmp, which disappears when the OS cleans it up
# and silently broke the build-mac-extras.sh follow-up step.
#
# Version is pinned to 58.2 deliberately — see prebuilt/versions/icu for the
# attempted-bump history (65.1 coredumps during lc-compile grammar generation).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ICU_VERSION="58.2"
ICU_VERSION_ALT="58_2"
ICU_VERSION_DASH="${ICU_VERSION_ALT//_/-}"

DOWNLOAD_URL="https://github.com/unicode-org/icu/releases/download/release-${ICU_VERSION_DASH}/icu4c-${ICU_VERSION_ALT}-src.tgz"
TARBALL_SHA256="2b0a4410153a9b20de0e20c7d8b66049a72aef244b53683d0d7521371683da0c"

BUILD_BASE="${REPO_ROOT}/prebuilt/build/icu-58-mac-arm64"
ICU_TGZ="${BUILD_BASE}/icu4c-${ICU_VERSION_ALT}-src.tgz"
ICU_SRC="${BUILD_BASE}/icu"
ICU_BUILD="${BUILD_BASE}/icu_build"
ICU_INSTALL="${BUILD_BASE}/icu_install"

PREBUILT_BIN="${REPO_ROOT}/prebuilt/bin/mac"
PREBUILT_LIB="${REPO_ROOT}/prebuilt/lib/mac"

echo "=== Building ICU ${ICU_VERSION} for arm64 macOS ==="

# ── 1. Fetch + verify source ──────────────────────────────────────────────────
mkdir -p "${BUILD_BASE}"

if [ ! -f "${ICU_TGZ}" ]; then
    echo "Downloading ICU ${ICU_VERSION} source..."
    # Download to a .partial path and rename only on success so an interrupted
    # curl never leaves a corrupt cache behind.
    curl -fL -o "${ICU_TGZ}.partial" "${DOWNLOAD_URL}"
    mv "${ICU_TGZ}.partial" "${ICU_TGZ}"
fi

echo "Verifying SHA256..."
ACTUAL_SHA="$(shasum -a 256 "${ICU_TGZ}" | awk '{print $1}')"
if [ "${ACTUAL_SHA}" != "${TARBALL_SHA256}" ]; then
    echo "ERROR: SHA256 mismatch for ${ICU_TGZ}"
    echo "  expected: ${TARBALL_SHA256}"
    echo "  actual:   ${ACTUAL_SHA}"
    echo "Removing bad tarball so the next run re-downloads."
    rm -f "${ICU_TGZ}"
    exit 1
fi

# Always re-extract — an interrupted tar would leave a partial tree that
# configure would silently consume.
echo "Unpacking..."
rm -rf "${ICU_SRC}"
tar -xf "${ICU_TGZ}" -C "${BUILD_BASE}"

# ── 2. Configure & build ──────────────────────────────────────────────────────
SDK="$(xcrun --sdk macosx --show-sdk-path)"
MACOS_MIN="11.0"

ICU_CONFIGURE_FLAGS=(
    "--disable-shared"
    "--enable-static"
    "--with-data-packaging=archive"
    "--disable-samples"
    "--disable-tests"
    "--prefix=${ICU_INSTALL}"
)

export CC="$(xcrun -find clang)"
export CXX="$(xcrun -find clang++)"
export CFLAGS="-arch arm64 -mmacosx-version-min=${MACOS_MIN} -isysroot ${SDK}"
export CXXFLAGS="${CFLAGS} -std=c++11 -stdlib=libc++"
export LDFLAGS="-arch arm64 -mmacosx-version-min=${MACOS_MIN} -isysroot ${SDK}"

rm -rf "${ICU_BUILD}" "${ICU_INSTALL}"
mkdir -p "${ICU_BUILD}"

echo "Configuring ICU..."
(cd "${ICU_BUILD}" && "${ICU_SRC}/source/runConfigureICU" MacOSX "${ICU_CONFIGURE_FLAGS[@]}")

echo "Building ICU (this takes a few minutes)..."
make -C "${ICU_BUILD}" -j"$(sysctl -n hw.logicalcpu)" 2>&1 | tail -5

echo "Installing to ${ICU_INSTALL}..."
make -C "${ICU_BUILD}" install 2>&1 | tail -5

# ── 3. Place icupkg ───────────────────────────────────────────────────────────
# ICU's 'make install' puts data-building tools (icupkg, genccode, …) under
# sbin/, not bin/. Look in both.
BUILT_ICUPKG=""
for candidate in "${ICU_INSTALL}/bin/icupkg" "${ICU_INSTALL}/sbin/icupkg"; do
    if [ -f "${candidate}" ]; then
        BUILT_ICUPKG="${candidate}"
        break
    fi
done

if [ -z "${BUILT_ICUPKG}" ]; then
    echo "ERROR: icupkg not found under ${ICU_INSTALL}/{bin,sbin}/"
    exit 1
fi

mkdir -p "${PREBUILT_BIN}"
rm -f "${PREBUILT_BIN}/icupkg"
cp "${BUILT_ICUPKG}" "${PREBUILT_BIN}/icupkg"
chmod +x "${PREBUILT_BIN}/icupkg"
codesign --force --sign - "${PREBUILT_BIN}/icupkg"

# ── 4. Place the five static libraries ────────────────────────────────────────
mkdir -p "${PREBUILT_LIB}"
for f in libicudata.a libicui18n.a libicuio.a libicutu.a libicuuc.a; do
    if [ ! -f "${ICU_INSTALL}/lib/${f}" ]; then
        echo "ERROR: ${ICU_INSTALL}/lib/${f} missing after make install"
        exit 1
    fi
    cp "${ICU_INSTALL}/lib/${f}" "${PREBUILT_LIB}/${f}"
done

# ── 4b. Place icudt58l.dat where the minimal_icu_data Xcode script expects it ─
# The Xcode build phase runs from prebuilt/ and calls:
#   icupkg share/icudt58l.dat ...
# so the file must live at prebuilt/share/icudt58l.dat.
ICU_DAT="${ICU_INSTALL}/share/icu/${ICU_VERSION}/icudt${ICU_VERSION_ALT%_*}l.dat"
if [ ! -f "${ICU_DAT}" ]; then
    # Fallback: search for any icudt*.dat under the install tree
    ICU_DAT="$(find "${ICU_INSTALL}/share" -name "icudt*.dat" | head -1)"
fi
if [ -z "${ICU_DAT}" ] || [ ! -f "${ICU_DAT}" ]; then
    echo "ERROR: icudt58l.dat not found under ${ICU_INSTALL}/share"
    exit 1
fi
mkdir -p "${REPO_ROOT}/prebuilt/share"
cp "${ICU_DAT}" "${REPO_ROOT}/prebuilt/share/icudt58l.dat"
echo "Installed ICU data: prebuilt/share/icudt58l.dat"

# ── 5. Summary ────────────────────────────────────────────────────────────────
echo ""
echo "=== Done. Installed artifacts: ==="
ls -la "${PREBUILT_BIN}/icupkg"
for f in libicudata.a libicui18n.a libicuio.a libicutu.a libicuuc.a; do
    ls -la "${PREBUILT_LIB}/${f}"
done
echo ""
echo "Verify with:"
echo "  ${PREBUILT_BIN}/icupkg --help"
