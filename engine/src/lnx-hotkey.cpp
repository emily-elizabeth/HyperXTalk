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
// Linux global hotkey backend.
//
// Two backends are supported, selected at runtime:
//
//   Portal (Wayland) — lnx-hotkey-portal.cpp
//     Uses the XDG GlobalShortcuts D-Bus portal.  Selected when
//     $WAYLAND_DISPLAY is set and the portal session can be created.
//     Works on GNOME 43+, KDE Plasma 5.27+.
//
//   X11 — lnx-hotkey-x11.cpp
//     Uses XGrabKey on the root window via a dedicated thread.
//     Selected when running under a plain X11 session.
//
// Both backends write a 4-byte engine ID into a self-pipe when a hotkey
// fires.  The main thread watches the pipe via g_io_add_watch() so that
// MCHotkeyDispatchFired() is always called from the correct thread.
//

#include "prefix.h"
#include <glib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>   // getenv

#include "mcstring.h"
#include "hotkey.h"
#include "globals.h"
#include "variable.h"

#include "lnx-hotkey-x11.h"
#include "lnx-hotkey-portal.h"

////////////////////////////////////////////////////////////////////////////////
// Backend selection

static bool _use_portal()
{
    // Use the portal backend when running under Wayland.
    return getenv("WAYLAND_DISPLAY") != nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Self-pipe for main-thread dispatch

static int   s_pipe_read  = -1;
static int   s_pipe_write = -1;
static guint s_io_watch   = 0;

static gboolean _pipe_readable(GIOChannel * /*channel*/,
                                GIOCondition /*cond*/,
                                gpointer     /*data*/)
{
    int32_t t_id;
    while (read(s_pipe_read, &t_id, sizeof(t_id)) == (ssize_t)sizeof(t_id))
        MCHotkeyDispatchFired(t_id);
    return TRUE;
}

static bool _ensure_pipe()
{
    if (s_pipe_read >= 0)
        return true;

    int fds[2];
    if (pipe(fds) != 0)
        return false;

    fcntl(fds[0], F_SETFL, O_NONBLOCK);

    s_pipe_read  = fds[0];
    s_pipe_write = fds[1];

    GIOChannel *t_chan = g_io_channel_unix_new(s_pipe_read);
    s_io_watch = g_io_add_watch(t_chan, G_IO_IN, _pipe_readable, nullptr);
    g_io_channel_unref(t_chan);

    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Helper: set MCresult to a C string error

static void _set_error(const char *msg)
{
    MCStringRef t_err;
    /* UNCHECKED */ MCStringCreateWithCString(msg, t_err);
    MCresult->setvalueref(t_err);
    MCValueRelease(t_err);
}

////////////////////////////////////////////////////////////////////////////////
// Platform entry points

bool MCPlatformRegisterHotkey(MCStringRef p_key, int32_t p_id)
{
    if (!_ensure_pipe())
    {
        _set_error("failed to create hotkey pipe");
        return false;
    }

    char *t_cstr = nullptr;
    if (!MCStringConvertToCString(p_key, t_cstr))
    {
        _set_error("out of memory converting key string");
        return false;
    }

    // ---- Portal path (Wayland) -----------------------------------------------
    if (_use_portal())
    {
        if (!lnx_hotkey_portal_init(s_pipe_write))
        {
            // Portal unavailable — fall through to X11 if $DISPLAY is also set.
            if (getenv("DISPLAY") == nullptr)
            {
                MCMemoryDeallocate(t_cstr);
                _set_error("global hotkeys require the XDG GlobalShortcuts portal "
                           "(GNOME 43+ or KDE Plasma 5.27+)");
                return false;
            }
            // Fall through to X11 below.
        }
        else
        {
            char t_error[256] = {};
            bool ok = lnx_hotkey_portal_register(p_id, t_cstr,
                                                  t_error, sizeof(t_error)) != 0;
            MCMemoryDeallocate(t_cstr);
            if (!ok)
                _set_error(t_error[0] ? t_error : "portal hotkey registration failed");
            return ok;
        }
    }

    // ---- X11 path ------------------------------------------------------------
    if (!lnx_hotkey_x11_init(s_pipe_write))
    {
        MCMemoryDeallocate(t_cstr);
        _set_error("failed to open X display for hotkey thread");
        return false;
    }

    unsigned t_mods  = 0;
    unsigned t_kcode = 0;
    char     t_error[128] = {};

    if (!lnx_hotkey_x11_parse(t_cstr, &t_mods, &t_kcode, t_error, sizeof(t_error)))
    {
        MCMemoryDeallocate(t_cstr);
        _set_error(t_error);
        return false;
    }
    MCMemoryDeallocate(t_cstr);

    if (!lnx_hotkey_x11_grab(t_mods, t_kcode))
    {
        lnx_hotkey_x11_flush();
        _set_error("hotkey already in use by another application");
        return false;
    }

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
    if (_use_portal())
    {
        lnx_hotkey_portal_unregister(p_id);
        return;
    }

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
    if (_use_portal())
    {
        lnx_hotkey_portal_unregister_all();
        return;
    }

    if (!lnx_hotkey_x11_display_open())
        return;

    lnx_hotkey_x11_remove_all_and_ungrab();
    lnx_hotkey_x11_flush();
}
