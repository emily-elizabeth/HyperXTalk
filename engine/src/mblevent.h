#ifndef MBLEVENTS_H
#define MBLEVENTS_H

#include "eventqueue.h"

class MCMovieTouchedEvent: public MCCustomEvent
{
public:
	MCMovieTouchedEvent(MCObject *p_object) :
      m_object(p_object->GetHandle())
	{
	}
    
    virtual ~MCMovieTouchedEvent()
    {
    }
	
	void Destroy(void)
	{
		delete this;
	}
	
	void Dispatch(void)
	{
		if (m_object.IsValid())
			m_object->message(MCM_movie_touched);
	}
	
private:
	MCObjectHandle m_object;
};

#endif // MBLEVENTS_H
