
#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "mcio.h"

#include "mcerror.h"
#include "globals.h"
#include "exec.h"

#include "mblsyntax.h"

////////////////////////////////////////////////////////////////////////////////

void MCIdleTimerExecLockIdleTimer(MCExecContext& ctxt)
{
    MCSystemLockIdleTimer();
}


void MCIdleTimerExecUnlockIdleTimer(MCExecContext& ctxt)
{
    MCSystemUnlockIdleTimer();
}

void MCIdleTimerGetIdleTimerLocked(MCExecContext& ctxt, bool& r_result)
{
    r_result = MCSystemIdleTimerLocked();
}
