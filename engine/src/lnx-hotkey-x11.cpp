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
// X11-only implementation of the Linux hotkey backend.
// This translation unit intentionally does NOT include prefix.h or any
// engine headers so that X11's Window/Atom/Drawable/Pixmap typedefs never
// collide with the GDK aliases defined by sysdefs.h.
//

#include "lnx-hotkey-x11.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

////////////////////////////////////////////////////////////////////////////////
// Entry storage

typedef struct
{
    int32_t  engine_id;
    unsigned key_code;
    unsigned modifiers;
} LnxHotkeyEntry;

static LnxHotkeyEntry  *s_entries      = NULL;
static size_t           s_entry_count  = 0;
static size_t           s_entry_cap    = 0;

// Mutex protecting s_entries from the background thread.
static pthread_mutex_t  s_mutex        = PTHREAD_MUTEX_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////
// X11 / thread state

static Display  *s_bg_display     = NULL;
static Window    s_root           = None;
static pthread_t s_thread;
static int       s_thread_running = 0;
static int       s_pipe_write_fd  = -1;

// Modifier combinations to grab (plain, NumLock, CapsLock, both).
static const unsigned kIgnoredMods[] = { 0, Mod2Mask, LockMask, Mod2Mask | LockMask };

////////////////////////////////////////////////////////////////////////////////
// Background thread

static void *_hotkey_thread(void *unused)
{
    (void)unused;
    XEvent t_event;
    for (;;)
    {
        XNextEvent(s_bg_display, &t_event);

        if (t_event.type != KeyPress)
            continue;

        XKeyEvent *ke = &t_event.xkey;
        // Keep only the modifier bits we care about; ignore NumLock, CapsLock,
        // ScrollLock, XKB group bits, and pointer-button state bits.
        unsigned t_clean = ke->state &
            (ShiftMask | ControlMask | Mod1Mask | Mod3Mask | Mod4Mask);

        pthread_mutex_lock(&s_mutex);
        size_t i;
        for (i = 0; i < s_entry_count; i++)
        {
            if (s_entries[i].key_code  == (unsigned)ke->keycode &&
                s_entries[i].modifiers == t_clean)
            {
                int32_t t_id = s_entries[i].engine_id;
                pthread_mutex_unlock(&s_mutex);
                (void)write(s_pipe_write_fd, &t_id, sizeof(t_id));
                goto next_event;
            }
        }
        pthread_mutex_unlock(&s_mutex);

    next_event:;
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Public C API

int lnx_hotkey_x11_init(int write_fd)
{
    if (s_thread_running)
        return 1;  // idempotent

    // Enable Xlib thread safety before opening a display that will be
    // shared between the main thread (XGrabKey) and the event thread (XNextEvent).
    XInitThreads();

    s_bg_display = XOpenDisplay(NULL);
    if (!s_bg_display)
        return 0;

    s_root           = DefaultRootWindow(s_bg_display);
    s_pipe_write_fd  = write_fd;

    // Ensure KeyPress events are delivered to the root window's event queue.
    XSelectInput(s_bg_display, s_root, KeyPressMask);

    if (pthread_create(&s_thread, NULL, _hotkey_thread, NULL) != 0)
    {
        XCloseDisplay(s_bg_display);
        s_bg_display = NULL;
        return 0;
    }
    pthread_detach(s_thread);
    s_thread_running = 1;
    return 1;
}

int lnx_hotkey_x11_display_open(void)
{
    return s_bg_display != NULL ? 1 : 0;
}

////////////////////////////////////////////////////////////////////////////////
// Key string parser

static int _token_to_keysym(const char *p_token, KeySym *r_sym)
{
    if (p_token[0] != '\0' && p_token[1] == '\0')
    {
        char c = (char)tolower((unsigned char)p_token[0]);
        if (c >= 'a' && c <= 'z') { *r_sym = (KeySym)c; return 1; }
        if (c >= '0' && c <= '9') { *r_sym = (KeySym)c; return 1; }
    }

    if ((p_token[0] == 'f' || p_token[0] == 'F') && p_token[1] != '\0')
    {
        int fnum = atoi(p_token + 1);
        if (fnum >= 1 && fnum <= 12) { *r_sym = XK_F1 + fnum - 1; return 1; }
    }

    if (strcasecmp(p_token, "space")     == 0) { *r_sym = XK_space;     return 1; }
    if (strcasecmp(p_token, "tab")       == 0) { *r_sym = XK_Tab;       return 1; }
    if (strcasecmp(p_token, "return")    == 0) { *r_sym = XK_Return;    return 1; }
    if (strcasecmp(p_token, "enter")     == 0) { *r_sym = XK_Return;    return 1; }
    if (strcasecmp(p_token, "escape")    == 0) { *r_sym = XK_Escape;    return 1; }
    if (strcasecmp(p_token, "esc")       == 0) { *r_sym = XK_Escape;    return 1; }
    if (strcasecmp(p_token, "delete")    == 0) { *r_sym = XK_Delete;    return 1; }
    if (strcasecmp(p_token, "backspace") == 0) { *r_sym = XK_BackSpace; return 1; }
    if (strcasecmp(p_token, "home")      == 0) { *r_sym = XK_Home;      return 1; }
    if (strcasecmp(p_token, "end")       == 0) { *r_sym = XK_End;       return 1; }
    if (strcasecmp(p_token, "pageup")    == 0) { *r_sym = XK_Page_Up;   return 1; }
    if (strcasecmp(p_token, "pagedown")  == 0) { *r_sym = XK_Page_Down; return 1; }
    if (strcasecmp(p_token, "left")      == 0) { *r_sym = XK_Left;      return 1; }
    if (strcasecmp(p_token, "right")     == 0) { *r_sym = XK_Right;     return 1; }
    if (strcasecmp(p_token, "up")        == 0) { *r_sym = XK_Up;        return 1; }
    if (strcasecmp(p_token, "down")      == 0) { *r_sym = XK_Down;      return 1; }
    if (strcasecmp(p_token, "insert")    == 0) { *r_sym = XK_Insert;    return 1; }

    return 0;
}

int lnx_hotkey_x11_parse(const char *p_key,
                          unsigned   *r_modifiers,
                          unsigned   *r_keycode,
                          char       *r_error,
                          size_t      p_error_len)
{
    char *t_cstr = strdup(p_key);
    if (!t_cstr)
    {
        strncpy(r_error, "out of memory", p_error_len);
        r_error[p_error_len - 1] = '\0';
        return 0;
    }

    *r_modifiers = 0;

    // Tokenise on '+'.
    char *t_tokens[16];
    int   t_count = 0;
    char *t_p     = t_cstr;

    while (*t_p && t_count < 15)
    {
        char *t_start = t_p;
        while (*t_p && *t_p != '+') t_p++;
        if (*t_p == '+')
        {
            *t_p = '\0';
            t_tokens[t_count++] = t_start;
            t_p++;
        }
        else
        {
            t_tokens[t_count++] = t_start;
            break;
        }
    }

    // All tokens except the last are modifiers.
    int i;
    for (i = 0; i < t_count - 1; i++)
    {
        const char *m = t_tokens[i];
        if      (strcasecmp(m, "ctrl")    == 0 || strcasecmp(m, "control") == 0)
            *r_modifiers |= ControlMask;
        else if (strcasecmp(m, "alt")     == 0 || strcasecmp(m, "option")  == 0)
            *r_modifiers |= Mod1Mask;
        else if (strcasecmp(m, "shift")   == 0)
            *r_modifiers |= ShiftMask;
        else if (strcasecmp(m, "win")     == 0 || strcasecmp(m, "cmd")     == 0 ||
                 strcasecmp(m, "command") == 0)
            *r_modifiers |= Mod4Mask;
        else
        {
            snprintf(r_error, p_error_len, "unknown modifier: %s", m);
            free(t_cstr);
            return 0;
        }
    }

    KeySym t_sym = NoSymbol;
    if (t_count == 0 || !_token_to_keysym(t_tokens[t_count - 1], &t_sym))
    {
        snprintf(r_error, p_error_len, "unknown key: %s",
                 t_count > 0 ? t_tokens[t_count - 1] : "(none)");
        free(t_cstr);
        return 0;
    }

    KeyCode t_kc = XKeysymToKeycode(s_bg_display, t_sym);
    if (t_kc == 0)
    {
        snprintf(r_error, p_error_len,
                 "key not available on this keyboard layout: %s",
                 t_tokens[t_count - 1]);
        free(t_cstr);
        return 0;
    }

    *r_keycode = (unsigned)t_kc;
    free(t_cstr);
    return 1;
}

////////////////////////////////////////////////////////////////////////////////
// Grab / ungrab helpers

// Error handler that records whether a BadAccess was received.
static int s_grab_had_error = 0;
static int _grab_xerror(Display *dpy, XErrorEvent *err)
{
    (void)dpy;
    if (err->error_code == BadAccess)
        s_grab_had_error = 1;
    return 0;
}

int lnx_hotkey_x11_grab(unsigned modifiers, unsigned keycode)
{
    if (!s_bg_display)
        return 0;

    s_grab_had_error = 0;
    XErrorHandler t_old = XSetErrorHandler(_grab_xerror);
    unsigned i;
    for (i = 0; i < sizeof(kIgnoredMods) / sizeof(kIgnoredMods[0]); i++)
        XGrabKey(s_bg_display, (int)keycode, modifiers | kIgnoredMods[i],
                 s_root, True, GrabModeAsync, GrabModeAsync);
    XSync(s_bg_display, False);   // flush + wait so errors arrive before we restore handler
    XSetErrorHandler(t_old);

    if (s_grab_had_error)
    {
        fprintf(stderr,
                "lnx-hotkey: XGrabKey BadAccess — key already grabbed by "
                "another application (keycode=%u mods=%u)\n",
                keycode, modifiers);
        return 0;
    }
    return 1;
}

void lnx_hotkey_x11_ungrab(unsigned modifiers, unsigned keycode)
{
    if (!s_bg_display)
        return;

    unsigned i;
    for (i = 0; i < sizeof(kIgnoredMods) / sizeof(kIgnoredMods[0]); i++)
        XUngrabKey(s_bg_display, (int)keycode, modifiers | kIgnoredMods[i], s_root);
}

void lnx_hotkey_x11_flush(void)
{
    if (s_bg_display)
        XFlush(s_bg_display);
}

////////////////////////////////////////////////////////////////////////////////
// Entry storage helpers

int lnx_hotkey_x11_store(int32_t engine_id, unsigned keycode, unsigned modifiers)
{
    pthread_mutex_lock(&s_mutex);

    if (s_entry_count >= s_entry_cap)
    {
        size_t new_cap = s_entry_cap == 0 ? 8 : s_entry_cap * 2;
        LnxHotkeyEntry *new_buf = (LnxHotkeyEntry *)realloc(s_entries,
                                        new_cap * sizeof(LnxHotkeyEntry));
        if (!new_buf)
        {
            pthread_mutex_unlock(&s_mutex);
            return 0;
        }
        s_entries    = new_buf;
        s_entry_cap  = new_cap;
    }

    s_entries[s_entry_count].engine_id  = engine_id;
    s_entries[s_entry_count].key_code   = keycode;
    s_entries[s_entry_count].modifiers  = modifiers;
    s_entry_count++;

    pthread_mutex_unlock(&s_mutex);
    return 1;
}

int lnx_hotkey_x11_remove(int32_t engine_id, unsigned *r_keycode, unsigned *r_modifiers)
{
    pthread_mutex_lock(&s_mutex);

    size_t i;
    for (i = 0; i < s_entry_count; i++)
    {
        if (s_entries[i].engine_id == engine_id)
        {
            *r_keycode   = s_entries[i].key_code;
            *r_modifiers = s_entries[i].modifiers;

            size_t j;
            for (j = i + 1; j < s_entry_count; j++)
                s_entries[j - 1] = s_entries[j];
            s_entry_count--;

            pthread_mutex_unlock(&s_mutex);
            return 1;
        }
    }

    pthread_mutex_unlock(&s_mutex);
    return 0;
}

void lnx_hotkey_x11_remove_all_and_ungrab(void)
{
    pthread_mutex_lock(&s_mutex);

    size_t i;
    for (i = 0; i < s_entry_count; i++)
    {
        unsigned j;
        for (j = 0; j < sizeof(kIgnoredMods) / sizeof(kIgnoredMods[0]); j++)
            XUngrabKey(s_bg_display,
                       (int)s_entries[i].key_code,
                       s_entries[i].modifiers | kIgnoredMods[j],
                       s_root);
    }

    free(s_entries);
    s_entries     = NULL;
    s_entry_count = 0;
    s_entry_cap   = 0;

    pthread_mutex_unlock(&s_mutex);
}
