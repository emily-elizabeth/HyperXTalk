#include "revpdfprinter.h"

#include <cairo-quartz.h>
#include <CoreFoundation/CFDate.h>
#include <CoreText/CoreText.h>

bool MCPDFPrintingDevice::create_cairo_font_from_custom_printer_font(const MCCustomPrinterFont &p_cp_font, cairo_font_face_t* &r_cairo_font)
{
	bool t_success = true;
	
	CGFontRef t_cg_font;
	t_cg_font = CTFontCopyGraphicsFont((CTFontRef)p_cp_font . handle, NULL);
	
	cairo_font_face_t *t_font;
	t_font = cairo_quartz_font_face_create_for_cgfont(t_cg_font);
	t_success = (m_status = cairo_font_face_status(t_font)) == CAIRO_STATUS_SUCCESS;
	if (t_success)
		r_cairo_font = t_font;
	
	CGFontRelease(t_cg_font);
	
	return t_success;
}



// SN-2014-12-23: [[ Bug 14278 ]] Added system-specific to get the path.
bool MCPDFPrintingDevice::get_filename(const char* p_utf8_path, char *& r_system_path)
{
    return MCCStringClone(p_utf8_path, r_system_path);
}
