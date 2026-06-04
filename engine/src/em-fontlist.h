#ifndef __MC_EMSCRIPTEN_FONTLIST_H__
#define __MC_EMSCRIPTEN_FONTLIST_H__

#include <foundation.h>

#include "sysdefs.h"

class MCFontnode;

class MCFontlist
{
public:
	MCFontlist();
	~MCFontlist();
	MCFontStruct *getfont(MCNameRef p_name,
	                      uint16_t p_size,
	                      uint16_t p_style,
	                      Boolean p_for_printer);
	bool getfontnames(MCStringRef p_type, MCListRef & r_names);
	bool getfontsizes(MCStringRef p_fname, MCListRef & r_sizes);
	bool getfontstyles(MCStringRef p_fname,
	                   uint16_t p_size,
	                   MCListRef & r_styles);
	bool getfontstructinfo(MCNameRef & r_name,
	                       uint16_t & r_size,
	                       uint16_t & r_style,
	                       Boolean & r_for_printer,
	                       MCFontStruct * p_font);

protected:
	MCFontnode *m_font_list;
};

#endif /* ! __MC_EMSCRIPTEN_FONTLIST_H__ */
