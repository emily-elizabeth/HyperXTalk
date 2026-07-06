# Common script functions

# Download url to file
function fetchUrl {
	URL=$1
	SAVEPATH=$2

	# Download using an HTTP client of some variety
	if $(which curl 1>/dev/null 2>/dev/null) ; then
		curl --fail -L "${URL}" -o "${SAVEPATH}"
	elif $(which wget 1>/dev/null 2>/dev/null) ; then
		wget "${URL}" -O "${SAVEPATH}"
	else
		# Perl as a last resort (useful for Cygwin)
		perl -MLWP::Simple -e "getstore('${URL}', '${SAVEPATH}') == 200 or exit 1"
	fi
}

DOWNLOADED=0

function fetchBinary
{
	cd "${BUILDDIR}"

	DOWNLOADED=0
	TARFILE="${ARCHIVE_DESTINATION}".tar

	#if directory $THIS does not exist then
	if [[ ! -d "$THIS"* ]] ; then
		# if $ARCHIVE_DESTINATION doesn't exist (file or directory) then
		if [ ! -f "$TARFILE" ] ; then
			echo "Fetching ${THIS} source"
			fetchUrl "${URL_ROOT}" "${TARFILE}"
			if [ $? != 0 ] ; then
				echo "downloading ${URL_ROOT} failed"
				# we no longer need $ARCHIVE_DESTINATION
				if [ -e "${ARCHIVE_DESTINATION}" ] ; then
					rm ${ARCHIVE_DESTINATION}
				fi
				exit
			fi
			DOWNLOADED=1
		fi # check for already downloaded source
	fi # check for destination directory
}

function untarBinary
{
	if [ ${DOWNLOADED} == 1 ] ; then
		if [ -f "$TARFILE" ] ; then
			echo "Untarring ${TARFILE}"
			tar -xf "${TARFILE}"
		fi
	fi
}

function unzipBinary
{
	if [ ${DOWNLOADED} == 1 ] ; then
		if [ -f "$ARCHIVE_DESTINATION" ] ; then
			echo "Unzipping ${ARCHIVE_DESTINATION}.zip"
			unzip "${ARCHIVE_DESTINATION}.zip"
		fi
	fi
}

# special case: file is named ".tar" but needs to be unzipped
function unxzBinary
{
	if [ ${DOWNLOADED} == 1 ] ; then
		if [ -f "$ARCHIVE_DESTINATION" ] ; then
			echo "Unzipping ${ARCHIVE_DESTINATION}.tar"
			unzip "${ARCHIVE_DESTINATION}.tar"
		fi
	fi
}

function makeBinary
{
	if [ ${DOWNLOADED} == 1 ] ; then
		if [ -e "${BUILDDIR}/${ARCHIVE_DESTINATION}/Makefile" ] ; then
			echo "running make"
			#mkdir -p ${BUILDDIR}/${ARCHIVE_DESTINATION}/build
			pushd "${BUILDDIR}/${ARCHIVE_DESTINATION}"
			make
			popd
		fi
	fi
}

function cmakeBinary
{
	if [ ${DOWNLOADED} == 1 ] ; then
		if [ -e "${BUILDDIR}/${ARCHIVE_DESTINATION}/CMakeLists.txt" ] ; then
			echo "running cmake"
			mkdir -p ${BUILDDIR}/${ARCHIVE_DESTINATION}/build
			pushd "${BUILDDIR}/${ARCHIVE_DESTINATION}"
			cmake -S . -B build
			makeBinary
			popd
		fi
	fi
}

function mesonBinary
{
	if [ ${DOWNLOADED} == 1 ] ; then
		if [ -e "${BUILDDIR}/${ARCHIVE_DESTINATION}/meson.build" ] ; then
			echo "running meson and ninja"
			mkdir -p ${BUILDDIR}/${ARCHIVE_DESTINATION}/build
			pushd "${BUILDDIR}/${ARCHIVE_DESTINATION}"
			meson setup --wipe build
			if [ -e "build/build.ninja" ] ; then
				echo "launching ninja"
				ninja -C build
			fi
			popd
		fi
	fi
}

# run cmake
# copy files if we have a newer version
function buildLibrary {
	if [ ${DOWNLOADED} == 1 ] ; then
		cmakeBinary
		mkdir -p ${FILE_DIRECTORY}
		cp ${BUILDDIR}/${ARCHIVE_DESTINATION}/*.h ${FILE_DIRECTORY}
		cp ${BUILDDIR}/${ARCHIVE_DESTINATION}/*.c ${FILE_DIRECTORY}
	fi
}

# run cmake
# run meson
# copy files if we have a newer version
function buildSrcLibrary {
	if [ ${DOWNLOADED} == 1 ] ; then
		cmakeBinary
		mesonBinary
		cp -r ${BUILDDIR}/${ARCHIVE_DESTINATION}/src/*.h ${INCLUDE_DIRECTORY}
		cp -r ${BUILDDIR}/${ARCHIVE_DESTINATION}/src/*.c ${FILE_DIRECTORY}
	fi
}
