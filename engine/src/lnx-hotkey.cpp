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
// All X11 interaction is delegated to lnx-hotkey-x11.cpp so that the
// X11 integer typedefs (Window, Atom, Drawable, Pixmap) never appear in
// the same TU as the GDK pointer aliases defined by sysdefs.h via prefix.h.
//
// Requires: -lX11  (already linked in the Linux desktop build)
//           GLib / GDK (already linked)
//

#include "prefix.h"
#include <glib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#include "mcstring.h"
#include "hotkey.h"
#include "globals.h"
#include "variable.h"

#include "lnx-hotkey-x11.h"

////////////////////////////////////////////////////////////////////////////////
// Self-pipe for main-thread dispatch

static int   s_pipe_read  = -1;
static int   s_pipe_write = -1;
static guint s_io_watch   = 0;   // GLib watch source ID

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

    if (!lnx_hotkey_x11_init(s_pipe_write))
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString("failed to open X display for hotkey thread", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    char *t_cstr = nullptr;
    if (!MCStringConvertToCString(p_key, t_cstr))
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString("out of memory converting key string", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    unsigned t_mods  = 0;
    unsigned t_kcode = 0;
    char     t_error[128] = {};

    if (!lnx_hotkey_x11_parse(t_cstr, &t_mods, &t_kcode, t_error, sizeof(t_error)))
    {
        MCMemoryDeallocate(t_cstr);
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(t_error, t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }
    MCMemoryDeallocate(t_cstr);

    lnx_hotkey_x11_grab(t_mods, t_kcode);

    if (!lnx_hotkey_x11_store(p_id, t_kcode, t_mods))
    {
        lnx_hotkey_x11_ungrab(t_mods, t_kcode);
        lnx_hotkey_x11_flush();
        return false;
    }

    lnx_hotkey_x11_flush();
    return true;
}

void MCPlatformUnregisterHotkey(int32_t p_id)
{
    if (!lnx_hotkey_x11_display_open())
        return;

    unsigned t_kcode = 0;
    unsigned t_mods  = 0;

    if (lnx_hotkey_x11_remove(p_id, &t_kcode, &t_mods))
    {
        lnx_hotkey_x11_ungrab(t_mods, t_kcode);
        lnx_hotkey_x11_flush();
    }
}

void MCPlatformUnregisterAllHotkeys()
{
    if (!lnx_hotkey_x11_display_open())
        return;

    lnx_hotkey_x11_remove_all_and_ungrab();
    lnx_hotkey_x11_flush();
}
