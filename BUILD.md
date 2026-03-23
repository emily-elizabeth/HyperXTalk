# HyperXTalk — macOS ARM64 Build Instructions

## Prerequisites

1. **Clone location** — the repo must live at `~/Developer/HyperXTalk`.  
   Create the folder if needed:
   ```bash
   mkdir -p ~/Developer
   git clone https://github.com/emily-elizabeth/HyperXTalk ~/Developer/HyperXTalk
   ```
   > ⚠️ Do **not** clone into `~/Documents/` or any folder synced by iCloud Drive.  
   > iCloud tags app bundles with extended attributes that break code signing.

2. **Xcode** — ensure you have the latest version installed and have accepted the license:
   ```bash
   sudo xcodebuild -license accept
   ```

3. **Homebrew** — required for `libffi`.  
   Install: https://docs.brew.sh/Installation  
   If already installed, update it:
   ```bash
   brew upgrade
   ```

4. **Python 3** — verify it is available:
   ```bash
   python3 --version   # should return Python 3.x
   ```

---

## Build Steps

Open Terminal and run the following commands in order.  
All commands assume you are in the repo root — start every session with:

```bash
cd ~/Developer/HyperXTalk
```

---

### 1. Build libffi

```bash
sh prebuilt/scripts/build-libffi-mac-arm64.sh
```

---

### 2. Build third-party libraries

```bash
REPO=~/Developer/HyperXTalk
for LIB in libskia libsqlite libxml libzip libcairo libxslt libiodbc; do
  echo "=== Building $LIB ==="
  xcodebuild \
    -project "$REPO/build-mac/livecode/thirdparty/$LIB/$LIB.xcodeproj" \
    -configuration Debug \
    -arch arm64 \
    SOLUTION_DIR="$REPO" 2>&1 | grep -E "BUILD (SUCCEEDED|FAILED)|error:"
done
```

---

### 3. Build libz

```bash
sh prebuilt/scripts/build-libz-mac-arm64.sh
```

---

### 4. Copy libraries into place

```bash
REPO=~/Developer/HyperXTalk
cp "$REPO/_build/mac/Debug/libcairo.a"  "$REPO/prebuilt/lib/mac/libcairo.a"
cp "$REPO/_build/mac/Debug/libxslt.a"   "$REPO/prebuilt/lib/mac/libxslt.a"
cp "$REPO/_build/mac/Debug/libiodbc.a"  "$REPO/prebuilt/lib/mac/libiodbc.a"
for F in "$REPO"/_build/mac/Debug/libskia*.a; do
  cp "$F" "$REPO/prebuilt/lib/mac/$(basename "$F")"
done
cp "$REPO/_build/mac/Debug/libxml.a"    "$REPO/prebuilt/lib/mac/libxml.a"
cp "$REPO/_build/mac/Debug/libzip.a"    "$REPO/prebuilt/lib/mac/libzip.a"
```

---

### 5. Build the engine

```bash
make compile-mac
```

> ⚠️ The build will end with a code signing error on the very last line.  
> This is expected — look for `** BUILD SUCCEEDED **` just above it.  
> The signing step that follows will handle this correctly.

---

### 6. Code sign mac-bin

```bash
REPO=~/Developer/HyperXTalk
MACBIN="$REPO/mac-bin"
find "$MACBIN" -not -name "*.dSYM" | while read F; do
    if [[ -f "$F" ]] && file "$F" | grep -qE "Mach-O|bundle"; then
        codesign --force --sign - "$F" 2>/dev/null && echo "Signed: $(basename $F)"
    fi
done
find "$MACBIN" -name "*.app" -exec codesign --force --options runtime \
  --entitlements "$REPO/HyperXTalk.entitlements" --sign - {} \;
echo "Done signing."
```

---

### 7. Build the installer

```bash
python3 build_installer.py
```

This produces:
```
_build/final/output/HyperXTalkInstaller-1_0_0-Mac.app
```

---

### 8. Install and run

```bash
open ~/Developer/HyperXTalk/_build/final/output/HyperXTalkInstaller-1_0_0-Mac.app
```

Follow the installer prompts. The default installation location is `/Applications`.  
Once installed, launch **HyperXTalk** from `/Applications` as normal.
