#!/bin/bash

source "${BASEDIR}/scripts/platform.inc"
source "${BASEDIR}/scripts/lib_versions.inc"
source "${BASEDIR}/scripts/util.inc"

# 2025.01.28 ignore the specified version
# and grab the version number from the downloaded file
## NOTE: libpng has unfortunate naming conventions
## URL_ROOT will grab the latest version on the website.
## This may be more recent than libpng_VERSION, so the version file
## may need to be updated.
#libpng-1.6.58.tar.xz
# https://sourceforge.net/projects/libpng/files/libpng16/1.6.58/libpng-1.6.58.tar.xz/download

THIS="libpng"
# remove the dots with sed
PNG_VERSION=$(echo ${libpng_VERSION} | sed 's/\.//g')
URL_ROOT="https://sourceforge.net/projects/${THIS}/files/latest/download"
ARCHIVE_DESTINATION=${THIS}
FILE_DIRECTORY="../../thirdparty/${THIS}/src"

function unxzBinary
{
	echo "Untarring ${THIS}.tar"
	unzip -o "${THIS}.tar" -d "lpng"
	# retrieve the latest version number (1658)
	PNG_LATEST=$(echo `ls lpng` | sed 's/[^0-9]//g') # 1658
	ARCHIVE_DESTINATION="${THIS}${PNG_LATEST}/lpng${PNG_LATEST}"
	mv "libpng.tar" "libpng${PNG_LATEST}.tar"
	echo "ARCHIVE_DESTINATION= ${ARCHIVE_DESTINATION}"
	echo "Renaming lpng to ${THIS}${PNG_LATEST}"
	mv lpng "${THIS}${PNG_LATEST}"
	# now we should have a directory named libpng1648
}

if [ ! -d "${BUILDDIR}/${THIS}${PNG_VERSION}" ] ; then
	fetchBinary
	unxzBinary
	buildLibrary
fi
