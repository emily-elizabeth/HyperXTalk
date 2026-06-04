#include "prefix.h"

#include "textlayout.h"

#include <CoreText/CoreText.h>

////////////////////////////////////////////////////////////////////////////////

int32_t MCCustomPrinterComputeFontSize(void *font)
{
	return CTFontGetSize((CTFontRef)font);
}

////////////////////////////////////////////////////////////////////////////////

bool MCSystemRequestPermission(MCStringRef p_permission, bool& r_granted)
{
    // Not implemented
    return false;
}

bool MCSystemPermissionExists(MCStringRef p_permission, bool& r_exists)
{
    // Not implemented
    return false;
}

bool MCSystemHasPermission(MCStringRef p_permission, bool& r_permission_granted)
{
    // Not implemented
    return false;
}

////////////////////////////////////////////////////////////////////////////////
