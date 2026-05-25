#include "prefix.h"

#include "parsedef.h"
#include "filedefs.h"
#include "osspec.h"

#include "globals.h"
#include "variable.h"

#include "exec.h"

char *MCS_getcurdir(void)
{
	MCAutoStringRef t_current;
	char *t_cstring;
    MCS_getcurdir(&t_current);
	if (MCStringGetLength(*t_current) &&
        MCStringConvertToCString(*t_current, t_cstring))
		return t_cstring;
	return NULL;
}

char *MCS_resolvepath(const char* p_path)
{
	MCAutoStringRef t_path;
	MCAutoStringRef t_resolved;
	char *t_cstring;

	if (MCStringCreateWithCString(p_path, &t_path) && MCS_resolvepath(*t_path, &t_resolved) && MCStringConvertToCString(*t_resolved, t_cstring))
		return t_cstring;

	return NULL;
}

const char *MCS_tmpnam()
{
	// MW-2008-06-19: Make sure fname is stored in a static to keep the (rather
	//   unpleasant) current semantics of the call.
	static char *fname;
	if (NULL != fname)
	{
		delete fname;
		fname = NULL;
	}

	MCAutoStringRef t_tmpname;
	MCS_tmpnam(&t_tmpname);
    /* UNCHECKED */ MCStringConvertToCString(*t_tmpname, fname);

	return fname;
}

#ifdef LEGACY_SPEC
void MCS_getspecialfolder(MCExecPoint &ep)
{
	MCExecContext ctxt(ep);
	MCNewAutoNameRef t_path;
	MCAutoStringRef t_special_folder_path;
	/* UNCHECKED */ ep.copyasnameref(&t_path);
	if (!MCS_getspecialfolder(*t_path, &t_special_folder_path))
	{
		MCresult->sets("folder not found");
		ep.clear();
	}
	/* UNCHECKED */ ep.setvalueref(*t_special_folder_path);
}
#endif

Boolean MCS_exists(const char *path, Boolean file)
{
	MCAutoStringRef t_string;
	/* UNCHECKED */ MCStringCreateWithCString(path, &t_string);

	return MCS_exists(*t_string, file == True);
}
