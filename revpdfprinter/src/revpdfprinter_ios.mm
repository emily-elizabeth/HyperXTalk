#include "revpdfprinter.h"

#include <cairo-quartz.h>
#include <CoreFoundation/CFDate.h>

#ifdef __IPHONE_3_2
#include <CoreText/CoreText.h>
#endif

bool MCPDFPrintingDevice::create_cairo_font_from_custom_printer_font(const MCCustomPrinterFont &p_cp_font, cairo_font_face_t* &r_cairo_font)
{
#ifdef __IPHONE_3_2
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
#else
	return false;
#endif
}

