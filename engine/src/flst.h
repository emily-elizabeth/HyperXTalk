#ifndef __MC_FONT_LIST__
#define __MC_FONT_LIST__

// MM-2013-09-13: [[ RefactorGraphics ]] Updated to include platform specific font lists for server font support.
#if defined(_WINDOWS_DESKTOP) || defined(_WINDOWS_SERVER)
#include "w32flst.h"
#elif defined(_MAC_DESKTOP) || defined(_MAC_SERVER)
#include "osxflst.h"
#elif defined(_LINUX_DESKTOP) || defined(_LINUX_SERVER)
#include "lnxflst.h"
#elif defined(_SERVER)
#include "srvflst.h"
#elif defined(_MOBILE)
#include "mblflst.h"
#elif defined(__EMSCRIPTEN__)
#include "em-fontlist.h"
#endif

#endif
