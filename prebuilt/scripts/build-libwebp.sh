#!/bin/bash

source "${BASEDIR}/scripts/platform.inc"
source "${BASEDIR}/scripts/lib_versions.inc"
source "${BASEDIR}/scripts/util.inc"

# libwebp source is committed to the repo at thirdparty/libwebp (v1.4.0).
# The gyp build compiles it from source, so this script only needs to
# ensure the source tree is populated.  If it's already there, skip.
#
# Original archives: https://storage.googleapis.com/downloads.webmproject.org/releases/webp/

THIS="libwebp"
URL_ROOT="https://storage.googleapis.com/downloads.webmproject.org/releases/webp/${THIS}-${libwebp_VERSION}.tar.gz"
ARCHIVE_DESTINATION="${THIS}-${libwebp_VERSION}"
FILE_DIRECTORY="../../thirdparty/${THIS}/src"

# If the source tree is already present (committed or previously fetched), skip.
if [ -d "../../thirdparty/${THIS}/src" ] && [ -n "$(ls -A ../../thirdparty/${THIS}/src 2>/dev/null)" ] ; then
	echo "libwebp sources already present — skipping fetch."
	exit 0
fi

fetchBinary
if [ ${DOWNLOADED} == 1 ] ; then
	cd "${BUILDDIR}"
	echo "Untarring ${ARCHIVE_DESTINATION}.tar"
	tar -xf "${ARCHIVE_DESTINATION}.tar"
	mkdir -p "${FILE_DIRECTORY}"
	cp -r "${BUILDDIR}/${ARCHIVE_DESTINATION}/src/"* "${FILE_DIRECTORY}/"
	mkdir -p "../../thirdparty/${THIS}/include/webp"
	cp -r "${BUILDDIR}/${ARCHIVE_DESTINATION}/src/webp/"* "../../thirdparty/${THIS}/include/webp/"
fi
