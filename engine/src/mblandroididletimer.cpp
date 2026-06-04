#include "prefix.h"


#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "mcerror.h"

#include "printer.h"
#include "globals.h"
#include "dispatch.h"
#include "stack.h"
#include "image.h"
#include "player.h"
#include "param.h"
#include "eventqueue.h"
#include "osspec.h"

#include "date.h"

#include "mbldc.h"

#include "mblandroidutil.h"
#include "mblandroidjava.h"

#include "mblsyntax.h"

#include <string.h>

#include <jni.h>


void MCSystemLockIdleTimer(void)
{
	MCAndroidEngineCall("doLockIdleTimer", "v", nil);
}

void MCSystemUnlockIdleTimer(void)
{
	MCAndroidEngineCall("doUnlockIdleTimer", "v", nil);
}

bool MCSystemIdleTimerLocked(void)
{
    bool r_idle_timer_locked = false;
	MCAndroidEngineCall("getLockIdleTimerLocked", "b", &r_idle_timer_locked);
        
	return r_idle_timer_locked;
}
