# This file sets the libraries version variables to the version defined in versions/
# grab the first line in the appropriate file

script_dir=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )

# create a version using
# LIBNAME{LIBNAME_buildrevision}{LIBNAME_buildversion}{LIBNAME_checksum}
# only LIBNAME is required, the others optional as needed
# to create the appropriate url

function readVersion {
	local LIBNAME=$1
	local LIBNAME_LC=$(tr "[:upper:]" "[:lower:]" <<< $LIBNAME)

	local VERSION=$(head --lines=1 "${script_dir}/../versions/${LIBNAME_LC}")
	eval ${LIBNAME}_VERSION='"${VERSION}"'
	export ${LIBNAME}_VERSION="${VERSION}"

	if [ -f "${script_dir}/../versions/${LIBNAME_LC}_buildrevision" ] ; then
		local BUILDREVISION=$(head --lines=1 "${script_dir}/../versions/${LIBNAME_LC}_buildrevision")
		eval ${LIBNAME}_BUILDREVISION='"${BUILDREVISION}"'
	fi

	# grab the buildversion file if it exists (cef only)
	if [ -f "${script_dir}/../versions/${LIBNAME_LC}_buildversion" ] ; then
		local BUILDVERSION=$(head --lines=1 "${script_dir}/../versions/${LIBNAME_LC}_buildversion")
		eval ${LIBNAME}_BUILDVERSION='"${BUILDVERSION}"'
	fi

	# grab the checksum file if it exists
	if [ -f "${script_dir}/../versions/${LIBNAME_LC}_checksum" ] ; then
		local CHECKSUM=$(head --lines=1 "${script_dir}/../versions/${LIBNAME_LC}_checksum")
		eval ${LIBNAME}_CHECKSUM='"${CHECKSUM}"'
	fi
}

readVersion OpenSSL
readVersion ICU
readVersion CEF
readVersion CEFChromium
#readVersion CEFBuildversion
readVersion Curl
readVersion libz
readVersion libzip
readVersion libgif
readVersion libjpeg
readVersion libpng
readVersion libwebp
readVersion libfreetype
readVersion libcairo
readVersion libiodbc

#readVersion Thirdparty

# extract hash from start of submodule status
# Thirdparty_VERSION=$(expr "$(git submodule status | grep 'thirdparty' )" : '[^0-9a-z]*\([0-9a-z]*\)')

# echo "OpenSSL_VERSION=${OpenSSL_VERSION}"

# Libraries used on all platforms
#Thirdparty_LIBS_CORE="libffi libpng libz libgif libjpeg libpcre libskia"

# Libraries used only on native (non emscripten) platforms
#Thirdparty_LIBS_NATIVE="libmysql libsqlite libxml libxslt libzip"

# Libraries used only on desktop platforms
#Thirdparty_LIBS_DESKTOP="libiodbc libpq libcairo"

# Libraries used by emscripten / android for fonts
#Thirdparty_LIBS_FONTS="libharfbuzz libfreetype"

#Thirdparty_LIBS_mac="$Thirdparty_LIBS_CORE $Thirdparty_LIBS_NATIVE $Thirdparty_LIBS_DESKTOP"
#Thirdparty_LIBS_linux="$Thirdparty_LIBS_CORE $Thirdparty_LIBS_NATIVE $Thirdparty_LIBS_DESKTOP"
#Thirdparty_LIBS_win32="$Thirdparty_LIBS_CORE $Thirdparty_LIBS_NATIVE $Thirdparty_LIBS_DESKTOP"
#Thirdparty_LIBS_ios="$Thirdparty_LIBS_CORE $Thirdparty_LIBS_NATIVE libcairo"
#Thirdparty_LIBS_emscripten="$Thirdparty_LIBS_CORE $Thirdparty_LIBS_FONTS"
#Thirdparty_LIBS_android="$Thirdparty_LIBS_CORE $Thirdparty_LIBS_NATIVE $Thirdparty_LIBS_FONTS libexpat libcairo"
