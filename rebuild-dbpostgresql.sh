#!/bin/bash
set -e
cd "$(dirname "$0")"

LIBPQ="prebuilt/lib/mac/libpq.a"
STUB_THRESHOLD=2000  # bytes — real libpq.a is hundreds of KB; stub is 736 bytes

# ── Check for a real libpq.a ──────────────────────────────────────────────────
if [ -f "$LIBPQ" ]; then
  LIB_SIZE=$(stat -f%z "$LIBPQ" 2>/dev/null || stat -c%s "$LIBPQ" 2>/dev/null || echo 0)
else
  LIB_SIZE=0
fi

if [ "$LIB_SIZE" -lt "$STUB_THRESHOLD" ]; then
  echo "┌──────────────────────────────────────────────────────────────────────┐"
  echo "│  PostgreSQL client library is a stub (${LIB_SIZE} bytes).                   │"
  echo "│  Run this first to bake in libpq (requires brew install libpq):     │"
  echo "│    sh prebuilt/scripts/build-libpq-mac-arm64.sh                     │"
  echo "└──────────────────────────────────────────────────────────────────────┘"
  echo ""
  echo "Continuing with stub — bundle will link against system libpq.dylib"
  echo "(requires a PostgreSQL client installation to connect)."
  echo ""
  PQ_BAKED_IN=0
else
  SIZE_KB=$((LIB_SIZE / 1024))
  echo "✓ libpq.a present (${SIZE_KB} KB) — PostgreSQL client will be baked in"
  PQ_BAKED_IN=1
fi

echo ""
echo "=== Building dbpostgresql.bundle ==="
xcodebuild \
  -project build-mac/hyperxtalk/revdb/revdb.xcodeproj \
  -target dbpostgresql \
  -configuration Release \
  2>&1 | tail -20

# Find the built bundle
BUILT=$(find _build/mac/Release -name "dbpostgresql.bundle" -type d 2>/dev/null | head -1)
if [ -z "$BUILT" ]; then
  echo "ERROR: Could not find built dbpostgresql.bundle"
  echo "Search paths tried: _build/mac/Release/"
  exit 1
fi
echo "Built: $BUILT"

# Verify the build contains the expected content
if [ "$PQ_BAKED_IN" -eq 1 ]; then
  # With static libpq: PQconnectdb should be defined in the bundle itself
  nm "$BUILT/Contents/MacOS/dbpostgresql" 2>/dev/null | grep -q " T _PQconnectdb" && \
    echo "✓ PQconnectdb present in binary (static)" || \
    echo "✗ WARNING: PQconnectdb not found as defined symbol — static linking may have failed"
else
  # With stub: symbols will appear as undefined (dynamic)
  nm "$BUILT/Contents/MacOS/dbpostgresql" 2>/dev/null | grep -q " U _PQconnectdb" && \
    echo "✓ PQconnectdb present as external reference (dynamic)" || \
    echo "✗ WARNING: PQconnectdb reference not found"
fi

echo ""
echo "=== Deploying ==="
DRIVERS="mac-bin/HyperXTalk.app/Contents/Tools/Externals/Database Drivers"
RT_DRIVERS="mac-bin/HyperXTalk.app/Contents/Tools/Runtime/Mac OS X/arm64/Externals/Database Drivers"

ditto "$BUILT" "mac-bin/dbpostgresql.bundle"
ditto "$BUILT" "$DRIVERS/dbpostgresql.bundle"
ditto "$BUILT" "$RT_DRIVERS/dbpostgresql.bundle"
echo "✓ mac-bin"

for CONFIG in Debug Release Fast; do
  BUILD_DIR="_build/mac/${CONFIG}"
  if [ -d "${BUILD_DIR}" ]; then
    ditto "$BUILT" "${BUILD_DIR}/dbpostgresql.bundle" 2>/dev/null && \
      echo "✓ _build/mac/${CONFIG}/dbpostgresql.bundle" || true
    APP_DRIVERS="${BUILD_DIR}/HyperXTalk.app/Contents/Tools/Externals/Database Drivers"
    APP_RT="${BUILD_DIR}/HyperXTalk.app/Contents/Tools/Runtime/Mac OS X/arm64/Externals/Database Drivers"
    [ -d "${APP_DRIVERS}" ] && ditto "$BUILT" "${APP_DRIVERS}/dbpostgresql.bundle" 2>/dev/null && \
      echo "✓ _build/mac/${CONFIG}/.app/.../Database Drivers" || true
    [ -d "${APP_RT}" ] && ditto "$BUILT" "${APP_RT}/dbpostgresql.bundle" 2>/dev/null || true
  fi
done
echo ""
echo "Done."
if [ "$PQ_BAKED_IN" -eq 1 ]; then
  echo "PostgreSQL client (libpq) is baked into dbpostgresql.bundle."
  echo "No system PostgreSQL installation required for connections."
else
  echo "Using dynamic libpq. Users need a PostgreSQL client installed to connect."
  echo "Run sh prebuilt/scripts/build-libpq-mac-arm64.sh to bake it in."
fi
