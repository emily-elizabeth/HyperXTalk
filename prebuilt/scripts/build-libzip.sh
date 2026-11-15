#!/bin/bash

source "${BASEDIR}/scripts/platform.inc"
source "${BASEDIR}/scripts/lib_versions.inc"
source "${BASEDIR}/scripts/util.inc"

# 2026.07.04 currently ${libzip_VERSION} is 1.11.4

THIS="libzip"
URL_ROOT="https://libzip.org/download/${THIS}-${libzip_VERSION}.tar.gz"
ARCHIVE_DESTINATION="${THIS}-${libzip_VERSION}"
THIRD_PARTY_FILE_DIRECTORY="../../thirdparty/${THIS}/src"

# run cmake
# run meson
# copy files if we have a newer version
function buildSrcLibrary {
	# pass these to cmake
	CMAKEFLAGS="-DENABLE_COMMONCRYPTO=OFF -DENABLE_GNUTLS=OFF -DENABLE_OPENSSL=OFF",

	cmakeBinary
	mesonBinary
	# copy the relevant files to the thirdparty directory
	cp -r ${BUILDDIR}/${ARCHIVE_DESTINATION}/lib/*.h ${THIRD_PARTY_FILE_DIRECTORY}
	cp -r ${BUILDDIR}/${ARCHIVE_DESTINATION}/lib/*.c ${THIRD_PARTY_FILE_DIRECTORY}
#	cp ${BUILDDIR}/${ARCHIVE_DESTINATION}/build/*.h ${THIRD_PARTY_FILE_DIRECTORY} # config.h and zipconf.h
#	rm ${THIRD_PARTY_FILE_DIRECTORY}/zip_dirent.c
#	cp -r  ${THIRD_PARTY_FILE_DIRECTORY}/hxt_zip_dirent.c ${THIRD_PARTY_FILE_DIRECTORY}/zip_dirent.c
#	cp -r  ${THIRD_PARTY_FILE_DIRECTORY}/hxt_zip_hash.c ${THIRD_PARTY_FILE_DIRECTORY}/zip_hash.c
#	cp -r  ${THIRD_PARTY_FILE_DIRECTORY}/hxt_config.h ${THIRD_PARTY_FILE_DIRECTORY}/config.h

	# copy config.h and zipconfig.h from archive/build to revzip folder
	cp ${BUILDDIR}/${ARCHIVE_DESTINATION}/build/*.h ../../revzip/src
	# copy zip.h from lib in archive to revzip folder
	cp ${BUILDDIR}/${ARCHIVE_DESTINATION}/lib/zip.h ../../revzip/src

	# copy zip.h from lib in archive to libzip/include folder
	cp -r ${BUILDDIR}/${ARCHIVE_DESTINATION}/lib/zip.h ../../thirdparty/libzip/include
	# copy zipconf.h from lib in archive to libzip/include folder
	cp -r ${BUILDDIR}/${ARCHIVE_DESTINATION}/build/zipconf.h ../../thirdparty/libzip/include
}

fetchBinary # only download .tar archive if libzip directory doesn't exist
untarBinary
buildSrcLibrary

