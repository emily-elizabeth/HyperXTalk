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
// Linux global hotkey backend using XGrabKey.
//
// Architecture:
//   A dedicated background thread opens its own X display connection and
//   calls XGrabKey on the root window for each registered hotkey.  It loops
//   on XNextEvent, and when a matching KeyPress arrives it writes the engine
//   ID into a self-pipe.
//
//   The main thread watches the read end of the pipe via g_io_add_watch(),
//   which integrates cleanly with the GLib/GDK event loop already used by
//   the Linux engine.  The GLib callback runs on the main thread, so
//   MCHotkeyDispatchFired() is always called from the correct thread.
//
// XGrabKey registers with Num Lock (Mod2) and Caps Lock (LockMask) masked
// out so the hotkey fires regardless of those lock states.
//
// Requires: -lX11  (already linked in the Linux desktop build)
//           GLib / GDK (already linked)
//

#include "prefix.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <glib.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "mcstring.h"
#include "param.h"
#include "hotkey.h"
#include "globals.h"

////////////////////////////////////////////////////////////////////////////////
// Self-pipe for main-thread dispatch

static int  s_pipe_read  = -1;
static int  s_pipe_write = -1;
static guint s_io_watch  = 0;   // GLib watch source ID

// GLib I/O callback — called on the main thread when data is ready on the pipe.
static gboolean _pipe_readable(GIOChannel * /*channel*/,
                                GIOCondition /*cond*/,
                                gpointer     /*data*/)
{
    int32_t t_id;
    while (read(s_pipe_read, &t_id, sizeof(t_id)) == (ssize_t)sizeof(t_id))
        MCHotkeyDispatchFired(t_id);
    return TRUE;  // keep the watch active
}

static bool _ensure_pipe()
{
    if (s_pipe_read >= 0)
        return true;

    int fds[2];
    if (pipe(fds) != 0)
        return false;

    // Make the read end non-blocking so the callback doesn't stall.
    fcntl(fds[0], F_SETFL, O_NONBLOCK);

    s_pipe_read  = fds[0];
    s_pipe_write = fds[1];

    // Register the read fd with GLib so it's checked in the main run loop.
    GIOChannel *t_chan = g_io_channel_unix_new(s_pipe_read);
    s_io_watch = g_io_add_watch(t_chan, G_IO_IN, _pipe_readable, nullptr);
    g_io_channel_unref(t_chan);

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Per-hotkey Linux state

struct MCLnxHotkeyEntry
{
    int32_t  engine_id;
    KeyCode  key_code;
    unsigned modifiers;  // clean modifier mask (no Num/Caps variants)
};

static MCLnxHotkeyEntry *s_lnx_entries     = nullptr;
static uindex_t          s_lnx_entry_count = 0;

// Background thread state.
static Display   *s_bg_display    = nullptr;
static Window     s_root          = None;
static pthread_t  s_thread;
static bool       s_thread_running = false;

// Mutex protecting s_lnx_entries from the background thread.
static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

// Modifier combinations to ignore (Num Lock, Caps Lock, both).
static const unsigned kIgnoredMods[] = { 0, Mod2Mask, LockMask, Mod2Mask | LockMask };

////////////////////////////////////////////////////////////////////////////////
// Background thread: blocks on XNextEvent, pipes matching IDs to the main thread.

static void *_hotkey_thread(void * /*unused*/)
{
    XEvent t_event;
    for (;;)
    {
        XNextEvent(s_bg_display, &t_event);

        if (t_event.type != KeyPress)
            continue;

        XKeyEvent *ke = &t_event.xkey;
        // Strip ignored modifier bits before comparing.
        unsigned t_clean = ke->state & ~(Mod2Mask | LockMask | Mod5Mask);

        pthread_mutex_lock(&s_mutex);
        for (uindex_t i = 0; i < s_lnx_entry_count; i++)
        {
            if (s_lnx_entries[i].key_code  == ke->keycode &&
                s_lnx_entries[i].modifiers == t_clean)
            {
                int32_t t_id = s_lnx_entries[i].engine_id;
                pthread_mutex_unlock(&s_mutex);
                (void)write(s_pipe_write, &t_id, sizeof(t_id));
                goto next_event;
            }
        }
        pthread_mutex_unlock(&s_mutex);

    next_event:;
    }
    return nullptr;
}

static bool _ensure_thread()
{
    if (s_thread_running)
        return true;

    s_bg_display = XOpenDisplay(nullptr);
    if (!s_bg_display)
        return false;

    s_root = DefaultRootWindow(s_bg_display);

    if (pthread_create(&s_thread, nullptr, _hotkey_thread, nullptr) != 0)
    {
        XCloseDisplay(s_bg_display);
        s_bg_display = nullptr;
        return false;
    }
    pthread_detach(s_thread);
    s_thread_running = true;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Key string parser  ("Ctrl+Shift+H" → X11 modifier mask + KeyCode)

static bool _token_to_keysym(const char *p_token, KeySym& r_sym)
{
    if (p_token[1] == '\0')
    {
        char c = (char)tolower((unsigned char)p_token[0]);
        if (c >= 'a' && c <= 'z') { r_sym = (KeySym)c;          return true; }
        if (c >= '0' && c <= '9') { r_sym = (KeySym)c;          return true; }
    }

    if ((p_token[0] == 'f' || p_token[0] == 'F') && p_token[1] != '\0')
    {
        int fnum = atoi(p_token + 1);
        if (fnum >= 1 && fnum <= 12) { r_sym = XK_F1 + fnum - 1; return true; }
    }

    if (strcasecmp(p_token, "space")     == 0) { r_sym = XK_space;     return true; }
    if (strcasecmp(p_token, "tab")       == 0) { r_sym = XK_Tab;       return true; }
    if (strcasecmp(p_token, "return")    == 0) { r_sym = XK_Return;    return true; }
    if (strcasecmp(p_token, "enter")     == 0) { r_sym = XK_Return;    return true; }
    if (strcasecmp(p_token, "escape")    == 0) { r_sym = XK_Escape;    return true; }
    if (strcasecmp(p_token, "esc")       == 0) { r_sym = XK_Escape;    return true; }
    if (strcasecmp(p_token, "delete")    == 0) { r_sym = XK_Delete;    return true; }
    if (strcasecmp(p_token, "backspace") == 0) { r_sym = XK_BackSpace; return true; }
    if (strcasecmp(p_token, "home")      == 0) { r_sym = XK_Home;      return true; }
    if (strcasecmp(p_token, "end")       == 0) { r_sym = XK_End;       return true; }
    if (strcasecmp(p_token, "pageup")    == 0) { r_sym = XK_Page_Up;   return true; }
    if (strcasecmp(p_token, "pagedown")  == 0) { r_sym = XK_Page_Down; return true; }
    if (strcasecmp(p_token, "left")      == 0) { r_sym = XK_Left;      return true; }
    if (strcasecmp(p_token, "right")     == 0) { r_sym = XK_Right;     return true; }
    if (strcasecmp(p_token, "up")        == 0) { r_sym = XK_Up;        return true; }
    if (strcasecmp(p_token, "down")      == 0) { r_sym = XK_Down;      return true; }
    if (strcasecmp(p_token, "insert")    == 0) { r_sym = XK_Insert;    return true; }

    return false;
}

static bool _parse_key_string(MCStringRef p_str,
                               unsigned&   r_modifiers,
                               KeyCode&    r_keycode,
                               char*       r_error,
                               size_t      p_error_len)
{
    char *t_cstr = nullptr;
    if (!MCStringConvertToCString(p_str, t_cstr))
    {
        strncpy(r_error, "out of memory", p_error_len);
        return false;
    }

    r_modifiers = 0;

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
        if      (strcasecmp(m, "ctrl")    == 0 || strcasecmp(m, "control") == 0)
            r_modifiers |= ControlMask;
        else if (strcasecmp(m, "alt")     == 0 || strcasecmp(m, "option")  == 0)
            r_modifiers |= Mod1Mask;
        else if (strcasecmp(m, "shift")   == 0)
            r_modifiers |= ShiftMask;
        else if (strcasecmp(m, "win")     == 0 || strcasecmp(m, "cmd")     == 0 ||
                 strcasecmp(m, "command") == 0)
            r_modifiers |= Mod4Mask;  // Super / Windows key
        else
        {
            snprintf(r_error, p_error_len, "unknown modifier: %s", m);
            MCMemoryDeallocate(t_cstr);
            return false;
        }
    }

    KeySym t_sym = NoSymbol;
    if (t_count == 0 || !_token_to_keysym(t_tokens[t_count - 1], t_sym))
    {
        snprintf(r_error, p_error_len, "unknown key: %s",
                 t_count > 0 ? t_tokens[t_count - 1] : "(none)");
        MCMemoryDeallocate(t_cstr);
        return false;
    }

    r_keycode = XKeysymToKeycode(s_bg_display, t_sym);
    if (r_keycode == 0)
    {
        snprintf(r_error, p_error_len,
                 "key not available on this keyboard layout: %s",
                 t_tokens[t_count - 1]);
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
    if (!_ensure_pipe())
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString("failed to create hotkey pipe", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    if (!_ensure_thread())
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString("failed to open X display for hotkey thread", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    unsigned t_mods  = 0;
    KeyCode  t_kcode = 0;
    char     t_error[128] = {};

    if (!_parse_key_string(p_key, t_mods, t_kcode, t_error, sizeof(t_error)))
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(t_error, t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    // Grab with every combination of Num Lock / Caps Lock.
    // Suppress X errors (e.g. BadAccess if another app holds the key).
    XErrorHandler t_old = XSetErrorHandler([](Display *, XErrorEvent *) -> int { return 0; });

    for (unsigned ignore : kIgnoredMods)
        XGrabKey(s_bg_display, t_kcode, t_mods | ignore,
                 s_root, True, GrabModeAsync, GrabModeAsync);

    XFlush(s_bg_display);
    XSetErrorHandler(t_old);

    // Store the entry (mutex-protected).
    pthread_mutex_lock(&s_mutex);
    bool t_ok = MCMemoryResizeArray(s_lnx_entry_count + 1,
                                    s_lnx_entries, s_lnx_entry_count);
    if (t_ok)
        s_lnx_entries[s_lnx_entry_count - 1] = { p_id, t_kcode, t_mods };
    pthread_mutex_unlock(&s_mutex);

    if (!t_ok)
    {
        for (unsigned ignore : kIgnoredMods)
            XUngrabKey(s_bg_display, t_kcode, t_mods | ignore, s_root);
        XFlush(s_bg_display);
        return false;
    }

    return true;
}

void MCPlatformUnregisterHotkey(int32_t p_id)
{
    if (!s_bg_display)
        return;

    pthread_mutex_lock(&s_mutex);
    for (uindex_t i = 0; i < s_lnx_entry_count; i++)
    {
        if (s_lnx_entries[i].engine_id == p_id)
        {
            KeyCode  t_kcode = s_lnx_entries[i].key_code;
            unsigned t_mods  = s_lnx_entries[i].modifiers;

            for (uindex_t j = i + 1; j < s_lnx_entry_count; j++)
                s_lnx_entries[j - 1] = s_lnx_entries[j];
            s_lnx_entry_count--;
            pthread_mutex_unlock(&s_mutex);

            for (unsigned ignore : kIgnoredMods)
                XUngrabKey(s_bg_display, t_kcode, t_mods | ignore, s_root);
            XFlush(s_bg_display);
            return;
        }
    }
    pthread_mutex_unlock(&s_mutex);
}

void MCPlatformUnregisterAllHotkeys()
{
    if (!s_bg_display)
        return;

    pthread_mutex_lock(&s_mutex);
    for (uindex_t i = 0; i < s_lnx_entry_count; i++)
    {
        for (unsigned ignore : kIgnoredMods)
            XUngrabKey(s_bg_display, s_lnx_entries[i].key_code,
                       s_lnx_entries[i].modifiers | ignore, s_root);
    }
    MCMemoryDeleteArray(s_lnx_entries);
    s_lnx_entries     = nullptr;
    s_lnx_entry_count = 0;
    pthread_mutex_unlock(&s_mutex);

    XFlush(s_bg_display);
}
