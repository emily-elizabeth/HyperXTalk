##
## Copyright (C) 2012 The Android Open Source Project
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##      http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

HARFBUZZ_SRC_FILES = \
	src/hb-blob.cc \
	src/hb-buffer-serialize.cc \
	src/hb-buffer.cc \
	src/hb-common.cc \
	src/hb-fallback-shape.cc \
	src/hb-face.cc \
	src/hb-font.cc \
	src/hb-icu.cc \
	src/hb-ot-tag.cc \
	src/hb-set.cc \
	src/hb-shape.cc \
	src/hb-shape-plan.cc \
	src/hb-shaper.cc \
	src/hb-unicode.cc \
	src/hb-warning.cc \
	src/hb-ot-layout.cc \
	src/hb-ot-map.cc \
	src/hb-ot-shape.cc \
	src/hb-ot-shape-complex-arabic.cc \
	src/hb-ot-shape-complex-default.cc \
	src/hb-ot-shape-complex-hangul.cc \
	src/hb-ot-shape-complex-hebrew.cc \
	src/hb-ot-shape-complex-indic.cc \
	src/hb-ot-shape-complex-indic-table.cc \
	src/hb-ot-shape-complex-myanmar.cc \
	src/hb-ot-shape-complex-sea.cc \
	src/hb-ot-shape-complex-thai.cc \
	src/hb-ot-shape-complex-tibetan.cc \
	src/hb-ot-shape-normalize.cc \
	src/hb-ot-shape-fallback.cc

#############################################################
#   build the harfbuzz static library
#

TARGET_PLATFORM=android-8

LOCAL_MODULE:= libharfbuzz

LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES:= \
	$(HARFBUZZ_SRC_FILES) \
	src/hb-ucdn.cc \
	src/hb-ucdn/ucdn.c

LOCAL_CPP_EXTENSION     := .cc

LOCAL_C_INCLUDES        := \
  $(LOCAL_PATH)/../libfreetype/include \
  $(LOCAL_PATH)/../../prebuilt/include \
  $(LOCAL_PATH)/hb-ucdn
  
LOCAL_CFLAGS += -DHB_NO_MT -DHAVE_OT -DHAVE_UCDN -DHAVE_FREETYPE
LOCAL_EXPORT_LDLIBS := -L$(LOCAL_PATH)/../prebuilt/lib/android/armv6 -licui18n -licuio -licule -liculx -licuuc -licudata

include $(BUILD_STATIC_LIBRARY)

