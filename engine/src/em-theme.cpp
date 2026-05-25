#include "prefix.h"

#include "objdefs.h"
#include "font.h"
#include "platform.h"
#include "mctheme.h"

MCTheme *
MCThemeCreateNative()
{
	return nil;
}


/* ================================================================
 * Platform theming
 * ================================================================ */

/* FIXME not yet implemented */

bool
MCPlatformGetControlThemePropColor(MCPlatformControlType p_type,
                                   MCPlatformControlPart p_part,
                                   MCPlatformControlState p_state,
                                   MCPlatformThemeProperty p_prop,
                                   MCColor& r_color)
{
	return false;
}

bool
MCPlatformGetControlThemePropFont(MCPlatformControlType p_type,
                                  MCPlatformControlPart p_part,
                                  MCPlatformControlState p_state,
                                  MCPlatformThemeProperty p_prop,
                                  MCFontRef& r_font)
{
	/* For now, ask for the compiled-in default font name and size*/
	return MCFontCreate(MCNAME(DEFAULT_TEXT_FONT), 0,
	                    DEFAULT_TEXT_SIZE, r_font);
}
