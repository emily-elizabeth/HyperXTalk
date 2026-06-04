#include "prefix.h"

#include "em-util.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "exec.h"
#include "stack.h"
#include "object.h"
#include "param.h"
#include "sysdefs.h"
#include "osspec.h"
#include "globals.h"

/* ================================================================
 * Locales
 * ================================================================ */

MCLocaleRef
MCS_getsystemlocale()
{
	/* Emscripten locale is basically C.UTF-8.  There's no standard
	 * way to get the user's desired locale from ECMAScript,
	 * either. */
	MCLocaleRef t_locale;
	/* UNCHECKED */ MCLocaleCreateWithName(MCSTR("C"), t_locale);
	return t_locale;
}


bool
MCS_put(MCExecContext &ctxt, MCSPutKind p_kind, MCStringRef p_string)
{
	bool t_success;
	switch (p_kind)
	{
	case kMCSPutBeforeMessage:
        // SN-2014-04-11 [[ FasterVariable ]] parameter updated to use the new 'set' operation on variables
        t_success = MCmb -> set(ctxt, p_string, kMCVariableSetBefore);
        break;
    case kMCSPutOutput:
    case kMCSPutIntoMessage:
		t_success = MCmb -> set(ctxt, p_string);
		break;
	case kMCSPutAfterMessage:
		// SN-2014-04-11 [[ FasterVariable ]] parameter updated to use the new 'set' operation on variables
		t_success = MCmb -> set(ctxt, p_string, kMCVariableSetAfter);
		break;
	default:
		t_success = false;
		break;
	}

	// MW-2012-02-23: [[ PutUnicode ]] If we don't understand the kind
	//   then return false (caller can then throw an error).
	return t_success;
}
