// MCLinuxActivateApplication: deiconify, raise, and focus the default stack
// window. Mirrors MCMacActivateApplication on Linux/GTK. Behaviour is
// best-effort — compositors that enforce focus-stealing prevention (e.g. GNOME
// 3 / Mutter) may flash the taskbar rather than immediately raising the window.

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

    // Deiconify in case the window is minimised, then raise and focus.
    MCscreen->uniconifywindow(t_window);
    MCscreen->raisewindow(t_window);
    MCscreen->setinputfocus(t_window);
}
