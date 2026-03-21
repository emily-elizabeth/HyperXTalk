#ifndef SkTypeface_FreeType_DEFINED
#define SkTypeface_FreeType_DEFINED

#include "SkTypeface.h"

#include <ft2build.h>
#include FT_FREETYPE_H

/**
 * Obtain access to the underlying FT_Face of a SkTypeface.
 * The FT_Face should be released with FT_Done_Face once finished.
 **/
SK_API extern FT_Face SkTypeface_GetFTFace(const SkTypeface* face);

#endif  // SkTypeface_FreeType_DEFINED
