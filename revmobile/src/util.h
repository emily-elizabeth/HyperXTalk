#ifndef __UTIL__
#define __UTIL__

#ifndef __MC_EXTERNAL__
#include "external.h"
#endif

#ifndef nil
#define nil 0
#endif

bool Throw(const char *p_message);
void Catch(MCVariableRef result);
bool CheckError(MCError p_error);
bool VariableFormat(MCVariableRef var, const char *p_format, ...);
bool VariableAppendFormat(MCVariableRef var, const char *p_format, ...);

#endif
