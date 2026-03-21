set -e

chflags -R nouchg .

for d in $(find . -type d) ; do
	pushd $d
	for f in *2016_11_30*.h ; do
		if [ -f "$f" ] ; then
			chmod +w,-x "$f"
			mv -v "$f" $(echo "$f" | awk '{print $1;}').h
		fi
	done
	for f in *2016_12_01*.cpp ; do
		if [ -f "$f" ] ; then
			chmod +w,-x "$f"
			mv -v "$f" $(echo "$f" | awk '{print $1;}').cpp
		fi
	done
	for f in *201?_* ; do
		if [ -f "$f" ] ; then
			rm -v "$f"
		fi
	done
	popd
done
