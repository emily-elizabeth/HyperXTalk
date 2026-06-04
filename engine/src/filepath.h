#ifndef __FILEPATH_H__
#define __FILEPATH_H__

#include "foundation.h"

extern bool MCPathIsRemoteURL(MCStringRef p_path);
extern bool MCPathIsAbsolute(MCStringRef p_path);
extern bool MCPathAppend(MCStringRef p_base, MCStringRef p_path, MCStringRef &r_new);

#endif//__FILEPATH_H__
