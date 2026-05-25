#include "prefix.h"

#include "objdefs.h"

/* ----------------------------------------------------------------
 * Functions implemented in em-event.js
 * ---------------------------------------------------------------- */

extern "C" bool MCEmscriptenEventInitializeJS(void);
extern "C" void MCEmscriptenEventFinalizeJS(void);

/* ----------------------------------------------------------------
 * Initialisation / finalisation
 * ---------------------------------------------------------------- */

bool
MCEmscriptenEventInitialize()
{
	return MCEmscriptenEventInitializeJS();
}

void
MCEmscriptenEventFinalize()
{
	MCEmscriptenEventFinalizeJS();
}

/* ---------------------------------------------------------------- */

extern "C" MC_DLLEXPORT_DEF uint32_t
MCEmscriptenEventEncodeModifiers(bool p_shift,
                                 bool p_alt,
                                 bool p_ctrl,
                                 bool p_meta)
{
	uint32_t t_result = 0;

	if (p_shift) t_result |= MS_SHIFT;
	if (p_alt)   t_result |= MS_ALT;
	if (p_ctrl)  t_result |= MS_CONTROL;
	if (p_meta)  t_result |= MS_MOD2;    /* Mac "command" key */

	return t_result;
}
