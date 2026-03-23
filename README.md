# HyperXTalk
Cross-platform x-talk development environment

Here are more things that need to be done before the steps below, thanks to Stam for documenting these:

***
PS: The changes required to get this to build are below. Some of this may just be AI confabulating, but was the only way I could get 'Success' on make compile-mac:

1. Build libffi from source for ARM64
The prebuilt libffi.a was missing. Run prebuilt/scripts/build-libffi-mac-arm64.sh to compile it from the bundled source in thirdparty/libffi/git_master/.

2. Fix python → python3 symlink
gen_icu_data_remove_list script called python (Python 2), which no longer exists on modern macOS:
sudo ln -s /usr/bin/python3 /usr/local/bin/python

3. Build missing third-party libraries from source
Several prebuilt .a files were absent. Built them via Xcode and copied to prebuilt/lib/mac/:
libskia, libsqlite, libxml, libzip, libcairo, libxslt, libiodbc

4. Copy libffi headers into the build include path
ffi.h, ffitarget.h, ffi_arm64.h, and related headers from thirdparty/libffi/git_master/darwin_common/include/ copied to _build/mac/Debug/include/.

5. OpenSSL 3.x API renames in prebuilt headers
The prebuilt .a libraries were OpenSSL 3.x but the headers still declared OpenSSL 1.x names. Fixed by renaming declarations and adding backward-compat macros:
prebuilt/include/openssl/evp.h — renamed declarations and appended:
c# define EVP_CIPHER_CTX_block_size EVP_CIPHER_CTX_get_block_size
c# define EVP_CIPHER_CTX_key_length EVP_CIPHER_CTX_get_key_length
c# define EVP_CIPHER_key_length EVP_CIPHER_get_key_length
prebuilt/include/openssl/ssl.h — renamed declaration and appended:
c# define SSL_get_peer_certificate SSL_get1_peer_certificate

6. Update ssl.stubs to match OpenSSL 3.x symbol names
thirdparty/libopenssl/ssl.stubs is the source for the generated weak stub library. Updated four entries:
EVP_CIPHER_key_length → EVP_CIPHER_get_key_length
EVP_CIPHER_CTX_key_length → EVP_CIPHER_CTX_get_key_length
EVP_CIPHER_CTX_block_size → EVP_CIPHER_CTX_get_block_size
SSL_get_peer_certificate → SSL_get1_peer_certificate
Then deleted the stale _build/mac/Debug/libopenssl_stubs.a to force regeneration.

7. Install libffi 3.5.2 via Homebrew
The Xcode project hardcoded /opt/homebrew/Cellar/libffi/3.5.2/. Updated Homebrew to get the exact version:
bashbrew update && brew upgrade libffi
Also copied `ffi.h` and `ffitarget.h` from Homebrew into `prebuilt/include/`.

8. Rebuild `libsqlite.a` with missing C++ dataset classes**
The prebuilt `libsqlite.a` only contained the raw SQLite C API. The C++ wrapper classes needed by `dbsqlite` were never compiled in. Manually compiled and added to the archive:
thirdparty/libsqlite/src/dataset.cpp
thirdparty/libsqlite/src/qry_dat.cpp
thirdparty/libsqlite/src/sqlitedataset.cpp
thirdparty/libsqlite/src/sqlitedecode.cpp
Rebuilt the archive cleanly by deleting the old one first and using ar rcs from scratch.
***

To reproduce from a clean checkout, these steps must be run before make compile-mac:

Rebuild FFI
```
sh prebuilt/scripts/build-libffi-mac-arm64.sh
```

Rebuild the third party libraries
```
REPO=/Users/emily-elizabethhoward/Developer/HyperXTalk
for LIB in libskia libsqlite libxml libzip libcairo libxslt libiodbc; do
  echo "=== Building $LIB ==="
  xcodebuild \
    -project "$REPO/build-mac/livecode/thirdparty/$LIB/$LIB.xcodeproj" \
    -configuration Debug \
    -arch arm64 \
    SOLUTION_DIR="$REPO" 2>&1 | grep -E "BUILD (SUCCEEDED|FAILED)|error:"
done
```

Rebuild libz
```
sh prebuilt/scripts/build-libz-mac-arm64.sh
```

Copy the files into place
```
REPO=/Users/emily-elizabethhoward/Developer/HyperXTalk
cp "$REPO/_build/mac/Debug/libcairo.a"  "$REPO/prebuilt/lib/mac/libcairo.a"
cp "$REPO/_build/mac/Debug/libxslt.a"   "$REPO/prebuilt/lib/mac/libxslt.a"
cp "$REPO/_build/mac/Debug/libiodbc.a"  "$REPO/prebuilt/lib/mac/libiodbc.a"
for F in "$REPO"/_build/mac/Debug/libskia*.a; do
  cp "$F" "$REPO/prebuilt/lib/mac/$(basename "$F")"
done
cp "$REPO/_build/mac/Debug/libxml.a"    "$REPO/prebuilt/lib/mac/libxml.a"
cp "$REPO/_build/mac/Debug/libzip.a"    "$REPO/prebuilt/lib/mac/libzip.a"
```

Build the standalone. The first command will make the executables and the second one will build the standalone in the /mac-bin folder
```
make compile-mac
make package-mac-bin
```
