# Copyright (C) 2015 LiveCode Ltd.
#
# This file is part of LiveCode.
#
# LiveCode is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License v3 as published by the Free
# Software Foundation.
#
# LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License
# along with LiveCode.  If not see <http://www.gnu.org/licenses/>.

# Usually, you'll just want to type "make all".

################################################################

# Tools that Make calls
XCODEBUILD ?= xcodebuild
WINE ?= wine

# Choose the correct build type
MODE ?= debug

# Where to run the build command depends on community vs commercial
ifeq ($(BUILD_EDITION),commercial)
  BUILD_SUBDIR :=
  BUILD_PROJECT := livecode-commercial
else
  BUILD_SUBDIR := /livecode
  BUILD_PROJECT := livecode
endif

# Prettifying output for CI builds
XCODEBUILD_FILTER ?=

# Code-signing identity used by compile-mac, package-mac, and package-mac-bin.
# Defaults to ad-hoc ("-").  Override on the command line or via the environment:
#   make package-mac-bin CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
# or:
#   export CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
CODESIGN_IDENTITY ?= -

include Makefile.common

################################################################

.DEFAULT: all

all: all-$(guess_platform)
check: check-$(guess_platform)

# [[ MDW-2017-05-09 ]] feature_clean_target
clean-linux:
	rm -rf linux-*-bin
	rm -rf build-linux-*
	rm -rf prebuilt/fetched
	rm -rf prebuilt/include
	rm -rf prebuilt/lib
	find . -name \*.lcb | xargs touch

check-common-%:
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:start:testengine"
	@echo "TEST Engine"
endif
	$(MAKE) -C tests bin_dir=../$*-bin
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:end:testengine"
	@echo "travis_fold:start:testide"
	@echo "TEST IDE"
endif
	$(MAKE) -C ide/tests bin_dir=../../$*-bin
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:end:testide"
	@echo "travis_fold:start:testextensions"
	@echo "TEST Extensions"
endif
	$(MAKE) -C extensions bin_dir=../$*-bin
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:end:testextensions"
endif
################################################################
# Linux rules
################################################################

LINUX_ARCHS = x86_64 x86 armv6hf armv7

config-linux-%:
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:start:config"
	@echo "CONFIGURE"
endif
	./config.sh --platform linux-$*
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:end:config"
endif
	
compile-linux-%:
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:start:compile"
	@echo "COMPILE"
endif
	$(MAKE) -C build-linux-$*/livecode default
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:end:compile"
endif
	
check-linux-%:
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:start:testcpp"
	@echo "TEST C++"
endif
	$(MAKE) -C build-linux-$*/livecode check
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:end:testcpp"
endif
	$(MAKE) check-common-linux-$*

all-linux-%:
	$(MAKE) config-linux-$*
	$(MAKE) compile-linux-$*

$(addsuffix -linux,all config compile check): %: %-$(guess_linux_arch)

################################################################
# Mac rules
################################################################

config-mac:
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:start:config"
	@echo "CONFIGURE"
endif
	./config.sh --platform mac
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:end:config"
endif
	
compile-mac:
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:start:compile"
	@echo "COMPILE"
endif
	$(XCODEBUILD) -project "build-mac$(BUILD_SUBDIR)/$(BUILD_PROJECT).xcodeproj" -configuration $(BUILDTYPE) -target default \
	  $(XCODEBUILD_FILTER)
	@# Sign the app with Hardened Runtime (--options runtime) so that all
	@# embedded entitlements are actually honored by the kernel (AMFI only
	@# reads entitlements when CS_RUNTIME is set in the CodeDirectory).
	@# Without this flag, com.apple.security.cs.allow-jit is ignored, every
	@# mmap(MAP_JIT) call in libffi returns EPERM, and dlmmap_locked falls
	@# back to creating an unsigned /tmp file which syspolicyd evaluates for
	@# ~4 seconds — causing the browser-widget hang introduced by rebranding.
	@#
	@# IMPORTANT: Do NOT use --deep here.  --deep re-signs every nested
	@# executable (lc-compile, helper tools, etc.) with --options runtime,
	@# which on macOS 26+ produces page-hash mismatches and kills those
	@# tools with SIGKILL (CODESIGNING, Code 2, Invalid Page).  Instead:
	@#   1. Sign nested dylibs/frameworks from the inside out (bare --sign -)
	@#      so they retain valid signatures Xcode already gave them.
	@#   2. Sign only the top-level app bundle with runtime + entitlements.
	@# The allow-jit entitlement is checked against the calling process
	@# (HyperXTalk) so only its signature needs CS_RUNTIME.
	@if [ -f "_build/mac/$(BUILDTYPE)/HyperXTalk.app/Contents/MacOS/HyperXTalk" ]; then \
	  echo "Signing nested frameworks and dylibs (inside-out, no runtime)..."; \
	  find "_build/mac/$(BUILDTYPE)/HyperXTalk.app" \
	      \( -name "*.framework" -o -name "*.dylib" \) | \
	    sort -r | while read F; do \
	    codesign --force --sign "$(CODESIGN_IDENTITY)" "$$F" 2>/dev/null || true; \
	  done; \
	  echo "Signing HyperXTalk.app with Hardened Runtime + entitlements..."; \
	  codesign --force --sign "$(CODESIGN_IDENTITY)" \
	    --options runtime \
	    --entitlements HyperXTalk.entitlements \
	    "_build/mac/$(BUILDTYPE)/HyperXTalk.app"; \
	fi
	@# Sign external bundles that live beside the app (revbrowser.bundle,
	@# revdb.bundle, etc.).  They are loaded into the HyperXTalk process so
	@# they do not need their own CS_RUNTIME — bare --sign - is sufficient.
	@echo "Signing external bundles in _build/mac/$(BUILDTYPE)/..."
	@find "_build/mac/$(BUILDTYPE)" -maxdepth 1 \
	    \( -name "*.bundle" -o -name "*.dylib" \) | while read F; do \
	  codesign --force --sign "$(CODESIGN_IDENTITY)" "$$F" 2>/dev/null || true; \
	done
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:end:compile"
endif

check-mac:
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:start:testcpp"
	@echo "TEST C++"
endif
	$(XCODEBUILD) -project "build-mac$(BUILD_SUBDIR)/$(BUILD_PROJECT).xcodeproj" -configuration $(BUILDTYPE) -target check \
	  $(XCODEBUILD_FILTER)
ifneq ($(TRAVIS),undefined)
	@echo "travis_fold:end:testcpp"
endif
	$(MAKE) check-common-mac


all-mac:
	$(MAKE) config-mac
	$(MAKE) compile-mac
	$(MAKE) package-mac

################################################################
# Mac packaging (self-contained bundle, no installer required)
################################################################

# Paths derived from the current build type
MAC_BIN     = _build/mac/$(BUILDTYPE)
BUNDLE      = $(MAC_BIN)/HyperXTalk.app
TOOLS_DIR   = $(BUNDLE)/Contents/Tools
SUPPORT_DIR = $(BUNDLE)/Contents/Support
RUNTIME_ARM64 = $(TOOLS_DIR)/Runtime/Mac OS X/arm64

package-mac:
	@echo "=== Packaging $(BUNDLE) ==="
	@# ----------------------------------------------------------------
	@# Directory structure
	@# ----------------------------------------------------------------
	@mkdir -p "$(TOOLS_DIR)/Toolset/libraries"
	@mkdir -p "$(TOOLS_DIR)/Plugins"
	@mkdir -p "$(TOOLS_DIR)/Externals/Database Drivers"
	@mkdir -p "$(RUNTIME_ARM64)/Support"
	@mkdir -p "$(RUNTIME_ARM64)/Externals/Database Drivers"
	@mkdir -p "$(TOOLS_DIR)/Extensions"
	@mkdir -p "$(TOOLS_DIR)/Toolchain"
	@mkdir -p "$(SUPPORT_DIR)"
	@# ----------------------------------------------------------------
	@# Edition marker
	@# ----------------------------------------------------------------
	@echo "Community" > "$(TOOLS_DIR)/edition.txt"
	@# ----------------------------------------------------------------
	@# Toolset: copy the entire IDE toolset tree
	@# ----------------------------------------------------------------
	@cp -R ide/Toolset/. "$(TOOLS_DIR)/Toolset/"
	@# ----------------------------------------------------------------
	@# Toolset libraries: IDE support scripts from ide-support/
	@# ----------------------------------------------------------------
	@for f in \
	    revsblibrary revsaveasstandalone; do \
	  [ -f "ide-support/$$f.livecodescript" ] && \
	    cp "ide-support/$$f.livecodescript" "$(TOOLS_DIR)/Toolset/libraries/" \
	    || true; \
	done
	@cp -f ide-support/revdocsparser.livecodescript \
	    "$(TOOLS_DIR)/Toolset/libraries/" 2>/dev/null || true
	@# ----------------------------------------------------------------
	@# Plugins
	@# ----------------------------------------------------------------
	@[ -d ide/Plugins ] && cp -R ide/Plugins/. "$(TOOLS_DIR)/Plugins/" || true
	@# ----------------------------------------------------------------
	@# Externals (IDE runtime use)
	@# ----------------------------------------------------------------
	@for b in revbrowser revxml revspeech revzip; do \
	  [ -d "$(MAC_BIN)/$$b.bundle" ] && \
	    cp -R "$(MAC_BIN)/$$b.bundle" "$(TOOLS_DIR)/Externals/" || true; \
	done
	@[ -d "$(MAC_BIN)/revdb.bundle" ] && \
	    cp -R "$(MAC_BIN)/revdb.bundle" "$(TOOLS_DIR)/Externals/" || true
	@for b in dbmysql dbodbc dbpostgresql dbsqlite; do \
	  [ -d "$(MAC_BIN)/$$b.bundle" ] && \
	    cp -R "$(MAC_BIN)/$$b.bundle" \
	      "$(TOOLS_DIR)/Externals/Database Drivers/" || true; \
	done
	@# ----------------------------------------------------------------
	@# Externals discovery files (read by the standalone builder)
	@# ----------------------------------------------------------------
	@printf 'Speech,revspeech.bundle\nXML,revxml.bundle\nBrowser,revbrowser.bundle\nRevolution Zip,revzip.bundle\n' \
	    > "$(RUNTIME_ARM64)/Externals/Externals.txt"
	@printf 'Database,revdb.bundle\n' \
	    >> "$(RUNTIME_ARM64)/Externals/Externals.txt"
	@printf 'MySQL,dbmysql.bundle\nODBC,dbodbc.bundle\nPostgreSQL,dbpostgresql.bundle\nSQLite,dbsqlite.bundle\n' \
	    > "$(RUNTIME_ARM64)/Externals/Database Drivers/Database Drivers.txt"
	@# ----------------------------------------------------------------
	@# Runtime: arm64 standalone engine + support libraries
	@# ----------------------------------------------------------------
	@# Remove existing Standalone.app first so cp -R replaces it rather
	@# than nesting HyperXTalk-Standalone.app inside it on re-runs.
	@rm -rf "$(RUNTIME_ARM64)/Standalone.app"
	@[ -d "$(MAC_BIN)/HyperXTalk-Standalone.app" ] && \
	    cp -R "$(MAC_BIN)/HyperXTalk-Standalone.app" \
	      "$(RUNTIME_ARM64)/Standalone.app" || true
	@# Re-sign Standalone.app inside-out with hardened runtime so
	@# notarization accepts it (strips get-task-allow from debug build).
	@if [ -d "$(RUNTIME_ARM64)/Standalone.app" ]; then \
	  find "$(RUNTIME_ARM64)/Standalone.app" \
	      \( -name "*.framework" -o -name "*.dylib" \) | \
	      sort -r | while read F; do \
	    codesign --force --sign "$(CODESIGN_IDENTITY)" \
	        --options runtime \
	        --entitlements HyperXTalk.entitlements "$$F" 2>/dev/null || true; \
	  done; \
	  find "$(RUNTIME_ARM64)/Standalone.app" -name "*.bundle" | \
	      while read F; do \
	    codesign --force --sign "$(CODESIGN_IDENTITY)" \
	        --options runtime \
	        --entitlements HyperXTalk.entitlements "$$F" 2>/dev/null || true; \
	  done; \
	  codesign --force --sign "$(CODESIGN_IDENTITY)" \
	      --options runtime \
	      --entitlements HyperXTalk.entitlements \
	      "$(RUNTIME_ARM64)/Standalone.app"; \
	fi
	@[ -d "$(MAC_BIN)/revpdfprinter.bundle" ] && \
	    cp -R "$(MAC_BIN)/revpdfprinter.bundle" \
	      "$(RUNTIME_ARM64)/Support/" || true
	@[ -f "$(MAC_BIN)/revsecurity.dylib" ] && \
	    cp "$(MAC_BIN)/revsecurity.dylib" \
	      "$(RUNTIME_ARM64)/Support/" || true
	@# ----------------------------------------------------------------
	@# Toolchain: compiler, runner, LCB modules
	@# ----------------------------------------------------------------
	@[ -f "$(MAC_BIN)/lc-compile" ] && \
	    cp "$(MAC_BIN)/lc-compile" "$(TOOLS_DIR)/Toolchain/" || true
	@[ -f "$(MAC_BIN)/lc-run" ] && \
	    cp "$(MAC_BIN)/lc-run" "$(TOOLS_DIR)/Toolchain/" || true
	@[ -f "$(MAC_BIN)/lc-compile-ffi-java" ] && \
	    cp "$(MAC_BIN)/lc-compile-ffi-java" \
	      "$(TOOLS_DIR)/Toolchain/" || true
	@[ -d "$(MAC_BIN)/modules" ] && \
	    cp -R "$(MAC_BIN)/modules" "$(TOOLS_DIR)/Toolchain/" || true
	@# Re-sign toolchain tools with hardened runtime (strips get-task-allow).
	@for t in lc-compile lc-run lc-compile-ffi-java; do \
	  [ -f "$(TOOLS_DIR)/Toolchain/$$t" ] && \
	    codesign --force --sign "$(CODESIGN_IDENTITY)" \
	        --options runtime \
	        --entitlements HyperXTalk.entitlements \
	        "$(TOOLS_DIR)/Toolchain/$$t" || true; \
	done
	@# ----------------------------------------------------------------
	@# Extensions
	@# ----------------------------------------------------------------
	@[ -d "$(MAC_BIN)/packaged_extensions" ] && \
	    cp -R "$(MAC_BIN)/packaged_extensions/." \
	      "$(TOOLS_DIR)/Extensions/" || true
	@# ----------------------------------------------------------------
	@# Support: licence and about text
	@# ----------------------------------------------------------------
	@[ -f "ide/License Agreement.txt" ] && \
	    cp "ide/License Agreement.txt" "$(SUPPORT_DIR)/" || true
	@[ -f "ide/about.txt" ] && \
	    cp "ide/about.txt" "$(SUPPORT_DIR)/" || true
	@[ -f "ide/Open Source Licenses.txt" ] && \
	    cp "ide/Open Source Licenses.txt" "$(SUPPORT_DIR)/" || true
	@# ----------------------------------------------------------------
	@# Strip extended attributes (resource forks, Finder info, etc.)
	@# xattr -cr must run before codesign or signing will be rejected.
	@# ----------------------------------------------------------------
	@echo "Stripping extended attributes..."
	@xattr -cr "$(BUNDLE)"
	@# ----------------------------------------------------------------
	@# Re-sign the bundle now that new files have been added
	@# ----------------------------------------------------------------
	@echo "Re-signing bundle contents..."
	@find "$(BUNDLE)" \( -name "*.framework" -o -name "*.dylib" \) | \
	    sort -r | while read F; do \
	  codesign --force --sign "$(CODESIGN_IDENTITY)" "$$F" 2>/dev/null || true; \
	done
	@find "$(BUNDLE)" -name "*.bundle" | while read F; do \
	  codesign --force --sign "$(CODESIGN_IDENTITY)" "$$F" 2>/dev/null || true; \
	done
	@codesign --force --sign "$(CODESIGN_IDENTITY)" \
	    --options runtime \
	    --entitlements HyperXTalk.entitlements \
	    "$(BUNDLE)"
	@echo "=== Package complete: $(BUNDLE) ==="

################################################################
# mac-bin packaging (self-contained bundle from ./mac-bin)
################################################################

MACBIN_BIN       = mac-bin
MACBIN_BUNDLE    = $(MACBIN_BIN)/HyperXTalk.app
MACBIN_TOOLS     = $(MACBIN_BUNDLE)/Contents/Tools
MACBIN_SUPPORT   = $(MACBIN_BUNDLE)/Contents/Support
MACBIN_RT_ARM64  = $(MACBIN_TOOLS)/Runtime/Mac OS X/arm64

package-mac-bin:
	@echo "=== Packaging $(MACBIN_BUNDLE) ==="
	@# ----------------------------------------------------------------
	@# Directory structure
	@# ----------------------------------------------------------------
	@mkdir -p "$(MACBIN_TOOLS)/Toolset/libraries"
	@mkdir -p "$(MACBIN_TOOLS)/Plugins"
	@mkdir -p "$(MACBIN_TOOLS)/Externals/Database Drivers"
	@mkdir -p "$(MACBIN_RT_ARM64)/Support"
	@mkdir -p "$(MACBIN_RT_ARM64)/Externals/Database Drivers"
	@mkdir -p "$(MACBIN_TOOLS)/Extensions"
	@mkdir -p "$(MACBIN_TOOLS)/Toolchain"
	@mkdir -p "$(MACBIN_SUPPORT)"
	@# ----------------------------------------------------------------
	@# Edition marker
	@# ----------------------------------------------------------------
	@echo "Community" > "$(MACBIN_TOOLS)/edition.txt"
	@# ----------------------------------------------------------------
	@# Toolset: copy the entire IDE toolset tree
	@# ----------------------------------------------------------------
	@cp -R ide/Toolset/. "$(MACBIN_TOOLS)/Toolset/"
	@# ----------------------------------------------------------------
	@# Toolset libraries: IDE support scripts from ide-support/
	@# ----------------------------------------------------------------
	@for f in \
	    revsblibrary revsaveasstandalone; do \
	  [ -f "ide-support/$$f.livecodescript" ] && \
	    cp "ide-support/$$f.livecodescript" "$(MACBIN_TOOLS)/Toolset/libraries/" \
	    || true; \
	done
	@cp -f ide-support/revdocsparser.livecodescript \
	    "$(MACBIN_TOOLS)/Toolset/libraries/" 2>/dev/null || true
	@# ----------------------------------------------------------------
	@# Plugins
	@# ----------------------------------------------------------------
	@[ -d ide/Plugins ] && cp -R ide/Plugins/. "$(MACBIN_TOOLS)/Plugins/" || true
	@# ----------------------------------------------------------------
	@# Externals (IDE runtime use)
	@# ----------------------------------------------------------------
	@for b in revbrowser revxml revspeech revzip; do \
	  [ -d "$(MACBIN_BIN)/$$b.bundle" ] && \
	    cp -R "$(MACBIN_BIN)/$$b.bundle" "$(MACBIN_TOOLS)/Externals/" || true; \
	done
	@[ -d "$(MACBIN_BIN)/revdb.bundle" ] && \
	    cp -R "$(MACBIN_BIN)/revdb.bundle" "$(MACBIN_TOOLS)/Externals/" || true
	@for b in dbmysql dbodbc dbpostgresql dbsqlite; do \
	  [ -d "$(MACBIN_BIN)/$$b.bundle" ] && \
	    cp -R "$(MACBIN_BIN)/$$b.bundle" \
	      "$(MACBIN_TOOLS)/Externals/Database Drivers/" || true; \
	done
	@# ----------------------------------------------------------------
	@# Externals discovery files (read by the standalone builder)
	@# ----------------------------------------------------------------
	@printf 'Speech,revspeech.bundle\nXML,revxml.bundle\nBrowser,revbrowser.bundle\nRevolution Zip,revzip.bundle\n' \
	    > "$(MACBIN_RT_ARM64)/Externals/Externals.txt"
	@printf 'Database,revdb.bundle\n' \
	    >> "$(MACBIN_RT_ARM64)/Externals/Externals.txt"
	@printf 'MySQL,dbmysql.bundle\nODBC,dbodbc.bundle\nPostgreSQL,dbpostgresql.bundle\nSQLite,dbsqlite.bundle\n' \
	    > "$(MACBIN_RT_ARM64)/Externals/Database Drivers/Database Drivers.txt"
	@# ----------------------------------------------------------------
	@# Runtime: arm64 standalone engine + support libraries
	@# ----------------------------------------------------------------
	@# Remove existing Standalone.app first so cp -R replaces it rather
	@# than nesting HyperXTalk-Standalone.app inside it on re-runs.
	@rm -rf "$(MACBIN_RT_ARM64)/Standalone.app"
	@[ -d "$(MACBIN_BIN)/HyperXTalk-Standalone.app" ] && \
	    cp -R "$(MACBIN_BIN)/HyperXTalk-Standalone.app" \
	      "$(MACBIN_RT_ARM64)/Standalone.app" || true
	@# Re-sign Standalone.app inside-out with hardened runtime so
	@# notarization accepts it (strips get-task-allow from debug build).
	@if [ -d "$(MACBIN_RT_ARM64)/Standalone.app" ]; then \
	  find "$(MACBIN_RT_ARM64)/Standalone.app" \
	      \( -name "*.framework" -o -name "*.dylib" \) | \
	      sort -r | while read F; do \
	    codesign --force --sign "$(CODESIGN_IDENTITY)" \
	        --options runtime \
	        --entitlements HyperXTalk.entitlements "$$F" 2>/dev/null || true; \
	  done; \
	  find "$(MACBIN_RT_ARM64)/Standalone.app" -name "*.bundle" | \
	      while read F; do \
	    codesign --force --sign "$(CODESIGN_IDENTITY)" \
	        --options runtime \
	        --entitlements HyperXTalk.entitlements "$$F" 2>/dev/null || true; \
	  done; \
	  codesign --force --sign "$(CODESIGN_IDENTITY)" \
	      --options runtime \
	      --entitlements HyperXTalk.entitlements \
	      "$(MACBIN_RT_ARM64)/Standalone.app"; \
	fi
	@[ -d "$(MACBIN_BIN)/revpdfprinter.bundle" ] && \
	    cp -R "$(MACBIN_BIN)/revpdfprinter.bundle" \
	      "$(MACBIN_RT_ARM64)/Support/" || true
	@[ -f "$(MACBIN_BIN)/revsecurity.dylib" ] && \
	    cp "$(MACBIN_BIN)/revsecurity.dylib" \
	      "$(MACBIN_RT_ARM64)/Support/" || true
	@# ----------------------------------------------------------------
	@# Toolchain: compiler, runner, LCB modules
	@# ----------------------------------------------------------------
	@[ -f "$(MACBIN_BIN)/lc-compile" ] && \
	    cp "$(MACBIN_BIN)/lc-compile" "$(MACBIN_TOOLS)/Toolchain/" || true
	@[ -f "$(MACBIN_BIN)/lc-run" ] && \
	    cp "$(MACBIN_BIN)/lc-run" "$(MACBIN_TOOLS)/Toolchain/" || true
	@[ -f "$(MACBIN_BIN)/lc-compile-ffi-java" ] && \
	    cp "$(MACBIN_BIN)/lc-compile-ffi-java" \
	      "$(MACBIN_TOOLS)/Toolchain/" || true
	@[ -d "$(MACBIN_BIN)/modules" ] && \
	    cp -R "$(MACBIN_BIN)/modules" "$(MACBIN_TOOLS)/Toolchain/" || true
	@# Re-sign toolchain tools with hardened runtime (strips get-task-allow).
	@for t in lc-compile lc-run lc-compile-ffi-java; do \
	  [ -f "$(MACBIN_TOOLS)/Toolchain/$$t" ] && \
	    codesign --force --sign "$(CODESIGN_IDENTITY)" \
	        --options runtime \
	        --entitlements HyperXTalk.entitlements \
	        "$(MACBIN_TOOLS)/Toolchain/$$t" || true; \
	done
	@# ----------------------------------------------------------------
	@# Extensions
	@# ----------------------------------------------------------------
	@[ -d "$(MACBIN_BIN)/packaged_extensions" ] && \
	    cp -R "$(MACBIN_BIN)/packaged_extensions/." \
	      "$(MACBIN_TOOLS)/Extensions/" || true
	@# ----------------------------------------------------------------
	@# Support: licence and about text
	@# ----------------------------------------------------------------
	@[ -f "ide/License Agreement.txt" ] && \
	    cp "ide/License Agreement.txt" "$(MACBIN_SUPPORT)/" || true
	@[ -f "ide/about.txt" ] && \
	    cp "ide/about.txt" "$(MACBIN_SUPPORT)/" || true
	@[ -f "ide/Open Source Licenses.txt" ] && \
	    cp "ide/Open Source Licenses.txt" "$(MACBIN_SUPPORT)/" || true
	@# ----------------------------------------------------------------
	@# Strip extended attributes (resource forks, Finder info, etc.)
	@# xattr -cr must run before codesign or signing will be rejected.
	@# ----------------------------------------------------------------
	@echo "Stripping extended attributes..."
	@xattr -cr "$(MACBIN_BUNDLE)"
	@# ----------------------------------------------------------------
	@# Re-sign the bundle now that new files have been added
	@# ----------------------------------------------------------------
	@echo "Re-signing bundle contents..."
	@find "$(MACBIN_BUNDLE)" \( -name "*.framework" -o -name "*.dylib" \) | \
	    sort -r | while read F; do \
	  codesign --force --sign "$(CODESIGN_IDENTITY)" "$$F" 2>/dev/null || true; \
	done
	@find "$(MACBIN_BUNDLE)" -name "*.bundle" | while read F; do \
	  codesign --force --sign "$(CODESIGN_IDENTITY)" "$$F" 2>/dev/null || true; \
	done
	@codesign --force --sign "$(CODESIGN_IDENTITY)" \
	    --options runtime \
	    --entitlements HyperXTalk.entitlements \
	    "$(MACBIN_BUNDLE)"
	@echo "=== Package complete: $(MACBIN_BUNDLE) ==="

################################################################
# Windows rules
################################################################

config-win-%:
	./config.sh --platform win-$*

compile-win-%:
	# windows builds occur under Wine
	cd build-win-$* && $(WINE) /K ../make.cmd

check-win-%:
	# windows builds occur under Wine
	cd build-win-$* && $(WINE) /K ../make.cmd check
	$(MAKE) check-common-win-$*

all-win-%:
	$(MAKE) config-win-$*
	$(MAKE) compile-win-$*

$(addsuffix -win,all config compile): %: %-x86

# Dummy rules for Windows x86-64 builds
# TODO Replace with real rules
config-win-x86_64:
	mkdir -p build-win-x86_64
compile-win-x86_64:
	mkdir -p win-x86_64-bin
all-win-x86_64:
	$(MAKE) config-win-x86_64
	$(MAKE) compile-win-x86_64

