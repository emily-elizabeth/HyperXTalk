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
// X11 hotkey backend — deliberately isolated from the engine.
//
// This translation unit MUST NOT include prefix.h or any engine header.
// prefix.h pulls in sysdefs.h which typedef-declares Atom, Window, Drawable,
// and Pixmap at global scope; those names clash with the identically-named
// X11 typedefs.  Keeping this TU engine-free avoids the conflict entirely.
//
// All communication with the engine side goes through lnx-hotkey-x11.h,
// which uses only <stdint.h> types in its interface.
//

// X11 headers first — no engine headers above this line.
#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

#include "lnx-hotkey-x11.h"

////////////////////////////////////////////////////////////////////////////////
// Self-pipe

static int s_pipe_read  = -1;
static int s_pipe_write = -1;

bool lnx_hotkey_x11_ensure_pipe(void)
{
    if (s_pipe_read >= 0)
        return true;

    int fds[2];
    if (pipe(fds) != 0)
        return false;

    // Make the read end non-blocking so the GLib callback never stalls.
    fcntl(fds[0], F_SETFL, O_NONBLOCK);

    s_pipe_read  = fds[0];
    s_pipe_write = fds[1];
    return true;
}

int lnx_hotkey_x11_pipe_read_fd(void)
{
    return s_pipe_read;
}

////////////////////////////////////////////////////////////////////////////////
// Per-hotkey entry

struct MCLnxHotkeyEntry
{
    int32_t  engine_id;
    KeyCode  key_code;
    unsigned modifiers;
};

static MCLnxHotkeyEntry *s_entries     = nullptr;
static int               s_entry_count = 0;

static pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

// Modifier combinations to register so Num Lock / Caps Lock are transparent.
static const unsigned kIgnoredMods[] =
    { 0, Mod2Mask, LockMask, Mod2Mask | LockMask };

////////////////////////////////////////////////////////////////////////////////
// Background thread

static Display  *s_display       = nullptr;
static Window    s_root          = None;
static pthread_t s_thread;
static bool      s_thread_running = false;

static void *_hotkey_thread(void * /*unused*/)
{
    XEvent t_event;
    for (;;)
    {
        XNextEvent(s_display, &t_event);

        if (t_event.type != KeyPress)
            continue;

        XKeyEvent *ke = &t_event.xkey;
        unsigned t_clean = ke->state & ~(Mod2Mask | LockMask | Mod5Mask);

        pthread_mutex_lock(&s_mutex);
        for (int i = 0; i < s_entry_count; i++)
        {
            if (s_entries[i].key_code  == ke->keycode &&
                s_entries[i].modifiers == t_clean)
            {
                int32_t t_id = s_entries[i].engine_id;
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

bool lnx_hotkey_x11_init(void)
{
    if (s_thread_running)
        return true;

    s_display = XOpenDisplay(nullptr);
    if (!s_display)
        return false;

    s_root = DefaultRootWindow(s_display);

    if (pthread_create(&s_thread, nullptr, _hotkey_thread, nullptr) != 0)
    {
        XCloseDisplay(s_display);
        s_display = nullptr;
        return false;
    }
    pthread_detach(s_thread);
    s_thread_running = true;
    return true;
}

void lnx_hotkey_x11_shutdown(void)
{
    if (!s_display)
        return;

    lnx_hotkey_x11_ungrab_all();

    // Signal the thread to exit by closing the display — XNextEvent will
    // return with an error and the thread will eventually terminate.
    XCloseDisplay(s_display);
    s_display       = nullptr;
    s_thread_running = false;
}

////////////////////////////////////////////////////////////////////////////////
// Grab / ungrab

bool lnx_hotkey_x11_grab(int32_t p_engine_id,
                          unsigned int p_modifiers,
                          unsigned int p_keycode)
{
    if (!s_display)
        return false;

    KeyCode t_kcode = (KeyCode)p_keycode;

    XErrorHandler t_old =
        XSetErrorHandler([](Display *, XErrorEvent *) -> int { return 0; });

    for (unsigned ignore : kIgnoredMods)
        XGrabKey(s_display, t_kcode, p_modifiers | ignore,
                 s_root, True, GrabModeAsync, GrabModeAsync);

    XFlush(s_display);
    XSetErrorHandler(t_old);

    // Append to the entry list.
    pthread_mutex_lock(&s_mutex);
    MCLnxHotkeyEntry *t_new =
        (MCLnxHotkeyEntry *)realloc(s_entries,
                                    (s_entry_count + 1) * sizeof(*s_entries));
    bool t_ok = (t_new != nullptr);
    if (t_ok)
    {
        s_entries = t_new;
        s_entries[s_entry_count++] = { p_engine_id, t_kcode, p_modifiers };
    }
    pthread_mutex_unlock(&s_mutex);

    if (!t_ok)
    {
        for (unsigned ignore : kIgnoredMods)
            XUngrabKey(s_display, t_kcode, p_modifiers | ignore, s_root);
        XFlush(s_display);
    }

    return t_ok;
}

void lnx_hotkey_x11_ungrab(int32_t p_engine_id)
{
    if (!s_display)
        return;

    pthread_mutex_lock(&s_mutex);
    for (int i = 0; i < s_entry_count; i++)
    {
        if (s_entries[i].engine_id == p_engine_id)
        {
            KeyCode  t_kcode = s_entries[i].key_code;
            unsigned t_mods  = s_entries[i].modifiers;

            for (int j = i + 1; j < s_entry_count; j++)
                s_entries[j - 1] = s_entries[j];
            s_entry_count--;
            pthread_mutex_unlock(&s_mutex);

            for (unsigned ignore : kIgnoredMods)
                XUngrabKey(s_display, t_kcode, t_mods | ignore, s_root);
            XFlush(s_display);
            return;
        }
    }
    pthread_mutex_unlock(&s_mutex);
}

void lnx_hotkey_x11_ungrab_all(void)
{
    if (!s_display)
        return;

    pthread_mutex_lock(&s_mutex);
    for (int i = 0; i < s_entry_count; i++)
    {
        for (unsigned ignore : kIgnoredMods)
            XUngrabKey(s_display, s_entries[i].key_code,
                       s_entries[i].modifiers | ignore, s_root);
    }
    free(s_entries);
    s_entries     = nullptr;
    s_entry_count = 0;
    pthread_mutex_unlock(&s_mutex);

    XFlush(s_display);
}

////////////////////////////////////////////////////////////////////////////////
// Key string parser

static bool _token_to_keysym(const char *p_token, KeySym& r_sym)
{
    if (p_token[1] == '\0')
    {
        char c = (char)tolower((unsigned char)p_token[0]);
        if (c >= 'a' && c <= 'z') { r_sym = (KeySym)c; return true; }
        if (c >= '0' && c <= '9') { r_sym = (KeySym)c; return true; }
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

bool lnx_hotkey_x11_parse_key(const char   *p_str,
                               unsigned int *r_modifiers,
                               unsigned int *r_keycode,
                               char         *r_error,
                               size_t        p_error_len)
{
    if (!s_display)
    {
        strncpy(r_error, "X display not open", p_error_len);
        return false;
    }

    // Work on a mutable copy.
    char *t_buf = strdup(p_str);
    if (!t_buf)
    {
        strncpy(r_error, "out of memory", p_error_len);
        return false;
    }

    unsigned t_mods = 0;

    char *t_tokens[16];
    int   t_count = 0;
    char *t_p     = t_buf;

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
            t_mods |= ControlMask;
        else if (strcasecmp(m, "alt")     == 0 || strcasecmp(m, "option")  == 0)
            t_mods |= Mod1Mask;
        else if (strcasecmp(m, "shift")   == 0)
            t_mods |= ShiftMask;
        else if (strcasecmp(m, "win")     == 0 || strcasecmp(m, "cmd")     == 0 ||
                 strcasecmp(m, "command") == 0)
            t_mods |= Mod4Mask;
        else
        {
            snprintf(r_error, p_error_len, "unknown modifier: %s", m);
            free(t_buf);
            return false;
        }
    }

    KeySym t_sym = NoSymbol;
    if (t_count == 0 || !_token_to_keysym(t_tokens[t_count - 1], t_sym))
    {
        snprintf(r_error, p_error_len, "unknown key: %s",
                 t_count > 0 ? t_tokens[t_count - 1] : "(none)");
        free(t_buf);
        return false;
    }

    KeyCode t_kcode = XKeysymToKeycode(s_display, t_sym);
    if (t_kcode == 0)
    {
        snprintf(r_error, p_error_len,
                 "key not available on this keyboard layout: %s",
                 t_tokens[t_count - 1]);
        free(t_buf);
        return false;
    }

    *r_modifiers = t_mods;
    *r_keycode   = (unsigned int)t_kcode;
    free(t_buf);
    return true;
}
