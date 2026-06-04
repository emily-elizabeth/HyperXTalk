#!/bin/bash

# Abort building if any errors occur
set -e

# Build location
export BASEDIR=$(dirname $0)
export BUILDDIR="`pwd`/build"
export INSTALL_DIR="`pwd`/build/install"
export OUTPUT_DIR="`pwd`"
mkdir -p "${BUILDDIR}"
mkdir -p "${INSTALL_DIR}"
mkdir -p "${OUTPUT_DIR}"

# Target platform and architecture
export PLATFORM=$1
export ARCH=$2

#only ios and android subplatforms are used
# MDW 2025.03.27 Emscripten: allow JS and WASM?
if [ "${PLATFORM}" == "ios" ] || [ "${PLATFORM}" == "android" ] ; then
	export SUBPLATFORM=$3
# MDW 2025.03.27 Emscripten: allow JS and WASM?
elif [ "${PLATFORM}" == "emscripten" ] ; then
	export SUBPLATFORM=
else
	export SUBPLATFORM=
fi

# Capture the existing build variables, if any
export CUSTOM_CC="${CC}"
export CUSTOM_CXX="${CXX}"
export CUSTOM_EMMAKE="${EMMAKE}"
export CUSTOM_EMCONFIGURE="${EMCONFIGURE}"

echo "PLATFORM=" ${PLATFORM}

# If the prebuilt output directory for this platform already contains libraries,
# skip the fetch/build entirely. All required prebuilt .a files are already
# committed to the repo; the fetch step only needs to run once when setting up
# a fresh clone without the prebuilt binaries.
PREBUILT_LIB_DIR="${OUTPUT_DIR}/lib/${PLATFORM}"
if [ "${PLATFORM}" == "mac" ] || [ "${PLATFORM}" == "ios" ]; then
    if [ -f "${PREBUILT_LIB_DIR}/libcustomcrypto.a" ] && [ -f "${PREBUILT_LIB_DIR}/libiodbc.a" ] && [ -f "${PREBUILT_LIB_DIR}/libpng.a" ]; then
        echo "Prebuilt libraries already present in ${PREBUILT_LIB_DIR} — skipping fetch."
        exit 0
    fi
fi

# Set which libs to build for the target platform
case "${PLATFORM}" in
	android)
		PREBUILT_LIBS="openssl icu libz libzip libgif libjpeg libpng libcairo libfreetype libiodbc"
		;;
	mac)
		PREBUILT_LIBS="openssl icu libz libzip libgif libjpeg libpng libcairo libfreetype libiodbc"
		;;
	ios)
		PREBUILT_LIBS="openssl icu libz libzip libgif libjpeg libpng libcairo libfreetype libiodbc"
		;;
	win32)
		PREBUILT_LIBS="openssl curl icu cef libz libzip libgif libjpeg libpng libcairo libfreetype libiodbc"
		;;
	linux)
		PREBUILT_LIBS="openssl curl icu cef libz libgif libjpeg libpng libcairo libfreetype libiodbc"
		;;
	emscripten)
		PREBUILT_LIBS="icu libz libzip libgif libjpeg libpng libcairo libfreetype libiodbc"
		;;
esac

echo "PREBUILT_LIBS=" ${PREBUILT_LIBS}
echo "BASEDIR=" ${BASEDIR}

# Build all of the libraries that the target platform depends on
for t_lib in ${PREBUILT_LIBS} ; do
	echo "Building ${t_lib} using ${BASEDIR}/scripts/build-${t_lib}.sh"
	${BASEDIR}/scripts/build-${t_lib}.sh
done
