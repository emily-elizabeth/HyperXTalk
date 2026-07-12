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

#define WIN32_LEAN_AND_MEAN

// HXT: Windows SDK 10.0.26100.0 added IMAGE_POLICY_ENTRY in winnt.h which
// uses 'None' as a union member name (line 24176).  The engine defines
// #define None 0 (from globdefs.h → w32defs.h / sysdefs.h) before this
// header is processed, so 'None' macro-expands to '0' inside the struct,
// producing C2059: syntax error: 'constant'.  Undefine before the include
// and restore afterwards so engine code that relies on None == 0 is unaffected.
#ifdef None
#undef None
#endif

#include <windows.h>

// Restore None == 0 for engine code (X11-style null handle alias).
#define None 0

// w32dcw32
#include <winsock2.h>

// w32clipboard
#include <objidl.h>

// w32color
#include <icm.h>

// w32dcs
#include <mmsystem.h>

// w32dnd
#include <shlguid.h>

// w32icon
#include <shellapi.h>

// w32printer
#include <commdlg.h>
#include <cderr.h>
#include <winspool.h>

//
extern HINSTANCE MChInst;

#undef GetCurrentTime
// Undef GetObject because GetObjectW was called instead of MCExecContext::GetObject()
#undef GetObject
