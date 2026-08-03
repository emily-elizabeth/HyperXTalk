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
# Documentation rules
################################################################
# This section is based on the `buildbot.mk` file
# from the original open source LiveCode repository

BUILD_STABILITY ?= development
# stable,maintenance,development,beta

BUILD_PLATFORM ?= $(guess_platform)

BUILDTOOL_STACK = builder/builder_tool.livecodescript

# There is no MacOS ARM64 package for wkhtmltopdf.  Use following instructions to install X86_64 version.
# https://rootlevel.in/blog/how-to-5/install-wkhtmltopdf-on-macos-for-odoo-23#blog_post_comment_quote

WKHTMLTOPDF ?= $(shell which wkhtmltopdf 2>/dev/null)

# Those directories are given to the tool builder, and they might get passed
# (like private-dir) to engine functions, to which a path relative to this file
# becomes invalid).
top_src_dir=${PWD}
engine_dir=${top_src_dir}
output_dir=${top_src_dir}
work_dir=${top_src_dir}/_cache/builder_tool
bin_dir = ${top_src_dir}/$(BUILD_PLATFORM)-bin
docs_source_dir = ${top_src_dir}/docs
docs_build_dir = ${top_src_dir}/_build/docs-build

ifeq ($(BUILD_PLATFORM),mac)
	HYPERXTALK = $(bin_dir)/HyperXTalk.app/Contents/MacOS/HyperXTalk
	buildtool_platform = mac
else ifeq ($(BUILD_PLATFORM),linux-x86)
	HYPERXTALK = $(bin_dir)/HyperXTalk
	buildtool_platform = linux
else ifeq ($(BUILD_PLATFORM),linux-x86_64)
	HYPERXTALK = $(bin_dir)/HyperXTalk
	buildtool_platform = linux
endif

buildtool_command = $(HYPERXTALK) -ui $(BUILDTOOL_STACK) \
	--build $(BUILD_STABILITY) \
	--engine-dir ${engine_dir} --output-dir ${docs_build_dir} --work-dir ${work_dir}

build-docs: build-docs-api build-docs-guide

build-docs-api:
	mkdir -p $(docs_build_dir)
	$(buildtool_command) --platform $(buildtool_platform) \
		--stage docs \
		--built-docs-dir $(docs_build_dir)
	  
build-notes:
	WKHTMLTOPDF=$(WKHTMLTOPDF) \
	$(buildtool_command) --platform $(buildtool_platform) \
		--stage notes --warn-as-error \
		--built-docs-dir $(docs_build_dir)

build-docs-guide:
	WKHTMLTOPDF=$(WKHTMLTOPDF) \
	$(buildtool_command) --platform $(buildtool_platform) \
		--stage guide --warn-as-error \
		--built-docs-dir $(docs_build_dir)
