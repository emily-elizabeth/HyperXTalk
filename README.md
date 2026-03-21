# HyperXTalk
Cross-platform x-talk development environment

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

Build the standalone
```
make compile-mac
```

Code sign all the bundles and executables and then the standalone
```
MACBIN=/Users/emily-elizabethhoward/Developer/HyperXTalk/mac-bin
find "$MACBIN" -not -name "*.dSYM" | while read F; do
    if [[ -f "$F" ]] && file "$F" | grep -qE "Mach-O|bundle"; then
        codesign --force --sign - "$F" 2>/dev/null && echo "Signed: $(basename $F)"
    fi
done
find "$MACBIN" -name "*.app" -exec codesign --force --deep --sign - {} \;
echo "Done signing."
```

Build the installer
```
python3 build_installer.py
```
