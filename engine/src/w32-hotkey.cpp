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

//
// Windows global hotkey backend using RegisterHotKey / WM_HOTKEY.
//
// RegisterHotKey() is called with hwnd = NULL so that WM_HOTKEY is posted to
// the calling thread's message queue (msg.hwnd = NULL) rather than to a
// specific window.  The engine's handle() loop in w32dcw32.cpp catches
// WM_HOTKEY explicitly before the DispatchMessageW branch — DispatchMessageW
// does NOT dispatch NULL-hwnd (thread-queue) messages to any WndProc.
//
// No extra privileges are required; the API works in normal user processes.
// MOD_NOREPEAT (0x4000) is set to avoid repeated firing while the key is held.
//

#include "prefix.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "globals.h"
#include "variable.h"
#include "hotkey.h"

////////////////////////////////////////////////////////////////////////////////
// Per-hotkey Windows state

struct MCW32HotkeyEntry
{
    int32_t engine_id;
    int     atom_id;   // same as the wParam in WM_HOTKEY
};

static MCW32HotkeyEntry *s_w32_entries     = nullptr;
static uindex_t          s_w32_entry_count = 0;

// We use the engine ID directly as the hotkey atom ID passed to RegisterHotKey,
// so WM_HOTKEY's wParam == engine_id.  (IDs 0–0xBFFF are valid for RegisterHotKey.)

////////////////////////////////////////////////////////////////////////////////
// Key string parser  ("Ctrl+Shift+H" → UINT modifiers, UINT vkCode)

static bool _token_to_vk(const char *p_token, UINT& r_vk)
{
    // Single letter A-Z
    if (p_token[1] == '\0')
    {
        char c = (char)toupper((unsigned char)p_token[0]);
        if (c >= 'A' && c <= 'Z') { r_vk = (UINT)c;          return true; }
        if (c >= '0' && c <= '9') { r_vk = (UINT)c;          return true; }
    }

    // Function keys F1-F12
    if ((p_token[0] == 'f' || p_token[0] == 'F') && p_token[1] != '\0')
    {
        int fnum = atoi(p_token + 1);
        if (fnum >= 1 && fnum <= 12) { r_vk = (UINT)(VK_F1 + fnum - 1); return true; }
    }

    if (_stricmp(p_token, "space")     == 0) { r_vk = VK_SPACE;   return true; }
    if (_stricmp(p_token, "tab")       == 0) { r_vk = VK_TAB;     return true; }
    if (_stricmp(p_token, "return")    == 0) { r_vk = VK_RETURN;  return true; }
    if (_stricmp(p_token, "enter")     == 0) { r_vk = VK_RETURN;  return true; }
    if (_stricmp(p_token, "escape")    == 0) { r_vk = VK_ESCAPE;  return true; }
    if (_stricmp(p_token, "esc")       == 0) { r_vk = VK_ESCAPE;  return true; }
    if (_stricmp(p_token, "delete")    == 0) { r_vk = VK_DELETE;  return true; }
    if (_stricmp(p_token, "backspace") == 0) { r_vk = VK_BACK;    return true; }
    if (_stricmp(p_token, "home")      == 0) { r_vk = VK_HOME;    return true; }
    if (_stricmp(p_token, "end")       == 0) { r_vk = VK_END;     return true; }
    if (_stricmp(p_token, "pageup")    == 0) { r_vk = VK_PRIOR;   return true; }
    if (_stricmp(p_token, "pagedown")  == 0) { r_vk = VK_NEXT;    return true; }
    if (_stricmp(p_token, "left")      == 0) { r_vk = VK_LEFT;    return true; }
    if (_stricmp(p_token, "right")     == 0) { r_vk = VK_RIGHT;   return true; }
    if (_stricmp(p_token, "up")        == 0) { r_vk = VK_UP;      return true; }
    if (_stricmp(p_token, "down")      == 0) { r_vk = VK_DOWN;    return true; }
    if (_stricmp(p_token, "insert")    == 0) { r_vk = VK_INSERT;  return true; }

    return false;
}

static bool _parse_key_string(MCStringRef p_str,
                               UINT&       r_modifiers,
                               UINT&       r_vk,
                               char*       r_error,
                               size_t      p_error_len)
{
    char *t_cstr = nullptr;
    if (!MCStringConvertToCString(p_str, t_cstr))
    {
        strncpy(r_error, "out of memory", p_error_len);
        return false;
    }

    r_modifiers = MOD_NOREPEAT;  // don't fire repeatedly while key is held
    r_vk        = 0;

    char *t_tokens[16];
    int   t_count = 0;
    char *t_p     = t_cstr;

    while (*t_p && t_count < 15)
    {
        char *t_start = t_p;
        while (*t_p && *t_p != '+') t_p++;
        *t_p = '\0';
        t_tokens[t_count++] = t_start;
        if (*(t_p + 1)) t_p++;
        else            break;
    }

    for (int i = 0; i < t_count - 1; i++)
    {
        const char *m = t_tokens[i];
        if      (_stricmp(m, "ctrl")    == 0 || _stricmp(m, "control") == 0)
            r_modifiers |= MOD_CONTROL;
        else if (_stricmp(m, "alt")     == 0 || _stricmp(m, "option")  == 0)
            r_modifiers |= MOD_ALT;
        else if (_stricmp(m, "shift")   == 0)
            r_modifiers |= MOD_SHIFT;
        else if (_stricmp(m, "win")     == 0 || _stricmp(m, "cmd")     == 0 ||
                 _stricmp(m, "command") == 0)
            r_modifiers |= MOD_WIN;
        else
        {
            _snprintf(r_error, p_error_len, "unknown modifier: %s", m);
            MCMemoryDeallocate(t_cstr);
            return false;
        }
    }

    if (t_count == 0 || !_token_to_vk(t_tokens[t_count - 1], r_vk))
    {
        _snprintf(r_error, p_error_len, "unknown key: %s",
                  t_count > 0 ? t_tokens[t_count - 1] : "(none)");
        MCMemoryDeallocate(t_cstr);
        return false;
    }

    MCMemoryDeallocate(t_cstr);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Platform entry points

bool MCPlatformRegisterHotkey(MCStringRef p_key, int32_t p_id)
{
    UINT t_mods = 0, t_vk = 0;
    char t_error[128] = {};

    if (!_parse_key_string(p_key, t_mods, t_vk, t_error, sizeof(t_error)))
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(t_error, t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    // Register as a thread-queue hotkey (hwnd = NULL).
    // WM_HOTKEY is then posted to the thread message queue (msg.hwnd = NULL)
    // rather than to a specific window.  The engine's handle() loop catches it
    // explicitly before the DispatchMessageW path, which does NOT dispatch
    // NULL-hwnd messages to any WndProc.
    // Valid atom ID range: 0x0000–0xBFFF.
    if (!RegisterHotKey(NULL, (int)p_id, t_mods, t_vk))
    {
        char t_msg[128];
        _snprintf(t_msg, sizeof(t_msg),
                  "RegisterHotKey failed (error %lu) — key may already be in use",
                  GetLastError());
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(t_msg, t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    // Grow the Windows-side entry table.
    if (!MCMemoryResizeArray(s_w32_entry_count + 1, s_w32_entries, s_w32_entry_count))
    {
        UnregisterHotKey(NULL, (int)p_id);
        return false;
    }
    s_w32_entries[s_w32_entry_count - 1] = { p_id, (int)p_id };
    return true;
}

void MCPlatformUnregisterHotkey(int32_t p_id)
{
    for (uindex_t i = 0; i < s_w32_entry_count; i++)
    {
        if (s_w32_entries[i].engine_id == p_id)
        {
            UnregisterHotKey(NULL, s_w32_entries[i].atom_id);
            for (uindex_t j = i + 1; j < s_w32_entry_count; j++)
                s_w32_entries[j - 1] = s_w32_entries[j];
            s_w32_entry_count--;
            return;
        }
    }
}

void MCPlatformUnregisterAllHotkeys()
{
    for (uindex_t i = 0; i < s_w32_entry_count; i++)
        UnregisterHotKey(NULL, s_w32_entries[i].atom_id);
    MCMemoryDeleteArray(s_w32_entries);
    s_w32_entries     = nullptr;
    s_w32_entry_count = 0;
}
