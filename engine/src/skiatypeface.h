#ifndef __MC_SKIA_TYPEFACE__
#define __MC_SKIA_TYPEFACE__

struct __MCSkiaTypeface;
typedef __MCSkiaTypeface *MCSkiaTypefaceRef;

struct MCSkiaFont
{
	uint32_t size;
	MCSkiaTypefaceRef typeface;
};

bool MCSkiaTypefaceCreateWithData(MCDataRef p_data, MCSkiaTypefaceRef &r_typeface);
bool MCSkiaTypefaceCreateWithName(MCStringRef p_name, bool p_bold, bool p_italic, MCSkiaTypefaceRef &r_typeface);
void MCSkiaTypefaceRelease(MCSkiaTypefaceRef p_typeface);
bool MCSkiaTypefaceGetMetrics(MCSkiaTypefaceRef p_typeface, uint32_t p_size, float &r_ascent, float &r_descent, float& r_leading, float& r_xheight);
bool MCSkiaTypefaceMeasureText(MCSkiaTypefaceRef p_typeface, uint32_t p_size, const char *p_text, uint32_t p_text_length, bool p_utf16, float &r_length);

#endif
