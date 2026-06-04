#include <windows.h>

#include "revspeech.h"

#include "w32sapi5speech.h"

INarrator *InstantiateNarrator(NarratorProvider p_provider)
{
	return new WindowsSAPI5Narrator();
}

extern "C" BOOL WINAPI DllMain(HINSTANCE tInstance, DWORD dwReason, LPVOID)
{
	return TRUE;
}
