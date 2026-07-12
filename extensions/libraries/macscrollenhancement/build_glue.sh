#!/bin/bash
# Build scrollenhancement_glue.dylib and install it into the LCB extension folder.
# Run from the directory containing LCScrollEnhancement.m
# Usage: ./build_glue.sh /path/to/com.hyperxtalk.library.macscrollenhancement

set -e

EXTENSION_DIR="${1:-.}"

echo "Building arm64..."
clang -x objective-c -dynamiclib -framework Cocoa \
  -arch arm64 \
  -fobjc-arc \
  -undefined dynamic_lookup \
  -o scrollenhancement_glue_arm64.dylib LCScrollEnhancement.m

echo "Building x86_64..."
clang -x objective-c -dynamiclib -framework Cocoa \
  -arch x86_64 \
  -fobjc-arc \
  -undefined dynamic_lookup \
  -o scrollenhancement_glue_x86_64.dylib LCScrollEnhancement.m

echo "Creating universal binary..."
lipo -create scrollenhancement_glue_arm64.dylib scrollenhancement_glue_x86_64.dylib \
  -output scrollenhancement_glue.dylib

echo "Signing..."
# Sign with the same identity used for the rest of the app so Hardened Runtime
# library validation accepts it.
#
# Override by setting CODESIGN_IDENTITY before running this script, e.g.:
#   CODESIGN_IDENTITY="Apple Development: you@example.com (TEAMID)" ./build_glue.sh
#
# Otherwise we locate HyperXTalk.app, read its Team ID, and find the matching
# cert in your keychain automatically.
if [ -z "$CODESIGN_IDENTITY" ]; then
  APP=$(mdfind "kMDItemCFBundleIdentifier == 'com.hyperxtalk.hyperxtalk'" 2>/dev/null | head -1)
  if [ -n "$APP" ]; then
    TEAM=$(codesign -dv --verbose=4 "$APP" 2>&1 | grep TeamIdentifier | sed 's/TeamIdentifier=//')
    echo "  HyperXTalk Team ID: $TEAM"
    CODESIGN_IDENTITY=$(security find-identity -v -p codesigning \
      | grep "$TEAM" | grep -o '"[^"]*"' | head -1 | tr -d '"')
  fi
fi
if [ -z "$CODESIGN_IDENTITY" ]; then
  echo "ERROR: could not determine signing identity." >&2
  echo "Set CODESIGN_IDENTITY and re-run:" >&2
  echo "  security find-identity -v -p codesigning" >&2
  exit 1
fi
echo "  Using identity: $CODESIGN_IDENTITY"
codesign --sign "$CODESIGN_IDENTITY" --force scrollenhancement_glue.dylib

echo "Installing..."
mkdir -p "$EXTENSION_DIR/code/x86_64-mac"
mkdir -p "$EXTENSION_DIR/code/arm64-mac"
cp scrollenhancement_glue.dylib "$EXTENSION_DIR/code/x86_64-mac/scrollenhancement_glue.dylib"
cp scrollenhancement_glue.dylib "$EXTENSION_DIR/code/arm64-mac/scrollenhancement_glue.dylib"

rm scrollenhancement_glue_arm64.dylib scrollenhancement_glue_x86_64.dylib

echo "Done! Dylib installed to:"
echo "  $EXTENSION_DIR/code/x86_64-mac/scrollenhancement_glue.dylib"
echo "  $EXTENSION_DIR/code/arm64-mac/scrollenhancement_glue.dylib"
