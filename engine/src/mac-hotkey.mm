/* Copyright (C) 2003-2015 LiveCode Ltd.

This file is part of LiveCode.

LiveCode is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

//
// macOS global hotkey backend using Carbon RegisterEventHotKey.
//
// Architecture:
//   A dedicated background pthread runs RunCurrentEventLoop() with its own
//   per-thread Carbon event dispatcher.  RegisterEventHotKey targets that
//   thread's dispatcher, so hotkey events are delivered on the background
//   thread regardless of what run-loop mode the main thread is currently in.
//
//   HyperXTalk uses a custom Cocoa event loop (it does NOT call [NSApp run]).
//   It alternates between:
//     - non-blocking:  NSAnyEventMask      + NSDefaultRunLoopMode
//     - blocking:      NSApplicationDefinedMask + NSEventTrackingRunLoopMode
//   Carbon's per-thread dispatcher is only serviced during NSDefaultRunLoopMode
//   iterations, so a naive single-thread approach fires only once.  Running
//   Carbon on a dedicated thread eliminates that dependency entirely.
//
//   When a hotkey fires on the background thread the handler uses
//   CFRunLoopPerformBlock(main_loop, kCFRunLoopCommonModes, ...) +
//   CFRunLoopWakeUp() to call MCHotkeyDispatchFired on the main thread.
//   kCFRunLoopCommonModes covers both NSDefaultRunLoopMode and
//   NSEventTrackingRunLoopMode, so delivery is not blocked by the engine's
//   NSEventTrackingRunLoopMode blocking wait.
//
// Requires: -framework Carbon, -framework Foundation
//           (both already available on all macOS targets)
//

#import <Foundation/Foundation.h>
#include <Carbon/Carbon.h>
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <string>

#include "prefix.h"
#include "mcstring.h"
#include "param.h"
#include "hotkey.h"
#include "globals.h"
// Forward-declare rather than including mac-internal.h (which requires
// platform-internal.h to precede it and pulls in heavy platform class headers).
void MCMacPlatformScheduleCallback(void (*)(void *), void *);

////////////////////////////////////////////////////////////////////////////////
// Per-hotkey Carbon state

struct MCMacHotkeyEntry
{
    int32_t         engine_id;
    EventHotKeyRef  hot_key_ref;
};

static MCMacHotkeyEntry *s_mac_entries     = nullptr;
static uindex_t          s_mac_entry_count = 0;

static const OSType kHotkeySignature = 'hxtk'; // HyperXTalk

////////////////////////////////////////////////////////////////////////////////
// Background Carbon event thread

static EventHandlerRef  s_handler_ref    = nullptr;
static EventHandlerUPP  s_handler_upp    = nullptr;
// The background thread's per-thread dispatcher target — stashed so the main
// thread can pass it as the 4th argument to RegisterEventHotKey.
static EventTargetRef   s_bg_target      = nullptr;
static pthread_t        s_bg_thread;
static bool             s_thread_started = false; // true once pthread_create called
// Semaphore (dispatch-based) used only during thread startup to sync s_bg_target.
static dispatch_semaphore_t s_ready_sem = nullptr;

// Carbon event handler — always called on the background thread.
static OSStatus _hot_key_handler(EventHandlerCallRef p_next,
                                 EventRef            p_event,
                                 void               *p_user_data)
{
    EventHotKeyID t_hkid;
    if (GetEventParameter(p_event,
                          kEventParamDirectObject,
                          typeEventHotKeyID,
                          nullptr,
                          sizeof(EventHotKeyID),
                          nullptr,
                          &t_hkid) != noErr)
        return eventNotHandledErr;

    if (t_hkid.signature != kHotkeySignature)
        return eventNotHandledErr;

    MCMacPlatformScheduleCallback(
        [](void *p_ctx) { MCHotkeyDispatchFired((int32_t)(uintptr_t)p_ctx); },
        (void *)(uintptr_t)t_hkid.id);
    return noErr;
}

// Background thread entry point.  Installs the Carbon handler on its own
// per-thread dispatcher, signals the main thread that setup is complete, then
// runs the Carbon event loop indefinitely so hotkeys are delivered even when
// the main thread is blocked in NSEventTrackingRunLoopMode.
static void *_carbon_thread_func(void * /*unused*/)
{
    EventTypeSpec t_spec = { kEventClassKeyboard, kEventHotKeyPressed };
    s_handler_upp = NewEventHandlerUPP(_hot_key_handler);

    // GetEventDispatcherTarget() here returns THIS thread's dispatcher, which
    // RunCurrentEventLoop() below will service continuously.
    OSStatus t_err = InstallEventHandler(
        GetEventDispatcherTarget(), s_handler_upp, 1, &t_spec, nullptr, &s_handler_ref);

    if (t_err == noErr)
        s_bg_target = GetEventDispatcherTarget();

    // Unblock _ensure_thread() — even on failure so it doesn't hang.
    dispatch_semaphore_signal(s_ready_sem);

    if (t_err != noErr)
        return nullptr;

    // Drive Carbon delivery for this thread forever (until process exit).
    // QuitEventLoop(GetCurrentEventLoop()) can be used for clean shutdown.
    RunCurrentEventLoop(kEventDurationForever);
    return nullptr;
}

// Start the background thread on first use.  Returns true if s_bg_target is
// valid (i.e. the thread started and the handler was installed).
static bool _ensure_thread()
{
    if (s_bg_target != nullptr)
        return true;
    if (s_thread_started)
        return false; // already tried and failed — don't retry

    s_ready_sem      = dispatch_semaphore_create(0);
    s_thread_started = true;

    if (pthread_create(&s_bg_thread, nullptr, _carbon_thread_func, nullptr) != 0)
    {
        dispatch_release(s_ready_sem);
        s_ready_sem = nullptr;
        return false;
    }
    pthread_detach(s_bg_thread);

    // Wait up to 2 s for the background thread to set s_bg_target.
    dispatch_semaphore_wait(s_ready_sem,
                            dispatch_time(DISPATCH_TIME_NOW, 2 * NSEC_PER_SEC));
    dispatch_release(s_ready_sem);
    s_ready_sem = nullptr;

    return s_bg_target != nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Key string parser  ("Ctrl+Shift+H" → keyCode + modifiers)

// Map a single key-name token to a Mac virtual key code.
// Returns true and sets r_key_code on success.
static bool _token_to_keycode(const char *p_token, uint32_t& r_key_code)
{
    // A-Z (case-insensitive single letter)
    if (p_token[1] == '\0')
    {
        char c = (char)toupper((unsigned char)p_token[0]);
        if (c >= 'A' && c <= 'Z')
        {
            // Carbon virtual key codes for ANSI keyboard (US layout assumed).
            // kVK_ANSI_* constants from <HIToolbox/Events.h>
            static const uint32_t s_alpha_vk[26] = {
                kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C, kVK_ANSI_D, kVK_ANSI_E,
                kVK_ANSI_F, kVK_ANSI_G, kVK_ANSI_H, kVK_ANSI_I, kVK_ANSI_J,
                kVK_ANSI_K, kVK_ANSI_L, kVK_ANSI_M, kVK_ANSI_N, kVK_ANSI_O,
                kVK_ANSI_P, kVK_ANSI_Q, kVK_ANSI_R, kVK_ANSI_S, kVK_ANSI_T,
                kVK_ANSI_U, kVK_ANSI_V, kVK_ANSI_W, kVK_ANSI_X, kVK_ANSI_Y,
                kVK_ANSI_Z
            };
            r_key_code = s_alpha_vk[c - 'A'];
            return true;
        }
        // 0-9
        if (c >= '0' && c <= '9')
        {
            static const uint32_t s_digit_vk[10] = {
                kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3, kVK_ANSI_4,
                kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7, kVK_ANSI_8, kVK_ANSI_9
            };
            r_key_code = s_digit_vk[c - '0'];
            return true;
        }
    }

    // Function keys F1-F12
    if ((p_token[0] == 'f' || p_token[0] == 'F') && p_token[1] != '\0')
    {
        int fnum = atoi(p_token + 1);
        if (fnum >= 1 && fnum <= 12)
        {
            static const uint32_t s_fn_vk[12] = {
                kVK_F1, kVK_F2, kVK_F3, kVK_F4, kVK_F5, kVK_F6,
                kVK_F7, kVK_F8, kVK_F9, kVK_F10, kVK_F11, kVK_F12
            };
            r_key_code = s_fn_vk[fnum - 1];
            return true;
        }
    }

    // Special keys
    if (strcasecmp(p_token, "space") == 0)     { r_key_code = kVK_Space;      return true; }
    if (strcasecmp(p_token, "tab") == 0)       { r_key_code = kVK_Tab;        return true; }
    if (strcasecmp(p_token, "return") == 0)    { r_key_code = kVK_Return;     return true; }
    if (strcasecmp(p_token, "enter") == 0)     { r_key_code = kVK_Return;     return true; }
    if (strcasecmp(p_token, "escape") == 0)    { r_key_code = kVK_Escape;     return true; }
    if (strcasecmp(p_token, "esc") == 0)       { r_key_code = kVK_Escape;     return true; }
    if (strcasecmp(p_token, "delete") == 0)    { r_key_code = kVK_Delete;     return true; }
    if (strcasecmp(p_token, "backspace") == 0) { r_key_code = kVK_Delete;     return true; }
    if (strcasecmp(p_token, "home") == 0)      { r_key_code = kVK_Home;       return true; }
    if (strcasecmp(p_token, "end") == 0)       { r_key_code = kVK_End;        return true; }
    if (strcasecmp(p_token, "pageup") == 0)    { r_key_code = kVK_PageUp;     return true; }
    if (strcasecmp(p_token, "pagedown") == 0)  { r_key_code = kVK_PageDown;   return true; }
    if (strcasecmp(p_token, "left") == 0)      { r_key_code = kVK_LeftArrow;  return true; }
    if (strcasecmp(p_token, "right") == 0)     { r_key_code = kVK_RightArrow; return true; }
    if (strcasecmp(p_token, "up") == 0)        { r_key_code = kVK_UpArrow;    return true; }
    if (strcasecmp(p_token, "down") == 0)      { r_key_code = kVK_DownArrow;  return true; }
    if (strcasecmp(p_token, "insert") == 0)    { r_key_code = kVK_Help;       return true; }

    return false;
}

// Parse "Ctrl+Shift+H" into (keyCode, carbonModifiers).
// Returns false and sets r_error on failure.
static bool _parse_key_string(MCStringRef p_str,
                               uint32_t&   r_key_code,
                               uint32_t&   r_modifiers,
                               std::string& r_error)
{
    char *t_cstr = nullptr;
    if (!MCStringConvertToCString(p_str, t_cstr))
    {
        r_error = "out of memory";
        return false;
    }

    r_modifiers = 0;
    r_key_code  = 0;

    // Tokenise on '+', keeping the key name as the last token.
    char *t_p = t_cstr;
    char *t_tokens[16];
    int   t_count = 0;

    while (*t_p && t_count < 15)
    {
        char *t_start = t_p;
        while (*t_p && *t_p != '+')
            t_p++;
        *t_p = '\0';
        t_tokens[t_count++] = t_start;
        if (*(t_p + 1))
            t_p++;
        else
            break;
    }

    // Last token is the key name; preceding tokens are modifiers.
    for (int i = 0; i < t_count - 1; i++)
    {
        const char *m = t_tokens[i];
        if      (strcasecmp(m, "ctrl")    == 0 || strcasecmp(m, "control") == 0)
            r_modifiers |= controlKey;
        else if (strcasecmp(m, "alt")     == 0 || strcasecmp(m, "option")  == 0)
            r_modifiers |= optionKey;
        else if (strcasecmp(m, "shift")   == 0)
            r_modifiers |= shiftKey;
        else if (strcasecmp(m, "cmd")     == 0 || strcasecmp(m, "command") == 0)
            r_modifiers |= cmdKey;
        else if (strcasecmp(m, "win")     == 0)
            r_modifiers |= cmdKey; // map Win → Cmd on macOS
        else
        {
            r_error = std::string("unknown modifier: ") + m;
            MCMemoryDeallocate(t_cstr);
            return false;
        }
    }

    if (t_count == 0 || !_token_to_keycode(t_tokens[t_count - 1], r_key_code))
    {
        r_error = std::string("unknown key: ") +
                  (t_count > 0 ? t_tokens[t_count - 1] : "(none)");
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
    if (!_ensure_thread())
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString("failed to start hotkey background thread", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    uint32_t t_key_code  = 0;
    uint32_t t_modifiers = 0;
    std::string t_error;

    if (!_parse_key_string(p_key, t_key_code, t_modifiers, t_error))
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(t_error.c_str(), t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    EventHotKeyID t_hkid = { kHotkeySignature, (UInt32)p_id };
    EventHotKeyRef t_ref = nullptr;

    // Target the background thread's dispatcher so events are delivered
    // there regardless of the main thread's current run-loop mode.
    OSStatus t_err = RegisterEventHotKey(
        t_key_code, t_modifiers, t_hkid,
        s_bg_target, 0, &t_ref);

    if (t_err != noErr)
    {
        char t_msg[64];
        snprintf(t_msg, sizeof(t_msg), "RegisterEventHotKey failed (OSStatus %d)", (int)t_err);
        MCStringRef t_mcerr;
        /* UNCHECKED */ MCStringCreateWithCString(t_msg, t_mcerr);
        MCresult->setvalueref(t_mcerr);
        MCValueRelease(t_mcerr);
        return false;
    }

    // Grow the Mac-side entry table.
    if (!MCMemoryResizeArray(s_mac_entry_count + 1, s_mac_entries, s_mac_entry_count))
    {
        UnregisterEventHotKey(t_ref);
        return false;
    }
    s_mac_entries[s_mac_entry_count - 1] = { p_id, t_ref };
    return true;
}

void MCPlatformUnregisterHotkey(int32_t p_id)
{
    for (uindex_t i = 0; i < s_mac_entry_count; i++)
    {
        if (s_mac_entries[i].engine_id == p_id)
        {
            UnregisterEventHotKey(s_mac_entries[i].hot_key_ref);
            // Compact the array.
            for (uindex_t j = i + 1; j < s_mac_entry_count; j++)
                s_mac_entries[j - 1] = s_mac_entries[j];
            s_mac_entry_count--;
            return;
        }
    }
}

void MCPlatformUnregisterAllHotkeys()
{
    for (uindex_t i = 0; i < s_mac_entry_count; i++)
        UnregisterEventHotKey(s_mac_entries[i].hot_key_ref);
    MCMemoryDeleteArray(s_mac_entries);
    s_mac_entries     = nullptr;
    s_mac_entry_count = 0;
}
