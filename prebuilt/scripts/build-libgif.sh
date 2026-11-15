#!/bin/bash

source "${BASEDIR}/scripts/platform.inc"
source "${BASEDIR}/scripts/lib_versions.inc"
source "${BASEDIR}/scripts/util.inc"

# 2026.10.06 currently ${libgif_VERSION} is 6.1.3
#giflib-6.1.3.tar

THIS="giflib"
URL_ROOT="https://sourceforge.net/projects/${THIS}/files/latest/download"
ARCHIVE_DESTINATION="${THIS}-${libgif_VERSION}"
FILE_DIRECTORY="../../thirdparty/libgif/src"

fetchBinary
untarBinary
buildLibrary
mkdir -p ../../thirdparty/libgif/include
cp ${FILE_DIRECTORY}/gif_lib.h ../../thirdparty/libgif/include

