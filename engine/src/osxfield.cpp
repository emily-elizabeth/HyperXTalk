#include "osxprefix.h"
#include <CoreText/CoreText.h>

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "uidc.h"
#include "field.h"
#include "paragraf.h"
#include "cdata.h"
#include "mcerror.h"

#include "exec.h"
#include "util.h"
#include "MCBlock.h"

#include "globals.h"

#include "text.h"


bool MCField::macmatchfontname(const char *p_font_name, char p_derived_font_name[])
{
    // Create a CTFont using the given name. It performs fall-back processing of
    // the name (which we had to previously do by hand for ATSUI): it checks for
    // a font with that exact name, then family name, etc.
    //
    // The size and transform matrix here are arbitrary. We just want to know
    // that the font exists.
    //
    CTFontRef t_font_ref = nil;
    CFStringRef t_font_name = CFStringCreateWithCString(kCFAllocatorDefault, p_font_name, kCFStringEncodingMacRoman);
    if (t_font_name != nil)
    {
        t_font_ref = CTFontCreateWithName(t_font_name, 12, &CGAffineTransformIdentity);
        CFRelease(t_font_name);
    }
    
    // Get the proper name for the font as well as its style name
    CFStringRef t_font_full_name = nil;
    if (t_font_ref != nil)
    {
        t_font_full_name = CTFontCopyFullName(t_font_ref);
        CFRelease(t_font_ref);
    }
    
    // Check for various stylistic variants at the end of the font name
    CFStringRef t_real_font_name = nil;
    if (t_font_full_name != nil)
    {
        CFIndex t_length = CFStringGetLength(t_font_full_name);
        if (CFStringHasSuffix(t_font_full_name, CFSTR("Bold Italic")))
            t_real_font_name = CFStringCreateWithSubstring(kCFAllocatorDefault, t_font_full_name, CFRangeMake(0, t_length-12));
        else if (CFStringHasSuffix(t_font_full_name, CFSTR("Bold")))
            t_real_font_name = CFStringCreateWithSubstring(kCFAllocatorDefault, t_font_full_name, CFRangeMake(0, t_length-5));
        else if (CFStringHasSuffix(t_font_full_name, CFSTR("Italic")))
            t_real_font_name = CFStringCreateWithSubstring(kCFAllocatorDefault, t_font_full_name, CFRangeMake(0, t_length-7));
        else
            t_real_font_name = (CFStringRef)CFRetain(t_font_full_name);
        CFRelease(t_font_full_name);
    }
    
    if (t_real_font_name != nil)
    {
        bool t_success;
        t_success = CFStringGetCString(t_real_font_name, p_derived_font_name, 256, kCFStringEncodingMacRoman);
        CFRelease(t_real_font_name);
        return t_success;
    }
    
    return false;
}
