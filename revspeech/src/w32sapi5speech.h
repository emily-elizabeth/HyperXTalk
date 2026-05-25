#ifndef __W32SAPI5SPEECH__
#define __W32SAPI5SPEECH__

#ifndef __REVSPEECH__
#include "revspeech.h"
#endif

#include <sapi.h>
#include <sperror.h>
#include <mlang.h>
#include <stdio.h>

#include "atlsubset.h"

class WindowsSAPI5Narrator: public INarrator
{
public:
	bool isspeaking;

	WindowsSAPI5Narrator(void);
	~WindowsSAPI5Narrator(void);

	bool Initialize(void);
	bool Finalize(void);
	bool IsInited() {return bInited;}

	bool Start(const char* p_string, bool p_is_utf8);
	bool SpeakToFile(const char* p_string, const char* p_file);
	bool Stop(void);
	bool Busy(void);

	bool SetVolume(int p_volume);
	bool GetVolume(int& p_volume);

	bool ListVoices(NarratorGender p_gender, NarratorListVoicesCallback p_callback, void *p_context);
	bool SetVoice(const char* p_voice);

	bool SetSpeed(int p_speed);
	bool GetSpeed(int& p_speed);

	bool SetPitch(int p_pitch);
	bool GetPitch(int& p_pitch);

private:
	bool bInited;
	// Last Error Message
	char m_strLastError[256];

	// Process events to track speech output status
	bool ProcessEvents(void);

	// Internal Error Message
	void Error( WCHAR* pText ){wcscpy((WCHAR *)m_strLastError, pText); };
	void Error( WCHAR* pText, HRESULT hr){ sprintf(m_strLastError,"%s Error Code:0x%x", pText, hr); };

	// Pointer to our tts voice	
	CComPtr<ISpVoice> m_cpVoice;		

	bool m_bUseDefaultPitch;
	short m_Npitch;
};

////////////////////////////////////////////////////////////////////////////////

class CSpDynamicString
{
public:
	CSpDynamicString()
	{
		m_ptr = NULL;
	}

	~CSpDynamicString()
	{
		if (m_ptr != NULL)
			CoTaskMemFree(m_ptr);
	}

	WCHAR** operator &(void)
	{
		return &m_ptr;
	}

	operator WCHAR* (void)
	{
		return m_ptr;
	}

protected:
	WCHAR *m_ptr;
};

#endif
