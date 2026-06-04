#ifndef __MC_SERVER_MAIN__
#define __MC_SERVER_MAIN__

#ifndef __MC_OSSPEC__
#include "osspec.h"
#endif

class MCServerScript;

extern MCStringRef MCserverinitialscript;
extern MCServerScript *MCserverscript;
extern MCSErrorMode MCservererrormode;
extern MCSOutputTextEncoding MCserveroutputtextencoding;
extern MCSOutputLineEndings MCserveroutputlineendings;

extern MCStringRef MCsessionsavepath;
extern MCStringRef MCsessionname;
extern MCStringRef MCsessionid;
extern uint32_t MCsessionlifetime;

extern char **MCservercgiheaders;
extern uint32_t MCservercgiheadercount;

typedef struct mcservercookie_t
{
	char *name;
	char *value;
	char *path;
	char *domain;
	uint32_t expires;
	bool secure;
	bool http_only;
} MCServerCookie;

extern MCServerCookie *MCservercgicookies;
extern uint32_t MCservercgicookiecount;

extern MCStringRef MCservercgidocumentroot;

#endif
