#ifndef _MODULE_ENGINE_H_
#define _MODULE_ENGINE_H_

#include <foundation.h>

////////////////////////////////////////////////////////////////////////////////

typedef struct __MCScriptObject *MCScriptObjectRef;

extern "C"
{
    extern MC_DLLEXPORT MCTypeInfoRef kMCEngineScriptObjectTypeInfo;

	extern MC_DLLEXPORT MCTypeInfoRef kMCEngineScriptObjectDoesNotExistErrorTypeInfo;
	extern MC_DLLEXPORT MCTypeInfoRef kMCEngineScriptObjectNoContextErrorTypeInfo;

    extern MC_DLLEXPORT MCArrayRef MCEngineExecDescribeScriptOfScriptObject(MCScriptObjectRef p_object, bool p_include_all = true);
    
    extern MC_DLLEXPORT void MCEngineRunloopBreakWait(void);
}

bool MCEngineScriptObjectCreate(MCObject *p_object, uint32_t p_part_id, MCScriptObjectRef& r_object);

void MCEngineScriptObjectPreventAccess(void);

void MCEngineScriptObjectAllowAccess(void);

MCObject* MCEngineCurrentContextObject(void);

////////////////////////////////////////////////////////////////////////////////

#endif
