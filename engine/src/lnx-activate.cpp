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

// MCLinuxActivateApplication: deiconify, raise, and focus the default stack
// window. Mirrors MCMacActivateApplication on Linux/GTK.

#include "lnxprefix.h"

#include "globdefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "stack.h"
#include "globals.h"
#include "lnxdc.h"

void MCLinuxActivateApplication(void)
{
    if (MCdefaultstackptr == (MCStackHandle)nullptr)
        return;

    Window t_window = MCdefaultstackptr->getwindow();
    if (t_window == nil)
        return;

    // gdk_window_focus() sends _NET_ACTIVE_WINDOW with MCeventtime.  When
    // called from a global hotkey (GLib IO watch) there is no recent X11
    // input event, so MCeventtime is stale and GNOME's focus-stealing
    // prevention rejects the request.  Fetch a fresh server timestamp first
    // so the compositor accepts the raise unconditionally.
    MCeventtime = (uint4)x11::gdk_x11_get_server_time(t_window);

    MCscreen->uniconifywindow(t_window);
    MCscreen->raisewindow(t_window);
    MCscreen->setinputfocus(t_window);
}
