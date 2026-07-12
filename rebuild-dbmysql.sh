#!/bin/bash
set -e
cd "$(dirname "$0")"

LIBMYSQL="prebuilt/lib/mac/libmysql.a"
STUB_THRESHOLD=2000  # bytes — real libmysql.a is hundreds of KB; stub is 736 bytes

# ── Check for a real libmysql.a ───────────────────────────────────────────────
if [ -f "$LIBMYSQL" ]; then
  LIB_SIZE=$(stat -f%z "$LIBMYSQL" 2>/dev/null || stat -c%s "$LIBMYSQL" 2>/dev/null || echo 0)
else
  LIB_SIZE=0
fi

if [ "$LIB_SIZE" -lt "$STUB_THRESHOLD" ]; then
  echo "┌──────────────────────────────────────────────────────────────────────┐"
  echo "│  MySQL client library is a stub (${LIB_SIZE} bytes).                       │"
  echo "│  Run this first to bake in MySQL (requires brew install mysql-client):│"
  echo "│    sh prebuilt/scripts/build-libmysql-mac-arm64.sh                  │"
  echo "└──────────────────────────────────────────────────────────────────────┘"
  echo ""
  echo "Continuing with stub — bundle will use dynamic MySQL lookup (requires"
  echo "system MySQL installation to connect)."
  echo ""
  MYSQL_BAKED_IN=0
else
  SIZE_KB=$((LIB_SIZE / 1024))
  echo "✓ libmysql.a present (${SIZE_KB} KB) — MySQL Connector/C will be baked in"
  MYSQL_BAKED_IN=1
fi

echo ""
echo "=== Building dbmysql.bundle ==="
xcodebuild \
  -project build-mac/hyperxtalk/revdb/revdb.xcodeproj \
  -target dbmysql \
  -configuration Release \
  2>&1 | tail -20

# Find the built bundle
BUILT=$(find _build/mac/Release -name "dbmysql.bundle" -type d 2>/dev/null | head -1)
if [ -z "$BUILT" ]; then
  echo "ERROR: Could not find built dbmysql.bundle"
  echo "Search paths tried: _build/mac/Release/"
  exit 1
fi
echo "Built: $BUILT"

# Verify the build contains the expected content
if [ "$MYSQL_BAKED_IN" -eq 1 ]; then
  # With static MySQL: mysql_init should be defined in the bundle itself
  strings "$BUILT/Contents/MacOS/dbmysql" | grep -q "mysql_init" && \
    echo "✓ mysql_init present in binary" || \
    echo "✗ WARNING: mysql_init not found — static linking may have failed"
else
  # With dynamic lookup: HXT error message should be present
  strings "$BUILT/Contents/MacOS/dbmysql" | grep -q "HXT:" && \
    echo "✓ Dynamic MySQL check present" || \
    echo "✗ WARNING: dynamic MySQL check string not found"
fi

echo ""
echo "=== Deploying ==="
DRIVERS="mac-bin/HyperXTalk.app/Contents/Tools/Externals/Database Drivers"
RT_DRIVERS="mac-bin/HyperXTalk.app/Contents/Tools/Runtime/Mac OS X/arm64/Externals/Database Drivers"

# Use ditto — unlike cp -R, ditto replaces existing directories rather than nesting
ditto "$BUILT" "mac-bin/dbmysql.bundle"
ditto "$BUILT" "$DRIVERS/dbmysql.bundle"
ditto "$BUILT" "$RT_DRIVERS/dbmysql.bundle"
echo "✓ mac-bin"

# Also deploy to every _build/mac/<config>/ directory that exists.
# make compile-mac outputs there, and HyperXTalk run from _build/ loads bundles
# from that location — without this, a stale bundle without HXT_MYSQL_STATIC
# would be used instead of the freshly-built one.
for CONFIG in Debug Release Fast; do
  BUILD_DIR="_build/mac/${CONFIG}"
  if [ -d "${BUILD_DIR}" ]; then
    ditto "$BUILT" "${BUILD_DIR}/dbmysql.bundle" 2>/dev/null && \
      echo "✓ _build/mac/${CONFIG}/dbmysql.bundle" || true
    # Also update inside the app bundle if it's there
    APP_DRIVERS="${BUILD_DIR}/HyperXTalk.app/Contents/Tools/Externals/Database Drivers"
    APP_RT="${BUILD_DIR}/HyperXTalk.app/Contents/Tools/Runtime/Mac OS X/arm64/Externals/Database Drivers"
    [ -d "${APP_DRIVERS}" ] && ditto "$BUILT" "${APP_DRIVERS}/dbmysql.bundle" 2>/dev/null && \
      echo "✓ _build/mac/${CONFIG}/.app/.../Database Drivers" || true
    [ -d "${APP_RT}" ] && ditto "$BUILT" "${APP_RT}/dbmysql.bundle" 2>/dev/null || true
  fi
done
echo ""
echo "Done."
if [ "$MYSQL_BAKED_IN" -eq 1 ]; then
  echo "MySQL C API is baked into dbmysql.bundle."
  echo "No system MySQL installation required for connections."
else
  echo "Using dynamic MySQL. Users need MySQL client installed to connect."
  echo "Run sh prebuilt/scripts/build-libmysql-mac-arm64.sh to bake it in."
fi