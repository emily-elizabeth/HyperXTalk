# HyperXTalk — Agent Context

Cross-platform x-talk engine/IDE (LiveCode fork).  Repo uses GYP + native build tools (Xcode/MSBuild/Make).  Heavy on macOS ARM64; Linux x86_64 and Windows x86_64 are CI-supported.

## macOS (primary platform)

### Exact build order
```bash
make prebuilt-mac    # ~10 min; idempotent
make config-mac      # GYP → Xcode projects
make compile-mac     # xcodebuild + ad-hoc signing
make package-mac-bin # assembles mac-bin/HyperXTalk.app
```

### Prerequisites
- Repo **must** live at `~/Developer/HyperXTalk`.  Do NOT use `~/Documents/` — iCloud extended attributes break code signing.
- Xcode (latest) with accepted licence: `sudo xcodebuild -license accept`
- Homebrew: `brew install openssl@3 libpq mysql-client pixman meson ninja pkg-config`
- Java JDK (arm64): `brew install --cask temurin` — `config.sh` auto-discovers it; no `JAVA_HOME` needed.
- Python 3 must be available.

### Non-obvious macOS gotchas

**Python 2 symlink**
Some scripts (e.g. ICU data generation) still call `python`.  On modern macOS:
```bash
sudo ln -s /usr/bin/python3 /usr/local/bin/python
```

**libffi**
The prebuilt `libffi.a` for ARM64 is built from vendored source, not fetched:
```bash
sh prebuilt/scripts/build-libffi-mac-arm64.sh
```

**server-community must be signed before extension script phases**
`make compile-mac` pre-builds `kernel-server` and ad-hoc signs it with Hardened Runtime before the main xcodebuild.  Unsigned = AMFI ignores `com.apple.security.cs.allow-jit` → libffi `MAP_JIT` returns `EPERM` → crash (signal 11).

**Code signing: NEVER use `--deep`**
On macOS 26+ `--deep` re-signs nested executables with `--options runtime` and produces page-hash mismatches → `SIGKILL` (CODESIGNING Code 2).  The Makefile signs inside-out (frameworks/dylibs first with bare `--sign`, then the app bundle with `--options runtime`).

**DB drivers are separate rebuild scripts**
If prebuilt stubs were used initially, bake in real libraries later:
```bash
sh rebuild-dbpostgresql.sh   # needs brew libpq
sh rebuild-dbmysql.sh        # needs brew mysql-client
```

**OpenSSL 3.x API renames**
Prebuilt headers still declare OpenSSL 1.x names.  If building from a very clean state you may need to update:
- `prebuilt/include/openssl/evp.h` — add `#define EVP_CIPHER_CTX_block_size EVP_CIPHER_CTX_get_block_size` (and key_length variants)
- `prebuilt/include/openssl/ssl.h` — add `#define SSL_get_peer_certificate SSL_get1_peer_certificate`
- `thirdparty/libopenssl/ssl.stubs` — update the four corresponding entries, then delete stale `_build/mac/Debug/libopenssl_stubs.a` to force regeneration.

### Running tests (macOS)
```bash
make check-mac                 # C++ + engine + IDE + extension tests
make -C tests bin_dir=../mac-bin check        # engine tests only
make -C ide/tests bin_dir=../../mac-bin check   # IDE tests only
make -C extensions bin_dir=../mac-bin check   # extension tests only
```

## Linux x86_64

### Build
```bash
./config.sh --platform linux-x86_64
make compile-linux-x86_64
```

### Prerequisites (Ubuntu/Debian)
```bash
sudo apt-get install build-essential python3 pkg-config \
  libssl-dev libcurl4-openssl-dev libgtk-3-dev libglib2.0-dev \
  libpng-dev libjpeg-dev libfreetype6-dev libfontconfig1-dev \
  libx11-dev libxext-dev libxrender-dev libxinerama-dev \
  libxrandr-dev libxcb1-dev libxv-dev libdbus-1-dev libcups2-dev \
  libgif-dev libpcre3-dev zlib1g-dev default-jdk
```
Uses system `libmysql` and `libpq` (`-Duse_system_libmysql=1 -Duse_system_libpq=1`).

### Running tests
```bash
make check-linux-x86_64
# or focused:
make -C tests bin_dir=../linux-x86_64-bin check
make -C ide/tests bin_dir=../../linux-x86_64-bin check
make -C extensions bin_dir=../linux-x86_64-bin check
```

## Windows x86_64

### Build
Use the batch files (run in cmd/pwsh, not Make directly):
```cmd
build-thirdparty-x64.bat   # may fail on first clean run; continue-on-error is OK
build-engine-x64.bat       # builds Debug (required by Release bootstrap)
build-release-x64.bat      # final Release output
```

### Non-obvious Windows gotchas
- WebView2 NuGet package is downloaded manually in CI (not via `nuget restore`).  See `.github/workflows/build.yml` for the PowerShell fetch/expand logic.
- `thirdparty/libcairo/src/config.h` must be copied from `config.h.win32`.
- `thirdparty/libcairo/src/cairo-features.h` must be overwritten with the Windows backend macros (see CI workflow).
- ATL headers may need a props-file injection (`ForceImportBeforeCppTargets`) if the v142 ATL component is missing but v143 ATL is present.
- Scoop is used in CI for MySQL/PostgreSQL client libraries.

## Project structure

| Directory | What it is |
|-----------|------------|
| `engine/` | Core C++ engine (entry: `engine/src/`) |
| `ide/` | IDE toolset, plugins, runtime — **git submodule** (`livecode-ide`) |
| `thirdparty/` | Vendored deps — **git submodule** (`livecode-thirdparty`) |
| `prebuilt/` | Prebuilt static libs/includes — **git submodule** (`livecode-prebuilt`) |
| `toolchain/` | LCB compiler (`lc-compile`), runner (`lc-run`), FFI-Java compiler |
| `extensions/` | LCB widgets, libraries, script libraries |
| `tests/` | Test runners and LCS/LCB/compiler/parser tests |
| `libfoundation/` | Core C++ runtime foundation (strings, streams, FFI, JNI) |
| `libgraphics/` | Skia/Cairo-based graphics engine |
| `libexternal/` | C external API bridge |
| `revdb/`, `revbrowser/`, `revzip/`, `revxml/`, `revspeech/` | External bundles (database, browser, zip, xml, speech) |

## Build system basics

- `config.py` / `config.sh` is the GYP wrapper.  It guesses platform/arch, validates Xcode/Android/Windows tools, and invokes `gyp/`.
- `Makefile.common` sets `BUILDTYPE` from `MODE` (`debug` → `Debug`, `release` → `Release`, `fast` → `Fast`).
- `guess_platform` resolves to `mac` on Darwin, `linux-x86_64` (etc.) on Linux.
- Build outputs:
  - macOS: `_build/mac/$(BUILDTYPE)/` and `mac-bin/`
  - Linux: `linux-$(ARCH)-bin/` and `build-linux-$(ARCH)/`
  - Windows: `build-win-x86_64/livecode/`

## Test suite anatomy

The test harness is in `tests/Makefile` and supports several targets:
- `lcs-check` — LiveCode Script engine tests (uses `standalone-community`/`LiveCode-Community`)
- `lcs-server-check` — Server engine tests (uses `server-community`)
- `lcb-check` — LiveCode Builder module tests (uses `lc-run`)
- `compiler-check` — LCB compiler tests
- `lcs-parser-check` — LCS parser tests
- `lc-run-check` — `lc-run` runtime tests
- `lc-compile-ffi-java-check` — Java FFI binding tests

Top-level aggregates (`make check-<platform>`) run C++ unit tests first, then `check-common-<platform>` which runs engine + IDE + extension tests in that order.

## Version

`version` file is the source of truth.  To bump: `make bump-revision` (updates `BUILD_REVISION` and `BUILD_LONG_VERSION`).

## CI / Packaging

- GitHub Actions in `.github/workflows/build.yml` builds macOS ARM64, Linux x86_64 (AppImage), and Windows x86_64.
- macOS packaging produces `mac-bin/HyperXTalk.app` (self-contained, no installer).
- Linux packaging produces an AppImage via `packaging/linux/build-appimage.sh`.
- Release artifacts are zipped and attached to GitHub Releases on `v*` tags.
