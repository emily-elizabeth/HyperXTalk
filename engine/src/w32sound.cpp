#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "param.h"
#include "mcerror.h"

#include "util.h"
#include "object.h"

#include "w32dc.h"

int4 MCScreenDC::getsoundvolume(void)
{
	return 0;
}

void MCScreenDC::setsoundvolume(int4 p_volume)
{
}

void MCScreenDC::startplayingsound(IO_handle p_stream, MCObject *p_callback, bool p_next, int p_volume)
{
}

void MCScreenDC::stopplayingsound(void)
{
}
