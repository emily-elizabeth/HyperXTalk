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
EMMAKE ?= emmake

# Choose the correct build type
MODE ?= release

# Where to run the build command depends on community vs commercial
BUILD_SUBDIR := /hyperxtalk
BUILD_PROJECT := hyperxtalk

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
	rm -rf prebuilt/build
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

include Makefile.linux

################################################################
# Mac rules
################################################################

include Makefile.Mac

################################################################
# Windows rules
################################################################

include Makefile.Win

################################################################
# Emscripten rules
################################################################
include Makefile.Emscripten
