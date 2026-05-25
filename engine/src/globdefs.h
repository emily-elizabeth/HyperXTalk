#ifndef __GLOBDEFS_H
#define __GLOBDEFS_H

////////////////////////////////////////////////////////////////////////////////

#if defined(TARGET_PLATFORM_WINDOWS)
#define _DESKTOP
#define _WINDOWS_DESKTOP
#ifndef _WINDOWS
#define _WINDOWS
#endif
#elif defined(TARGET_PLATFORM_MACOS_X)
#define _DESKTOP
#define _MAC_DESKTOP
#ifndef _MACOSX
#define _MACOSX
#endif
#elif defined(TARGET_PLATFORM_LINUX)
#define _DESKTOP
#define _LINUX_DESKTOP
#ifndef _LINUX
#define _LINUX
#endif
#elif defined(TARGET_PLATFORM_MOBILE)

#ifndef _MOBILE
#define _MOBILE
#endif

#if defined(TARGET_SUBPLATFORM_IPHONE)
#define _IOS_MOBILE
#elif defined(TARGET_SUBPLATFORM_ANDROID)
#define _ANDROID_MOBILE
#endif

#endif

////////////////////////////////////////////////////////////////////////////////

#include "sysdefs.h"

////////////////////////////////////////////////////////////////////////////////

#ifndef __MCUTILITY_H
#include "mcutility.h"
#endif

////////////////////////////////////////////////////////////////////////////////

#endif
