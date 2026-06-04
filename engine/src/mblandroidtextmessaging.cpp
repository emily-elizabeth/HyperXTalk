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

typedef enum
{
    kMCAndroidTextWaiting,
	kMCAndroidTextSent,
	kMCAndroidTextCanceled,
} MCAndroidTextStatus;

static MCAndroidTextStatus s_text_status = kMCAndroidTextWaiting; 

void MCAndroidTextDone()
{
	s_text_status = kMCAndroidTextSent;
}

void MCAndroidTextCanceled()
{
	s_text_status = kMCAndroidTextCanceled;
}

bool MCSystemCanSendTextMessage()
{
    bool t_can_send = false;
	MCAndroidEngineCall("canSendTextMessage", "b", &t_can_send);
	return t_can_send;
}

bool MCSystemComposeTextMessage(MCStringRef p_recipients, MCStringRef p_body)
{
    s_text_status = kMCAndroidTextWaiting;

    MCAndroidEngineRemoteCall("composeTextMessage", "vxx", nil, p_recipients, p_body);
    while (s_text_status == kMCAndroidTextWaiting)
		MCscreen->wait(60.0, False, True);
	
	MCresult -> sets(s_text_status == kMCAndroidTextSent ? "sent" : "cancel");

    return true;
}
