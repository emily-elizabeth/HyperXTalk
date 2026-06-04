#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "mcio.h"

#include "globals.h"
#include "stack.h"
#include "image.h"
#include "param.h"

#include "exec.h"

#include "eventqueue.h"

#include "mblsyntax.h"

////////////////////////////////////////////////////////////////////////////////

bool MCParseParameters(MCParameter*& p_parameters, const char *p_format, ...);

////////////////////////////////////////////////////////////////////////////////

class MCSoundFinishedOnChannelEvent: public MCCustomEvent
{
public:
	MCSoundFinishedOnChannelEvent(MCObjectHandle p_object, MCStringRef p_channel, MCStringRef p_sound)
	{
		m_object = nil;
		m_channel = MCValueRetain(p_channel);
		m_sound = MCValueRetain(p_sound);
		
		m_object = p_object;
	}
	
	void Destroy(void)
	{
		MCValueRelease(m_channel);
		MCValueRelease(m_sound);
		delete this;
	}
	
	void Dispatch(void)
	{
		if (m_object.IsValid())
			m_object->message_with_valueref_args(MCM_sound_finished_on_channel, m_channel, m_sound);
	}
	
private:
	MCObjectHandle m_object;
	MCStringRef m_channel;
	MCStringRef m_sound;
};

void MCSoundPostSoundFinishedOnChannelMessage(MCStringRef p_channel, MCStringRef p_sound, MCObjectHandle p_object)
{
    MCCustomEvent *t_event = nil;
    t_event = new (nothrow) MCSoundFinishedOnChannelEvent(p_object, p_channel, p_sound);
    if (t_event != nil)
        MCEventQueuePostCustom(t_event);
}

////////////////////////////////////////////////////////////////////////////////
