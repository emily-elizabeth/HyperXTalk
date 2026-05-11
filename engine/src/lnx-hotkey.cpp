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
// Linux global hotkey backend — engine side.
//
// This file contains only engine includes and GLib; it deliberately does NOT
// include any X11 headers.  All X11 work is delegated to lnx-hotkey-x11.cpp
// via the plain-C interface in lnx-hotkey-x11.h.
//
// The separation exists because prefix.h (included below) pulls in
// sysdefs.h, which typedef-declares Atom, Window, Drawable, and Pixmap at
// global scope.  Those names clash with X11's identically-named typedefs, so
// the two sets of headers must never appear in the same translation unit.
//

#include "prefix.h"

#include <glib.h>
#include <unistd.h>
#include <stdint.h>

#include "mcstring.h"
#include "param.h"
#include "hotkey.h"
#include "globals.h"
#include "lnx-hotkey-x11.h"

////////////////////////////////////////////////////////////////////////////////
// GLib pipe watch — runs on the main thread

static guint s_io_watch = 0;

// GLib I/O callback: drains the self-pipe and dispatches each hotkey ID.
static gboolean _pipe_readable(GIOChannel * /*channel*/,
                                GIOCondition /*cond*/,
                                gpointer     /*data*/)
{
    int t_fd = lnx_hotkey_x11_pipe_read_fd();
    int32_t t_id;
    while (read(t_fd, &t_id, sizeof(t_id)) == (ssize_t)sizeof(t_id))
        MCHotkeyDispatchFired(t_id);
    return TRUE;
}

static bool _ensure_watch()
{
    if (s_io_watch != 0)
        return true;

    if (!lnx_hotkey_x11_ensure_pipe())
        return false;

    int t_fd = lnx_hotkey_x11_pipe_read_fd();
    GIOChannel *t_chan = g_io_channel_unix_new(t_fd);
    s_io_watch = g_io_add_watch(t_chan, G_IO_IN, _pipe_readable, nullptr);
    g_io_channel_unref(t_chan);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Platform entry points

bool MCPlatformRegisterHotkey(MCStringRef p_key, int32_t p_id)
{
    if (!_ensure_watch())
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(
            "failed to create hotkey pipe", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    if (!lnx_hotkey_x11_init())
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(
            "failed to open X display for hotkey thread", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    // Convert the engine string to a plain C string for the X11 TU.
    char *t_cstr = nullptr;
    if (!MCStringConvertToCString(p_key, t_cstr))
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString("out of memory", t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    unsigned int t_mods   = 0;
    unsigned int t_kcode  = 0;
    char         t_error[128] = {};

    bool t_ok = lnx_hotkey_x11_parse_key(t_cstr, &t_mods, &t_kcode,
                                          t_error, sizeof(t_error));
    MCMemoryDeallocate(t_cstr);

    if (!t_ok)
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(t_error, t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    if (!lnx_hotkey_x11_grab(p_id, t_mods, t_kcode))
    {
        MCStringRef t_err;
        /* UNCHECKED */ MCStringCreateWithCString(
            "failed to grab hotkey (already registered by another application?)",
            t_err);
        MCresult->setvalueref(t_err);
        MCValueRelease(t_err);
        return false;
    }

    return true;
}

void MCPlatformUnregisterHotkey(int32_t p_id)
{
    lnx_hotkey_x11_ungrab(p_id);
}

void MCPlatformUnregisterAllHotkeys()
{
    lnx_hotkey_x11_ungrab_all();
}
