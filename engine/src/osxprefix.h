/* Copyright (C) 2003-2015 LiveCode Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include "globdefs.h"

#include <CoreGraphics/CoreGraphics.h>

// ARM/modern macOS: Carbon/Carbon.h removed.
// Resource fork APIs (MCS_mac_openresourcefile_with_path, MCS_mac_closeresourcefile)
// have been removed - FSOpenResFile and related APIs are unavailable on arm64.
// AppleEvent types are available via <CoreServices/CoreServices.h> where needed.

// ARM64/modern macOS: Carbon HITheme types are unavailable.
// Provide minimal stubs so legacy osx* files compile.
#if defined(__arm64__) || defined(__aarch64__)

// HIToolbox / Carbon theme stubs
typedef UInt32 ThemeTrackKind;
typedef UInt32 ThemeDrawState;
typedef UInt32 ThemeButtonKind;
typedef UInt32 ThemeButtonAdornment;
typedef UInt32 ThemeButtonValue;
typedef CGRect HIRect;

typedef struct {
    CGFloat version;
    ThemeTrackKind kind;
    HIRect bounds;
    SInt32 min;
    SInt32 max;
    SInt32 value;
    UInt32 attributes;
    ThemeDrawState enableState;
    SInt32 trackInfo;
} HIThemeTrackDrawInfo;

typedef struct {
    CGFloat version;
    ThemeButtonKind kind;
    ThemeDrawState state;
    ThemeButtonValue value;
    ThemeButtonAdornment adornment;
} HIThemeButtonDrawInfo;

#endif // __arm64__
