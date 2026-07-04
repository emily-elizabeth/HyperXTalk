#!/bin/bash

source "${BASEDIR}/scripts/platform.inc"
source "${BASEDIR}/scripts/lib_versions.inc"
source "${BASEDIR}/scripts/util.inc"

# 2026.07.04 currently ${libzip_VERSION} is 1.11.4

THIS="libzip"
URL_ROOT="https://libzip.org/download/${THIS}-${libzip_VERSION}.tar.gz"
ARCHIVE_DESTINATION="${THIS}-${libzip_VERSION}"
FILE_DIRECTORY="../../thirdparty/${THIS}/src"

# run cmake
# run meson
# copy files if we have a newer version
function buildSrcLibrary {
	cmakeBinary
	mesonBinary
	# copy the relevant files to the thirdparty directory
	cp -r ${BUILDDIR}/${ARCHIVE_DESTINATION}/lib/*.h ${FILE_DIRECTORY}
	cp -r ${BUILDDIR}/${ARCHIVE_DESTINATION}/lib/*.c ${FILE_DIRECTORY}
	cp ${BUILDDIR}/${ARCHIVE_DESTINATION}/build/*.h ${FILE_DIRECTORY}
}

fetchBinary # only download .tar archive if libzip directory doesn't exist
untarBinary
buildSrcLibrary
cp ${BUILDDIR}/${ARCHIVE_DESTINATION}/build/*.h ../../revzip/src

