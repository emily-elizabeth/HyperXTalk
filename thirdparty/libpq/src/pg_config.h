#if defined(WIN32)
#include "pg_config.win32.h"
#elif defined(_LINUX)
#include "pg_config.linux.h"
#elif defined(_MACOSX)
#include "pg_config.mac.h"
#endif

#ifndef FRONTEND
#define FRONTEND 1
#endif

#define USE_SSL 1
#define HAVE_SSL_GET_CURRENT_COMPRESSION 1