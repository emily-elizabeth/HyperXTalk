/*
Copyright (C) 2026 HyperXTalk

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

HyperXTalk is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
HyperXTalk. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <windows.h>

// ── Worker → main thread messages ────────────────────────────────────────────
//
// Posted by the SAPI worker thread via PostThreadMessageW(s_main_thread_id, ...)
// so that msg.hwnd == NULL.  DispatchMessageW silently drops NULL-hwnd messages,
// so MCScreenDC::handle() in w32dcw32.cpp must catch them explicitly — exactly
// as it does for WM_HOTKEY — before the DispatchMessageW branch.
//
// lParam for _VOICE_COMMAND, _UNRECOGNIZED, and _FAILED is a retained
// MCStringRef; MCPlatformHandleSpeechThreadMessage releases it.

#define WM_SPH_VOICE_COMMAND  (WM_APP + 100) // lParam = MCStringRef
#define WM_SPH_WAKE_WORD      (WM_APP + 101)
#define WM_SPH_TIMEOUT        (WM_APP + 102)
#define WM_SPH_UNRECOGNIZED   (WM_APP + 103) // lParam = MCStringRef
#define WM_SPH_STARTED        (WM_APP + 104)
#define WM_SPH_FAILED         (WM_APP + 105) // lParam = MCStringRef

// Inclusive range guard used in MCScreenDC::handle() to identify speech messages.
// WM_APP+106..109 are reserved for future expansion.
#define WM_SPH_MAIN_FIRST     (WM_APP + 100)
#define WM_SPH_MAIN_LAST      (WM_APP + 109)

// Handle a worker→main speech message on the main thread.
// Called from MCScreenDC::handle() when msg.hwnd == NULL and
// msg.message is in [WM_SPH_MAIN_FIRST, WM_SPH_MAIN_LAST].
void MCPlatformHandleSpeechThreadMessage(UINT p_msg, WPARAM p_wp, LPARAM p_lp);
