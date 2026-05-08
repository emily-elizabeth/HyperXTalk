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

//
// Cross-platform global hotkey support.
//
// Usage from HyperXTalk scripts:
//
//   registerHotkey "Ctrl+Shift+H", "myHandler"
//   -- Registers a system-wide hotkey.  When the combination is pressed,
//   -- the message <myHandler> is sent to the current card of the default
//   -- stack and bubbles up the normal message path.
//   -- Registering the same key again replaces the previous handler.
//
//   unregisterHotkey "Ctrl+Shift+H"
//   -- Removes a previously registered hotkey.  No-op if not registered.
//
//   unregisterAllHotkeys
//   -- Removes every hotkey registered in the current session.
//
// Key string format:
//   Modifiers (case-insensitive) separated by "+" followed by the key name.
//   Recognised modifiers:  Ctrl, Control, Alt, Option, Shift, Cmd, Command, Win
//   Key names: A-Z, 0-9, F1-F12, Space, Tab, Return, Enter, Escape, Esc,
//              Delete, Backspace, Home, End, PageUp, PageDown,
//              Left, Right, Up, Down, Insert
//
//   Examples:
//     "Ctrl+Shift+H"
//     "Alt+F4"
//     "Cmd+Option+Space"
//

#ifndef HOTKEY_H
#define HOTKEY_H

#include "mcstring.h"

// ── Platform entry points (implemented in mac/w32/lnx-hotkey.*) ──────────────

// Register a global hotkey.  p_key is a human-readable combination string
// (see format above).  p_id is the engine-assigned integer identifier that
// will be passed back to MCHotkeyDispatchFired() when the key is pressed.
//
// Returns true on success.  On failure, sets the result with a human-readable
// error string and returns false.
bool MCPlatformRegisterHotkey(MCStringRef p_key, int32_t p_id);

// Unregister a previously registered hotkey by engine ID.
// No-op if p_id is not currently registered.
void MCPlatformUnregisterHotkey(int32_t p_id);

// Unregister every registered hotkey.
void MCPlatformUnregisterAllHotkeys();

// ── Engine callbacks (called on the main thread by platform code) ─────────────

// Called when a registered hotkey combination is pressed.
// Looks up the handler name associated with p_id and sends it to the
// current card of the default stack.
void MCHotkeyDispatchFired(int32_t p_id);

#endif // HOTKEY_H
