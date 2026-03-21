#ifndef _HAD_ZIPINTW32_H
#define _HAD_ZIPINTW32_H

// SN-2015-03-10: [[ Bug 14413 ]] We need few types (WCHAR)
// and function from the Windows API
#include <windows.h>
#include <WinNT.h>

#include "zip.h"
#include "zipint.h"

////////////////////////////////////////////////////////////////////////////////

typedef unsigned short mode_t;

#define fseeko fseek
#define strcasecmp _stricmp
#define fdopen _fdopen
#define strdup _strdup
#define fileno _fileno

////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

extern WCHAR* ConvertCStringToLpwstr(const char *p_input);

#ifdef __cplusplus
}
#endif

#endif /* _HAD_ZIPINTW32_H */