#!/bin/bash
# Build windowanimation_glue.dylib and install it into the LCB extension folder.
# Run from the directory containing LCWindowAnimation.m
# Usage: ./build_glue.sh /path/to/org.openxtalk.windowanimation

set -e

EXTENSION_DIR="${1:-.}"

echo "Building arm64..."
clang -x objective-c -dynamiclib -framework Cocoa \
  -arch arm64 \
  -fobjc-arc \
  -undefined dynamic_lookup \
  -o windowanimation_glue_arm64.dylib LCWindowAnimation.m

echo "Building x86_64..."
clang -x objective-c -dynamiclib -framework Cocoa \
  -arch x86_64 \
  -fobjc-arc \
  -undefined dynamic_lookup \
  -o windowanimation_glue_x86_64.dylib LCWindowAnimation.m

echo "Creating universal binary..."
lipo -create windowanimation_glue_arm64.dylib windowanimation_glue_x86_64.dylib \
  -output windowanimation_glue.dylib

echo "Installing..."
mkdir -p "$EXTENSION_DIR/code/x86_64-mac"
mkdir -p "$EXTENSION_DIR/code/arm64-mac"
cp windowanimation_glue.dylib "$EXTENSION_DIR/code/x86_64-mac/windowanimation_glue.dylib"
cp windowanimation_glue.dylib "$EXTENSION_DIR/code/arm64-mac/windowanimation_glue.dylib"

rm windowanimation_glue_arm64.dylib windowanimation_glue_x86_64.dylib

echo "Done! Dylib installed to:"
echo "  $EXTENSION_DIR/code/x86_64-mac/windowanimation_glue.dylib"
echo "  $EXTENSION_DIR/code/arm64-mac/windowanimation_glue.dylib"
