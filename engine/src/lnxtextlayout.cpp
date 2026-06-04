#include "lnxprefix.h"

#include "globdefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "filedefs.h"

#include "util.h"
#include "globals.h"

#include "textlayout.h"
#include "lnxflst.h"

bool MCTextLayoutInitialize(void)
{
	return true;
}

void MCTextLayoutFinalize(void)
{
}

bool MCTextLayout(const unichar_t *p_chars, uint32_t p_char_count, MCFontStruct *p_font, MCTextLayoutCallback p_callback, void *p_context)
{
	return MCFontlistGetCurrent() -> ctxt_layouttext(p_chars, p_char_count, p_font, p_callback, p_context);
}
