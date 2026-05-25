#!/bin/bash

source "${BASEDIR}/scripts/platform.inc"
source "${BASEDIR}/scripts/lib_versions.inc"
source "${BASEDIR}/scripts/util.inc"

# 2024.07.24 currently ${libcairo_VERSION} is 1.18.0

THIS="libcairo"
URL_ROOT="https://www.cairographics.org/releases/cairo-${libcairo_VERSION}.tar.xz"
ARCHIVE_DESTINATION="cairo-${libcairo_VERSION}"
FILE_DIRECTORY="../../thirdparty/${THIS}/src"
INCLUDE_DIRECTORY="../../thirdparty/${THIS}/src"

if [ "${PLATFORM}" == "mac" ] ; then
	CAIRO_HAS_QUARTZ_FONT=1
	CAIRO_HAS_QUARTZ_IMAGE_SURFACE=1
	CAIRO_HAS_QUARTZ_SURFACE=1
elif [ "${PLATFORM}" == "linux" ] ; then
	CAIRO_HAS_FT_FONT=1
	CAIRO_HAS_FC_FONT=1
elif [ "${PLATFORM}" == "android" ] ; then
	CAIRO_HAS_FT_FONT=1
	CAIRO_HAS_FC_FONT=1
fi

fetchBinary
untarBinary
buildSrcLibrary
cp -u ${BUILDDIR}/${ARCHIVE_DESTINATION}/build/src/*.h ${INCLUDE_DIRECTORY}

