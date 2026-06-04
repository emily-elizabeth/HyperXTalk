#ifndef __OSXSPEECH__
#define __OSXSPEECH__

#ifndef __REVSPEECH__
#include "revspeech.h"
#endif

typedef int volume_t;

class OSXSpeechNarrator: public INarrator
{
public:
	OSXSpeechNarrator(void);
	~OSXSpeechNarrator(void);
	
	bool Initialize(void);
	bool Finalize(void);
	
	bool Start(const char* p_string, bool p_is_utf8);
	bool Stop(void);
	bool Busy(void);

	bool ListVoices(NarratorGender p_gender, NarratorListVoicesCallback p_callback, void *p_context);
	bool SetVoice(const char* p_voice);

	bool SetSpeed(int p_speed);
	bool GetSpeed(int& p_speed);

	bool SetPitch(int p_pitch);
	bool GetPitch(int& p_pitch);

	bool SetVolume(int p_volume);
	bool GetVolume(int& p_volume);
	
private:
	SpeechChannel spchannel;
	char speechvoice[255];
    CFStringRef speechtext;
	Fixed speechspeed, speechpitch;
	
	bool SpeechStart(bool StartInit);
	bool SpeechStop(bool ReleaseInit);
	void FindAndSelect();
};

#endif
