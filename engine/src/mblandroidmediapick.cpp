#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"


#include "globals.h"
#include "stack.h"
#include "system.h"
#include "player.h"
#include "eventqueue.h"
#include "osspec.h"

#include "mblandroid.h"
#include "mblandroidutil.h"

#include "mblandroidjava.h"
#include "mblsyntax.h"

#include <jni.h>
#include "mblandroidjava.h"

#include "mblad.h"

///////////////////////////////////////////////////////////////////////////////

typedef enum
{
    kMCAndroidMediaWaiting,
    kMCAndroidMediaDone,
    kMCAndroidMediaCanceled,
} MCAndroidMediaStatus;

static MCAndroidMediaStatus s_media_status = kMCAndroidMediaWaiting;
static MCStringRef s_media_content = nil;

void MCAndroidMediaPickInitialize()
{
    s_media_content = MCValueRetain(kMCEmptyString);
}

bool MCSystemPickMedia(MCMediaType p_types, bool p_multiple, MCStringRef& r_result)
{
    s_media_status = kMCAndroidMediaWaiting;
    
    bool t_audio, t_video;
    t_audio = (kMCMediaTypeAnyAudio & p_types) != 0;
    t_video = (kMCMediaTypeAnyVideo & p_types) != 0;
    
    if (t_audio && !t_video)
	{
        MCAndroidEngineCall("pickMedia", "vs", nil, "audio/*");
	}
	else if (!t_audio && t_video)
	{
        MCAndroidEngineCall("pickMedia", "vs", nil, "video/*");
	}
    else
	{
        MCAndroidEngineCall("pickMedia", "vs", nil, "audio/* video/*");
	}
    
    while (s_media_status == kMCAndroidMediaWaiting)
        MCscreen->wait(60.0, False, True);

    return MCStringCopy(s_media_content, r_result);
    //    MCLog("Media Types Returned: %s", s_media_content);
}

void MCAndroidMediaDone(MCStringRef p_media_content)
{
    MCStringCopy(p_media_content, s_media_content);
    //    MCLog("MCAndroidMediaDone() called %s", p_media_content);
	s_media_status = kMCAndroidMediaDone;
}

void MCAndroidMediaCanceled()
{
    //    MCLog("MCAndroidMediaCanceled() called", nil);
	s_media_status = kMCAndroidMediaCanceled;
}
