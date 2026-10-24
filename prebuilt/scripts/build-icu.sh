#!/bin/bash
set -e

source "${BASEDIR}/scripts/platform.inc"
source "${BASEDIR}/scripts/lib_versions.inc"
source "${BASEDIR}/scripts/util.inc"

# Configuration flags
ICU_CONFIG="--disable-shared --enable-static --prefix=/ --sbindir=/bin --with-data-packaging=archive --disable-samples --disable-tests --disable-extras"
ICU_CFLAGS="-DU_USING_ICU_NAMESPACE=0 -DUNISTR_FROM_CHAR_EXPLICIT=explicit -DUNISTR_FROM_STRING_EXPLICIT=explicit"

ICU_VERSION_DASH=$(echo "${ICU_VERSION}" | sed 's/\./-/g')
ICU_VERSION_UNDERSCORE=$(echo "${ICU_VERSION}" | sed 's/\./_/g')
ICU_VERSION_MAJOR=$(echo "${ICU_VERSION}" | sed 's/\..*//g')
ICU_CHECKSUM=$(echo "${ICU_CHECKSUM}")

echo icu checksum is ${ICU_CHECKSUM}
echo icu version is ${ICU_VERSION_MAJOR}

# mdw 2023.11.10 new download url
ICU_SHASUM_ROOT="https://github.com/unicode-org/icu/releases/download/release-"
if [ ${ICU_VERSION_MAJOR} -gt 64 ] ; then
	# mdw 2026.0.05 new download url for 78.3
	ICU_ROOT="https://github.com/unicode-org/icu/archive/refs/tags/release-"
else
	ICU_ROOT="https://github.com/unicode-org/icu/releases/download/release-"
fi

# Grab the source for the library
ICU_SRC="icu-${ICU_VERSION}"
ICU_MD5="icu-${ICU_VERSION}.md5"
cd "${BUILDDIR}"

# Needed for cross-compiles
case $(uname) in
	Linux*)
		HOST_ICU_DIR="${BUILDDIR}/icu-${ICU_VERSION}-linux-${CROSS_HOST}"
		;;
	Darwin*)
		HOST_ICU_DIR="${BUILDDIR}/icu-${ICU_VERSION}-mac-i386"
		;;
	CYGWIN*)
		HOST_ICU_DIR="${BUILDDIR}/icu-${ICU_VERSION}-win32-i386"
#		HOST_ICU_DIR="${BUILDDIR}/icu-${ICU_VERSION}-win-86"
# move to 64-bit Windows?
#		HOST_ICU_DIR="${BUILDDIR}/icu-${ICU_VERSION}-win-86_64"
		;;
esac

if [ ${ICU_VERSION_MAJOR} -gt 64 ] ; then
	ICU_URL="${ICU_ROOT}${ICU_VERSION}.zip"
else
	ICU_URL="${ICU_ROOT}${ICU_VERSION_DASH}/icu4c-${ICU_VERSION_UNDERSCORE}-src.tgz"
fi
ICU_TGZ="icu4c-${ICU_VERSION_UNDERSCORE}-src.tgz"

if [ ! -d "$ICU_SRC" ] ; then
	if [ ! -e "$ICU_TGZ" ] ; then
		echo "Fetching ICU source at ${ICU_URL}"
		fetchUrl ${ICU_URL} "${ICU_TGZ}"
		if [ $? != 0 ] ; then
			echo "    failed"
			if [ -e "${ICU_TGZ}" ] ; then
				rm ${ICU_TGZ}
			fi
			exit
		fi
	fi

# the shasum check file is named differently in different versions.
	if [ "${ICU_VERSION_MAJOR}" -gt "64" ] ; then
		ICU_MD5_URL="SHASUM512.txt"
	elif [ "${ICU_VERSION_MAJOR}" == "63" ] ; then
		ICU_MD5_URL="icu4c-SHASUM512.txt.asc"
	elif [ "${ICU_VERSION_MAJOR}" == "58" ] ; then
		ICU_MD5_URL="icu4c-SHASUM512.txt.asc"
	else
		echo "bad shasum url for icu version ${ICU_VERSION}"
	fi
	ICU_SHASUM_URL="${ICU_SHASUM_ROOT}${ICU_VERSION}/${ICU_MD5_URL}"

	# validate the checksum (skip for ICU 58 — its shasum file lacks parseable checksums)
	if [ 0 != "${ICU_CHECKSUM}" ] && [ "${ICU_VERSION_MAJOR}" != "58" ] ; then
		echo "Fetching checksum file ${ICU_CHECKSUM} "
		fetchUrl ${ICU_CHECKSUM} "KEYS"
		if [ $? != 0 ] ; then
			echo "downloading checksum file failed"
			if [ -e "${ICU_TGZ}" ] ; then
				rm ${ICU_TGZ}
			fi
			exit
		fi

		# validate using gpg if gpg is installed
		if command -v gpg >/dev/null 2>&1; then
			gpg --import KEYS
		fi

		echo "Fetching shasum file ${ICU_SHASUM_URL}"
#		if fetchUrl ${ICU_SHASUM_URL} ${ICU_MD5_URL}; then
#			shasum -c -s --ignore-missing ${ICU_MD5_URL}
#			if [ $? != 0 ] ; then
#				echo "checksum verification failed"
#				if [ -e "${ICU_TGZ}" ] ; then
#					rm ${ICU_TGZ}
#				fi
#				exit 1
#			fi
#		else
#			echo "Error: downloading shasum file failed, cannot verify integrity" && exit 1
#		fi

	fi

	echo "Unpacking ICU source"
	if [ "${ICU_VERSION_MAJOR}" -gt "64" ] ; then
		unzip ${ICU_TGZ}
		mv icu-release-${ICU_VERSION} "${ICU_SRC}"
	else
		tar -xf "${ICU_TGZ}"
		mv icu "${ICU_SRC}"
	fi

	# Fix for modern glibc (2.26+) where xlocale.h was removed
	find "${ICU_SRC}" \( -name "*.cpp" -o -name "*.c" \) | xargs grep -l "xlocale.h" 2>/dev/null | while read f; do
		sed -i.bak 's/xlocale.h/locale.h/g' "$f" && rm -f "$f.bak"
	done
fi

# copy header files from prebuilt/build/icu-58-2/source/common to prebuilt/include/unicode
# echo source = ${BUILDDIR}/${ICU_SRC}/source/common
# echo dest = ${BUILDDIR}/../include/unicode
mkdir -p ${BUILDDIR}/../include/unicode
if [ "${ICU_VERSION_MAJOR}" -gt "64" ] ; then
	ICU4C="icu4c/"
else
	ICU4C=""
fi

cp ${BUILDDIR}/${ICU_SRC}/${ICU4C}source/common/*.h ${BUILDDIR}/../include/unicode
cp ${BUILDDIR}/${ICU_SRC}/${ICU4C}source/common/unicode/*.h ${BUILDDIR}/../include/unicode

ICU_LIBS="data i18n io le lx tu uc"
ICU_BINARIES="icupkg pkgdata"

function buildICU {
	local PLATFORM=$1
	local ARCH=$2
	local SUBPLATFORM=$3

echo "PLATFORM = ${PLATFORM}, ARCH = ${ARCH} HOST_ARCH = ${HOST_ARCH}"

	# Platform-specific options for ICU
	case "${PLATFORM}" in
		mac)
			CONFIG_TYPE="MacOSX"
#			if [ "${ARCH}" == "ppc" -o "${ARCH}" == "ppc64" ] ; then
#				echo "WARNING: cross-compiled ICU data for PPC does not work"
#				CONFIG_TYPE+=" --host=${ARCH}-apple-darwin --with-cross-build=${HOST_ICU_DIR}"
#			fi
			;;
		linux)
			CONFIG_TYPE="Linux"
			if [ "${ARCH}" == "x86_64" ] ; then
				CONFIG_TYPE+=" --with-library-bits=64"
			elif [ "${ARCH}" == "armv6hf" ] ; then
				CONFIG_TYPE+=" --host=arm-rpi-linux-gnueabihf --with-cross-build=${HOST_ICU_DIR}"
			elif [ "${ARCH}" == "armv7" ] ; then
				CONFIG_TYPE+=" --host=arm-rpi2-linux-gnueabihf --with-cross-build=${HOST_ICU_DIR}"
			else
				CONFIG_TYPE+=" --with-library-bits=32"
			fi
			;;

		android)
			CONFIG_TYPE="Linux --with-cross-build=${HOST_ICU_DIR} --disable-tools"
			export EXTRA_CFLAGS="-D__STDC_INT64__ -DU_HAVE_NL_LANGINFO_CODESET=0"
			export EXTRA_CXXFLAGS="${EXTRA_CFLAGS}"
			;;
		emscripten)
			CONFIG_TYPE=
			CONFIG_FLAGS="--with-cross-build=${HOST_ICU_DIR}"
			;;
		ios)
			CONFIG_TYPE=
			CONFIG_FLAGS="--host=arm-apple-darwin --with-cross-build=${HOST_ICU_DIR} --disable-tools"
			;;
		win32)
			CONFIG_TYPE="Cygwin/MSVC"
			;;
	esac

	# Utility for displaying platform name
	if [ ! -z "${SUBPLATFORM}" ] ; then
		local NAME="${PLATFORM}/${ARCH}/${SUBPLATFORM}"
		local PLATFORM_NAME=${SUBPLATFORM}
	else
		local NAME="${PLATFORM}/${ARCH}"
		local PLATFORM_NAME=${PLATFORM}
	fi

	ICU_ARCH_SRC="${ICU_SRC}-${PLATFORM_NAME}-${ARCH}"

	# Copy the source to a target-specific directory
	if [ ! -d "${ICU_ARCH_SRC}" ] ; then
		echo "Creating ICU build directory for ${NAME}"
		mkdir -p "${ICU_ARCH_SRC}"
	fi

	# Get the command used to build a previous copy, if any
	if [ -e "${ICU_ARCH_SRC}/config.cmd" ] ; then
		ICU_ARCH_CURRENT_CONFIG=`cat ${ICU_ARCH_SRC}/config.cmd`
	else
		ICU_ARCH_CURRENT_CONFIG=
	fi

	ICU_ARCH_CONFIG="${CONFIG_TYPE} ${ICU_CONFIG} ${CONFIG_FLAGS}"

	# Re-configure and re-build, if required
	if [ "${ICU_ARCH_CONFIG}" != "${ICU_ARCH_CURRENT_CONFIG}" ] ; then
		cd "${ICU_ARCH_SRC}"
		echo "Configuring ICU for ${NAME}"

		echo "*DEBUG* calling setCCForArch ${ARCH}"
		setCCForTarget "${PLATFORM}" "${ARCH}" "${SUBPLATFORM}"

		# We need to pass the target triple for Android builds
		if [ "${PLATFORM}" == "android" ] ; then
			CONFIG_TYPE+=" --host=${ANDROID_TRIPLE}"
		fi

		echo "*DEBUG* calling ICU configure script"
		# Method for configuration depends on the platform
		if [ -z "${CONFIG_TYPE}" ] ; then
			${EMCONFIGURE} "../${ICU_SRC}/${ICU4C}source/configure" ${ICU_CONFIG} ${CONFIG_FLAGS}
		else
			"../${ICU_SRC}/${ICU4C}source/runConfigureICU" ${CONFIG_TYPE} ${ICU_CONFIG} ${CONFIG_FLAGS}
		fi

		echo "*DEBUG* disabling c++11 support on incompatible platforms"
		# Disable C++11 support on platforms where we can't guarantee a compatible runtime
#			android|linux)
		case "${PLATFORM}" in
			android)
				sed -i.bak -e "s/\(^CXXFLAGS.*\)--std=c++0x/\1/" icudefs.mk && rm -f icudefs.mk.bak
				;;
		esac

		# Make sure U_HAVE_STRTOD_L is 0 on android
 		if [ "android" == "${PLATFORM}" ] ; then
 			sed -i.bak -e "s/U_HAVE_STRTOD_L=1/U_HAVE_STRTOD_L=0/" icudefs.mk && rm -f icudefs.mk.bak
 		fi

		echo "Building ICU for ${NAME}"
		export VERBOSE=1
		${EMMAKE} make clean && \
			${EMMAKE} make ${MAKEFLAGS} && \
			${EMMAKE} make DESTDIR="${INSTALL_DIR}/${NAME}" install
		RESULT=$?
		cd ..

		# Save the configuration for this build
		if [ 0 == $RESULT  ] ; then
			echo "${ICU_ARCH_CONFIG}" > "${ICU_ARCH_SRC}/config.cmd"
		else
			echo "    failed"
			exit 1
		fi
	else
		echo "Found existing ICU build for ${NAME}"
	fi

	if [ -z "${HOST_ICU_DIR}" ] ; then
		HOST_ICU_BINDIR="${BUILDDIR}/${ICU_ARCH_SRC}/bin"
	else
		HOST_ICU_BINDIR="${HOST_ICU_DIR}/bin"
	fi

	# Copy data file, if not done yet
	if [ ! -f "${OUTPUT_DIR}/share/icudt${ICU_VERSION_MAJOR}l.dat" ] ; then
		echo "Copying icu data file"
		mkdir -p "${OUTPUT_DIR}/share"
		cp "${INSTALL_DIR}/${NAME}/share/icu/${ICU_VERSION}/icudt${ICU_VERSION_MAJOR}l.dat" "${OUTPUT_DIR}/share/icudt${ICU_VERSION_MAJOR}l.dat"
	fi

	# Copy libraries
	for L in ${ICU_LIBS} ; do
		if [ -f "${INSTALL_DIR}/${NAME}/lib/libicu${L}.a" ] ; then
			#if [ "${PLATFORM}" == "mac" -o "${PLATFORM}" == "ios" ] ; then
				#VAR="ICU${L}_LIBS"
				#eval "$VAR+=\"${INSTALL_DIR}/${NAME}/lib/libicu${L}.a \""
			#else
				mkdir -p "${OUTPUT_DIR}/lib/${NAME}"
				cp "${INSTALL_DIR}/${NAME}/lib/libicu${L}.a" "${OUTPUT_DIR}/lib/${NAME}/libicu${L}.a"
			#fi
		fi
	done

	# Copy executables
	for B in ${ICU_BINARIES} ; do
		if [ -f "${INSTALL_DIR}/${NAME}/bin/${B}" ] ; then
			#if [ "${PLATFORM}" == "mac" -o "${PLATFORM}" == "ios" ] ; then
				#VAR="ICU${B}_BINARIES"
				#eval "$VAR+=\"${INSTALL_DIR}/${NAME}/bin/${B} \""
			#else
				mkdir -p "${OUTPUT_DIR}/bin/${NAME}"
				cp "${INSTALL_DIR}/${NAME}/bin/${B}" "${OUTPUT_DIR}/bin/${NAME}/${B}"
			#fi
		fi
	done

	# Copy over the headers, if it has not yet been done
	if [ ! -e "${OUTPUT_DIR}/include/unicode" ] ; then
		echo "Copying ICU headers from ${INSTALL_DIR}/${NAME}/include"
		mkdir -p "${OUTPUT_DIR}/include/unicode"
		cp -r "${INSTALL_DIR}/${NAME}/include"/* "${OUTPUT_DIR}/include/"

		# Some header massaging is required in order to avoid Win32 link errors
		# NOTE - need to provide backup file extension for compatability with both MacOSX & Linux
		sed -i.bak -e 's/define U_IMPORT __declspec(dllimport)/define U_IMPORT/g' "${OUTPUT_DIR}/include/unicode/platform.h"
		sed -i.bak -e 's/define U_EXPORT __declspec(dllexport)/define U_EXPORT/g' "${OUTPUT_DIR}/include/unicode/platform.h"
		# Remove backup file
		rm "${OUTPUT_DIR}/include/unicode/platform.h.bak"
	fi
}


# Need to build ICU tools for the host platform
if [ "${HOST_PLATFORM}" != "${PLATFORM}" ] ; then
	# prevent custom c/c++ vars from being used to build for host
	TMP_CUSTOM_CC="${CUSTOM_CC}"
	TMP_CUSTOM_CXX="${CUSTOM_CXX}"
	CUSTOM_CC=
	CUSTOM_CXX=

	buildICU "${HOST_PLATFORM}" "${HOST_ARCH}"

	# Restore custom c/c++ vars
	CUSTOM_CC="${TMP_CUSTOM_CC}"
	CUSTOM_CXX="${TMP_CUSTOM_CXX}"

	# clear universal libs lists
	for L in ${ICU_LIBS} ; do
		VAR="ICU${L}_LIBS"
		eval "$VAR="
	done

	#clear universal binaries lists
	for B in ${ICU_BINARIES} ; do
		VAR="ICU${B}_BINARIES"
		eval "$VAR="
	done

	HOST_ICU_DIR="${BUILDDIR}/icu-${ICU_VERSION}-${HOST_PLATFORM}-${HOST_ARCH}"
fi

if [ "${ARCH}" == "universal" ] ; then
	# perform build for universal architectures
	for UARCH in ${UNIVERSAL_ARCHS} ; do
		buildICU "${PLATFORM}" "${UARCH}" "${SUBPLATFORM}"
	done

	# Create the universal libraries
	echo "Creating ICU ${PLATFORM_NAME} universal libraries"
	mkdir -p "${OUTPUT_DIR}/lib/${PLATFORM}/${SUBPLATFORM}"

	for L in ${ICU_LIBS} ; do
		VAR="ICU${L}_LIBS"
		eval VALUE=\$$VAR
		if [ ! -z "${VALUE}" ] ; then
			lipo -create $VALUE -output "${OUTPUT_DIR}/lib/${PLATFORM}/${SUBPLATFORM}/libicu${L}.a"
		fi
		eval "$VAR="
	done

	# Create the universal binaries
	echo "Creating ICU ${PLATFORM_NAME} universal binaries"
	mkdir -p "${OUTPUT_DIR}/bin/${PLATFORM}/${SUBPLATFORM}"

	for B in ${ICU_BINARIES} ; do
		VAR="ICU${B}_BINARIES"
		eval VALUE=\$$VAR
		if [ ! -z "${VALUE}" ] ; then
			lipo -create $VALUE -output "${OUTPUT_DIR}/bin/${PLATFORM}/${SUBPLATFORM}/${B}"
		fi
		eval "$VAR="
	done
else
	buildICU "${PLATFORM}" "${ARCH}" "${SUBPLATFORM}"
fi
