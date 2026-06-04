///////////////////////////////////////////////////////////////////////////////
// Unix support function definitions

#ifndef DARWIN
#include <sys/time.h>
#endif

#ifdef SELECT

#ifndef LINUX
#include <sys/select.h>
#include <sys/stream.h>
#endif

#else

#ifndef DARWIN
#include <poll.h>
#endif

#endif

#include <ctype.h>
#include <dlfcn.h>

#if defined DARWIN
#define VXCMD_STRING "VXCMD_macho"
#else
#define VXCMD_STRING "VXCMD"
#endif

#include "revdb.h"

void MCU_path2std(char *p_path);
void MCU_path2native(char *p_path);
void MCU_fix_path(char *cstr);
char *MCS_getcurdir(void);
char *MCS_resolvepath(const char *path);

#include <pwd.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
