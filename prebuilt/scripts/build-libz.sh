#!/bin/bash

source "${BASEDIR}/scripts/platform.inc"
source "${BASEDIR}/scripts/lib_versions.inc"
source "${BASEDIR}/scripts/util.inc"

# 2026.10.06 currently ${LIBZ_VERSION} is 1.3.2

THIS="libz"
URL_ROOT="https://www.zlib.net/current/zlib.tar.gz"
ARCHIVE_DESTINATION="zlib-${libz_VERSION}"
FILE_DIRECTORY="../../thirdparty/${THIS}/src"

fetchBinary
if [ ${DOWNLOADED} == 1 ] ; then
	untarBinary

	# Use configure script instead of cmake for zlib
	cd "${BUILDDIR}/${ARCHIVE_DESTINATION}"
	if [ -e "./configure" ]; then
		echo "configuring zlib with configure script"
		./configure
		make
		cp zlib.h zconf.h "${BUILDDIR}/../include/" 2>/dev/null || true
		cp libz.a "${BUILDDIR}/../lib/${PLATFORM}/${ARCH}/" 2>/dev/null || true
	fi
fi

