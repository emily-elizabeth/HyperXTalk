#!/bin/bash

source "${BASEDIR}/scripts/platform.inc"
source "${BASEDIR}/scripts/lib_versions.inc"
source "${BASEDIR}/scripts/util.inc"

# 2024.11.26 currently ${libjpeg_VERSION} is 9f
#https://ijg.org/files/jpegsrc.v9b.tar.gz

SHORT_THIS="jpeg"
THIS="libjpeg"
URL_ROOT="https://ijg.org/files/${SHORT_THIS}src.v${libjpeg_VERSION}.tar.gz"
# major version of 9f is 9
JPEG_MAJOR_VERSION=$(echo "${libjpeg_VERSION}" | sed 's/[a-z]//')
ARCHIVE_DESTINATION="${SHORT_THIS}sr${JPEG_MAJOR_VERSION}"
FILE_DIRECTORY="../../thirdparty/${THIS}/src"

fetchBinary
if [ ${DOWNLOADED} == 1 ] ; then
	#rm -rf ${SHORT_THIS}-${libjpeg_VERSION}
	#unxzBinary
	untarBinary
	mv ${SHORT_THIS}-${libjpeg_VERSION} ${ARCHIVE_DESTINATION}
	# have to convert the windows-style line endings to linux
	# sudo apt install dos2unix
	pushd ${BUILDDIR}/${ARCHIVE_DESTINATION}
	if [ -e "${BUILDDIR}/${ARCHIVE_DESTINATION}/configure" ] ; then
		echo "running configure"
		./configure
	fi
	makeBinary
	popd
	buildLibrary
fi

