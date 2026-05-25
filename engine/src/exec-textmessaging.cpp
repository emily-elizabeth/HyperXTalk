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

void MCTextMessagingGetCanComposeTextMessage(MCExecContext& ctxt, bool& r_result)
{
    r_result = MCSystemCanSendTextMessage();
}

void MCTextMessagingExecComposeTextMessage(MCExecContext& ctxt, MCStringRef p_recipients, MCStringRef p_body)
{
    if (!MCSystemCanSendTextMessage())
        ctxt . SetTheResultToValue(kMCFalse);
    else
        MCSystemComposeTextMessage(p_recipients, p_body);
}
