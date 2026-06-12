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
