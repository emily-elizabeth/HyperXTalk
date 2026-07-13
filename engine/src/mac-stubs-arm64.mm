// ARM64 stubs for functions defined in Carbon-era files excluded on arm64

#if defined(__arm64__) || defined(__aarch64__)

#import <AppKit/AppKit.h>
#include <math.h>

#include "osxprefix.h"
#include "globdefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "filedefs.h"
#include "mcstring.h"
#include "globals.h"
#include "object.h"
#include "stack.h"
#include "font.h"
#include "mctheme.h"
#include "printer.h"
#include "image.h"
#include "uidc.h"
#include "osxflst.h"
#include "mode.h"
#include "redraw.h"
#include "context.h"
#include "graphics_util.h"
#include "util.h"
#define _MAC_DESKTOP
#include "hc.h"
#include "exec.h"

// ── Dark-mode helper (callable from plain C++) ─────────────────────────
// Returns true when the app is currently running in dark Aqua.
// Exposed as extern "C" so that C++ translation units (e.g. buttondraw.cpp)
// can call it without Objective-C includes.
extern "C" bool MCplatformIsDarkMode(void)
{
    NSAppearanceName t_best =
        [[NSApp effectiveAppearance]
            bestMatchFromAppearancesWithNames:
                @[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]];
    return [t_best isEqualToString:NSAppearanceNameDarkAqua];
}

// Returns the current windowBackgroundColor as a LiveCode-style hex string
// ("#rrggbb") written into the caller-supplied buffer (at least 8 bytes).
// Resolves the colour inside the app's effective appearance so the value
// is correct for whichever light/dark mode is currently active.
extern "C" void MCplatformGetWindowBackgroundColor(char *p_buf, size_t p_buflen)
{
    if (p_buf == nullptr || p_buflen < 8)
        return;

    __block CGFloat r = 0.0, g = 0.0, b = 0.0;
    [[NSApp effectiveAppearance] performAsCurrentDrawingAppearance:^{
        NSColor *t_color =
            [[NSColor windowBackgroundColor]
                colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
        if (t_color != nil)
        {
            r = [t_color redComponent];
            g = [t_color greenComponent];
            b = [t_color blueComponent];
        }
    }];

    // Clamp and format as #rrggbb.
    auto clamp = ^(CGFloat v){ return (int)(v < 0.0 ? 0 : v > 1.0 ? 255 : v * 255.0 + 0.5); };
    snprintf(p_buf, p_buflen, "#%02x%02x%02x", clamp(r), clamp(g), clamp(b));
}

// Returns the current labelColor (primary text colour) as a LiveCode-style hex
// string ("#rrggbb") written into the caller-supplied buffer (at least 8 bytes).
extern "C" void MCplatformGetLabelColor(char *p_buf, size_t p_buflen)
{
    if (p_buf == nullptr || p_buflen < 8)
        return;

    __block CGFloat r = 0.0, g = 0.0, b = 0.0;
    [[NSApp effectiveAppearance] performAsCurrentDrawingAppearance:^{
        NSColor *t_color =
            [[NSColor labelColor]
                colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
        if (t_color != nil)
        {
            r = [t_color redComponent];
            g = [t_color greenComponent];
            b = [t_color blueComponent];
        }
    }];

    auto clamp = ^(CGFloat v){ return (int)(v < 0.0 ? 0 : v > 1.0 ? 255 : v * 255.0 + 0.5); };
    snprintf(p_buf, p_buflen, "#%02x%02x%02x", clamp(r), clamp(g), clamp(b));
}

// ── ARM64 MCThemeDrawInfo ───────────────────────────────────────────────
// Replaces the Carbon/HITheme-based version in osxtheme.h (which is
// excluded on ARM64).  Only drawwidget() and MCThemeDraw() in this file
// ever populate or read this struct, so the layout only has to be
// self-consistent here.
struct MCThemeDrawInfo
{
    MCRectangle dest;          // target widget bounds
    Widget_Type widget_type;   // which widget to draw
    Widget_Part part;          // which part of the widget
    uint32_t    state;         // WTHEME_STATE_* bitmask
    uint32_t    attributes;    // WTHEME_ATT_* bitmask
    union
    {
        // Tab button: is_first/is_last control which edges are rounded.
        struct { bool is_first; bool is_last; } tab;
        // Scrollbar, slider, and progress bar all share this layout.
        struct
        {
            double startvalue;   // min value
            double thumbpos;     // current value (scroll pos or progress)
            double endvalue;     // max value
            double thumbsize;    // thumb page-size (0 for progress)
            bool   horizontal;   // true = horizontal orientation
        } scrollbar;
    };
};

// ── Forward declarations ────────────────────────────────────────────────
extern CGBitmapInfo MCGPixelFormatToCGBitmapInfo(uint32_t p_pixel_format, bool p_alpha);
extern bool         MCMacPlatformGetImageColorSpace(CGColorSpaceRef &r_colorspace);

// ── Shared dummy view for NSCell / NSView drawing ───────────────────────
// NSCell's drawWithFrame:inView: requires a non-nil NSView on some macOS
// versions.  NSView subclasses (NSScroller, NSProgressIndicator) need a
// window to obtain drawing attributes.  We keep a permanently allocated
// off-screen window/view pair.
static NSView   *s_dummy_view   = nil;
static NSWindow *s_dummy_window = nil;

static NSView *GetDummyView(void)
{
    if (s_dummy_view == nil)
    {
        // Off-screen borderless window — never shown, but gives NSViews a
        // proper backing store, appearance, and colorspace.
        // defer:NO (not defer:YES) is critical: a deferred window never
        // receives a proper window-server connection, so NSCell drawing
        // falls back to the old Aqua gradient style instead of the modern
        // flat appearance.
        s_dummy_window = [[NSWindow alloc]
                initWithContentRect:NSMakeRect(-16000, -16000, 4096, 4096)
                          styleMask:NSWindowStyleMaskBorderless
                            backing:NSBackingStoreBuffered
                              defer:NO];
        s_dummy_view = [s_dummy_window contentView];   // retained by window
    }
    return s_dummy_view;
}

// ── MCNativeTheme — full AppKit Aqua implementation ────────────────────
class MCNativeTheme : public MCTheme
{
public:
    virtual Boolean load()           { return True; }

    // LF_NATIVEMAC enables the IsMacLFAM() path in button/scrollbar drawing.
    virtual uint2   getthemeid()       { return LF_NATIVEMAC; }
    // LF_MAC keeps IsMacLF() true (general Mac code-paths).
    virtual uint2   getthemefamilyid() { return LF_MAC; }

    // Tell the metacontext how large to make the per-draw buffer.
    virtual uint32_t getthemedrawinfosize() { return sizeof(MCThemeDrawInfo); }

    virtual Boolean getthemepropbool(Widget_ThemeProps p)
    {
        // Mirror the x86 MCNativeTheme behaviour from osxtheme.mm.
        if (p == WTHEME_PROP_DRAWTABPANEFIRST)      return true;
        if (p == WTHEME_PROP_TABSELECTONMOUSEUP)    return true;
        if (p == WTHEME_PROP_TABBUTTONSOVERLAPPANE) return true;
        return False;
    }

    virtual int4 getmetric(Widget_Metric m)
    {
        switch (m)
        {
            case WTHEME_METRIC_TABOVERLAP:             return -1;
            case WTHEME_METRIC_TABRIGHTMARGIN:         return 11;
            case WTHEME_METRIC_TABLEFTMARGIN:          return 12;
            case WTHEME_METRIC_TABNONSELECTEDOFFSET:   return 0;
            case WTHEME_METRIC_COMBOSIZE:              return 22;
            case WTHEME_METRIC_OPTIONBUTTONARROWSIZE:  return 21;
            case WTHEME_METRIC_TABBUTTON_HEIGHT:       return 21;
            default: return 0;
        }
    }

    // Return True for ALL widget types — exactly what x86 osxtheme.mm does
    // (its default: case is "return True").  Returning False would cause the
    // software fallback in scrollbardraw.cpp / buttondraw.cpp to run, which
    // requires IsMacEmulatedLF() == true (only when MCcurtheme == NULL), so
    // it would produce black boxes instead of any visible widget.
    virtual Boolean iswidgetsupported(Widget_Type wtype)
    {
        return True;
    }

    virtual Widget_Part hittest(const MCWidgetInfo &winfo, int2 mx, int2 my, const MCRectangle &drect) override
    {
        // For stepper (small scrollbar) buttons, determine which arrow was clicked
        if (winfo.type == WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_UP ||
            winfo.type == WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_DOWN ||
            winfo.type == WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_LEFT ||
            winfo.type == WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_RIGHT)
        {
            if (MCU_point_in_rect(drect, mx, my))
            {
                switch (winfo.type)
                {
                    case WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_UP:
                        return WTHEME_PART_ARROW_INC;
                    case WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_DOWN:
                        return WTHEME_PART_ARROW_DEC;
                    case WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_LEFT:
                        return WTHEME_PART_ARROW_INC;
                    case WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_RIGHT:
                        return WTHEME_PART_ARROW_DEC;
                    default:
                        return WTHEME_PART_ALL;
                }
            }
            return WTHEME_PART_UNDEFINED;
        }
        
        // For whole stepper control, determine which arrow button was clicked
        if (winfo.type == WTHEME_TYPE_SMALLSCROLLBAR)
        {
            bool t_vertical = (drect.height > drect.width);
            
            if (t_vertical) {
                // Top half = increment (UP arrow), bottom half = decrement (DOWN arrow)
                int t_mid = drect.y + drect.height / 2;
                if (my >= drect.y && my < t_mid) {
                    return WTHEME_PART_ARROW_INC;
                } else if (my >= t_mid && my <= drect.y + drect.height) {
                    return WTHEME_PART_ARROW_DEC;
                }
            } else {
                // Left half = increment (LEFT arrow), right half = decrement (RIGHT arrow)
                int t_mid = drect.x + drect.width / 2;
                if (mx >= drect.x && mx < t_mid) {
                    return WTHEME_PART_ARROW_INC;
                } else if (mx >= t_mid && mx <= drect.x + drect.width) {
                    return WTHEME_PART_ARROW_DEC;
                }
            }
            return WTHEME_PART_UNDEFINED;
        }
        
        // For scrollbars and sliders, determine which part was clicked
        bool t_is_scrollbar = (winfo.type == WTHEME_TYPE_SCROLLBAR);
        bool t_is_slider = (winfo.type == WTHEME_TYPE_SLIDER ||
                           winfo.type == WTHEME_TYPE_SLIDER_TRACK_HORIZONTAL ||
                           winfo.type == WTHEME_TYPE_SLIDER_TRACK_VERTICAL ||
                           winfo.type == WTHEME_TYPE_SLIDER_THUMB_HORIZONTAL ||
                           winfo.type == WTHEME_TYPE_SLIDER_THUMB_VERTICAL);
        
        if (t_is_scrollbar || t_is_slider)
        {
            // Use default behavior if we can't determine specific parts
            if (winfo.datatype != WTHEME_DATA_SCROLLBAR || winfo.data == nil)
            {
                return WTHEME_PART_THUMB;
            }
            
            MCWidgetScrollBarInfo *sb = (MCWidgetScrollBarInfo *)winfo.data;
            
            // Guard against invalid values
            if (sb->endvalue <= sb->startvalue)
            {
                return WTHEME_PART_THUMB;
            }
            
            bool t_horizontal = (drect.width > drect.height);
            
            CGFloat t_length = t_horizontal ? drect.width : drect.height;
            CGFloat t_thickness = t_horizontal ? drect.height : drect.width;
            
            // For sliders, use fixed thumb size (14px radius = 28px diameter)
            // For scrollbars, use the thumbsize from the info
            CGFloat t_thumb_len;
            if (t_is_slider)
            {
                t_thumb_len = 28.0f; // Fixed slider thumb size
            }
            else
            {
                // Check if there's content to scroll - if not, return track part
                if (sb->thumbsize <= 0.0)
                {
                    if (t_horizontal) {
                        return (mx < drect.x + drect.width / 2) ? WTHEME_PART_TRACK_DEC : WTHEME_PART_TRACK_INC;
                    } else {
                        return (my < drect.y + drect.height / 2) ? WTHEME_PART_TRACK_DEC : WTHEME_PART_TRACK_INC;
                    }
                }
                t_thumb_len = (CGFloat)(sb->thumbsize / (sb->endvalue - sb->startvalue)) * t_length;
                if (t_thumb_len < 16.0f) t_thumb_len = 16.0f; // Minimum thumb size to match visual appearance
            }
            
            double t_range = sb->endvalue - sb->startvalue;
            CGFloat t_thumb_x;
            
            if (t_is_slider)
            {
                // Slider: position is fraction of total range
                CGFloat t_norm = (CGFloat)((sb->thumbpos - sb->startvalue) / t_range);
                t_thumb_x = t_norm * (t_length - t_thumb_len);
            }
            else
            {
                // Scrollbar: subtract thumb size from scrollable range
                double t_scrollable = t_range - sb->thumbsize;
                if (sb->thumbsize <= 0.0)
                {
                    // No content - return track part
                    if (t_horizontal) {
                        return (mx < drect.x + drect.width / 2) ? WTHEME_PART_TRACK_DEC : WTHEME_PART_TRACK_INC;
                    } else {
                        return (my < drect.y + drect.height / 2) ? WTHEME_PART_TRACK_DEC : WTHEME_PART_TRACK_INC;
                    }
                }
                if (t_scrollable > 0)
                {
                    CGFloat t_norm = (CGFloat)((sb->thumbpos - sb->startvalue) / t_scrollable);
                    t_thumb_x = t_norm * (t_length - t_thumb_len);
                }
                else
                {
                    t_thumb_x = 0;
                }
            }
            
            // Check if mouse is in thumb area or track
            if (t_horizontal) {
                if (mx >= drect.x + t_thumb_x && mx <= drect.x + t_thumb_x + t_thumb_len &&
                    my >= drect.y && my <= drect.y + t_thickness) {
                    return WTHEME_PART_THUMB;
                }
                // For sliders, return thumb to enable dragging anywhere
                if (t_is_slider) {
                    return WTHEME_PART_THUMB;
                }
                // For scrollbars, return track part based on position relative to thumb
                if (mx < drect.x + t_thumb_x) {
                    return WTHEME_PART_TRACK_DEC;
                } else {
                    return WTHEME_PART_TRACK_INC;
                }
            } else {
                if (mx >= drect.x && mx <= drect.x + t_thickness &&
                    my >= drect.y + t_thumb_x && my <= drect.y + t_thumb_x + t_thumb_len) {
                    return WTHEME_PART_THUMB;
                }
                // For sliders, return thumb to enable dragging anywhere
                if (t_is_slider) {
                    return WTHEME_PART_THUMB;
                }
                // For scrollbars, return track part based on position relative to thumb
                if (my < drect.y + t_thumb_x) {
                    return WTHEME_PART_TRACK_DEC;
                } else {
                    return WTHEME_PART_TRACK_INC;
                }
            }
        }
        
        // For other widgets, use default behavior
        return MCU_point_in_rect(drect, mx, my) ? WTHEME_PART_ALL : WTHEME_PART_UNDEFINED;
    }

    virtual void getwidgetrect(const MCWidgetInfo &winfo, Widget_Metric wmetric, const MCRectangle &srect, MCRectangle &drect) override
    {
        // For scrollbars and sliders, compute the thumb rect
        bool t_is_scrollbar = (winfo.type == WTHEME_TYPE_SCROLLBAR || winfo.type == WTHEME_TYPE_SMALLSCROLLBAR);
        bool t_is_slider = (winfo.type == WTHEME_TYPE_SLIDER ||
                           winfo.type == WTHEME_TYPE_SLIDER_TRACK_HORIZONTAL ||
                           winfo.type == WTHEME_TYPE_SLIDER_TRACK_VERTICAL ||
                           winfo.type == WTHEME_TYPE_SLIDER_THUMB_HORIZONTAL ||
                           winfo.type == WTHEME_TYPE_SLIDER_THUMB_VERTICAL);
        
        if ((t_is_scrollbar || t_is_slider) && wmetric == WTHEME_METRIC_PARTSIZE)
        {
            if (winfo.datatype == WTHEME_DATA_SCROLLBAR && winfo.data != nil)
            {
                MCWidgetScrollBarInfo *sb = (MCWidgetScrollBarInfo *)winfo.data;
                // Use the rect's aspect to determine orientation (matching scrolbar.cpp logic)
                bool t_horizontal = (srect.width > srect.height);
                
                CGFloat t_length = t_horizontal ? srect.width : srect.height;
                CGFloat t_thickness = t_horizontal ? srect.height : srect.width;
                
                double t_range = sb->endvalue - sb->startvalue;
                
                if (t_range > 0.0)
                {
                    // For sliders, use fixed thumb size; for scrollbars, use thumbsize from info
                    CGFloat t_norm;
                    CGFloat t_thumb_len;
                    if (t_is_slider)
                    {
                        // Slider: position is fraction of total range, fixed thumb size
                        t_norm = (CGFloat)((sb->thumbpos - sb->startvalue) / t_range);
                        t_thumb_len = 28.0f; // Fixed slider thumb size
                    }
                    else
                    {
                        // Scrollbar: subtract thumb size from scrollable range
                        if (sb->thumbsize <= 0.0)
                        {
                            // No content - hide thumb completely
                            drect.x = drect.y = drect.width = drect.height = 0;
                            return;
                        }
                        if (sb->thumbsize < t_range)
                        {
                            t_norm = (CGFloat)((sb->thumbpos - sb->startvalue) / (t_range - sb->thumbsize));
                            t_thumb_len = (CGFloat)(sb->thumbsize / t_range) * t_length;
                            if (t_thumb_len < 16.0f) t_thumb_len = 16.0f; // Minimum thumb size to match visual appearance
                        }
                        else
                        {
                            t_norm = 0.0f;
                            t_thumb_len = t_length;
                        }
                    }
                    
                    CGFloat t_thumb_x = t_norm * (t_length - t_thumb_len);
                    
                    if (t_horizontal) {
                        drect.x = srect.x + t_thumb_x;
                        drect.y = srect.y;
                        drect.width = t_thumb_len;
                        drect.height = t_thickness;
                    } else {
                        drect.x = srect.x;
                        drect.y = srect.y + t_thumb_x;
                        drect.width = t_thickness;
                        drect.height = t_thumb_len;
                    }
                    return;
                }
                else if (t_range > 0.0 && sb->thumbsize >= t_range)
                {
                    // No scrolling needed - full thumb
                    drect = srect;
                    return;
                }
            }
        }
        
        // Default: zero rect
        drect.x = drect.y = drect.width = drect.height = 0;
    }

    virtual Boolean drawwidget(MCDC *dc, const MCWidgetInfo &winfo, const MCRectangle &d);

    // Draw the keyboard-focus ring around a field/control using the system's
    // keyboardFocusIndicatorColor so the ring tracks the user's accent colour.
    virtual bool drawfocusborder(MCContext *p_context, const MCRectangle& p_dirty,
                                 const MCRectangle& p_rect) override
    {
        MCThemeDrawInfo t_info;
        memset(&t_info, 0, sizeof(t_info));
        t_info.dest = p_rect;
        p_context->drawtheme(THEME_DRAW_TYPE_FOCUS_RECT, &t_info);
        return true;
    }
};

// ── MCNativeTheme::drawwidget ───────────────────────────────────────────
Boolean MCNativeTheme::drawwidget(MCDC *dc, const MCWidgetInfo &winfo, const MCRectangle &d)
{
    MCThemeDrawInfo t_info = {};
    t_info.dest        = d;
    t_info.widget_type = winfo.type;
    t_info.part        = winfo.part;
    t_info.state       = winfo.state;
    t_info.attributes  = winfo.attributes;

    switch (winfo.type)
    {
        // ── Buttons ──────────────────────────────────────────────────
        case WTHEME_TYPE_PUSHBUTTON:
        case WTHEME_TYPE_BEVELBUTTON:
        case WTHEME_TYPE_CHECKBOX:
        case WTHEME_TYPE_RADIOBUTTON:
        case WTHEME_TYPE_OPTIONBUTTON:
        case WTHEME_TYPE_PULLDOWN:
        case WTHEME_TYPE_COMBOBUTTON:
        case WTHEME_TYPE_COMBO:
        case WTHEME_TYPE_RECTANGLE_BUTTON:
            dc->drawtheme(THEME_DRAW_TYPE_BUTTON, &t_info);
            break;

        // ── Tab buttons / Tab pane ────────────────────────────────────
        case WTHEME_TYPE_TAB:
            // Height is clamped to 22 px, mirroring drawthemetabs() in
            // osxtheme.mm (the Carbon version does the same).
            t_info.dest.height  = 22;
            t_info.tab.is_first = (winfo.attributes & WTHEME_ATT_FIRSTTAB) != 0;
            t_info.tab.is_last  = (winfo.attributes & WTHEME_ATT_LASTTAB)  != 0;
            dc->drawtheme(THEME_DRAW_TYPE_TAB, &t_info);
            break;

        case WTHEME_TYPE_TABPANE:
            dc->drawtheme(THEME_DRAW_TYPE_TAB_PANE, &t_info);
            break;

        // ── Scrollbar / Slider ────────────────────────────────────────
        case WTHEME_TYPE_SCROLLBAR:
        case WTHEME_TYPE_SLIDER:
        case WTHEME_TYPE_SLIDER_TRACK_HORIZONTAL:
        case WTHEME_TYPE_SLIDER_TRACK_VERTICAL:
        case WTHEME_TYPE_SLIDER_THUMB_HORIZONTAL:
        case WTHEME_TYPE_SLIDER_THUMB_VERTICAL:
        {
            if (winfo.datatype == WTHEME_DATA_SCROLLBAR && winfo.data != nil)
            {
                MCWidgetScrollBarInfo *sb = (MCWidgetScrollBarInfo *)winfo.data;
                t_info.scrollbar.startvalue = sb->startvalue;
                t_info.scrollbar.thumbpos   = sb->thumbpos;
                t_info.scrollbar.endvalue   = sb->endvalue;
                t_info.scrollbar.thumbsize  = sb->thumbsize;
            }
            t_info.scrollbar.horizontal = (winfo.attributes & WTHEME_ATT_SBVERTICAL) == 0;
            
            // Use slider draw type for slider widgets, scrollbar for scrollbar widgets
            MCThemeDrawType dt = (winfo.type == WTHEME_TYPE_SLIDER ||
                                 winfo.type == WTHEME_TYPE_SLIDER_TRACK_HORIZONTAL ||
                                 winfo.type == WTHEME_TYPE_SLIDER_TRACK_VERTICAL ||
                                 winfo.type == WTHEME_TYPE_SLIDER_THUMB_HORIZONTAL ||
                                 winfo.type == WTHEME_TYPE_SLIDER_THUMB_VERTICAL)
                                 ? THEME_DRAW_TYPE_SLIDER
                                 : THEME_DRAW_TYPE_SCROLLBAR;
            dc->drawtheme(dt, &t_info);
            break;
        }

        // ── Stepper (small scrollbar) ───────────────────────────────────
        case WTHEME_TYPE_SMALLSCROLLBAR:
        {
            t_info.scrollbar.horizontal = (winfo.attributes & WTHEME_ATT_SBVERTICAL) == 0;
            dc->drawtheme(THEME_DRAW_TYPE_SPIN, &t_info);
            break;
        }

        // ── Progress bar ──────────────────────────────────────────────
        case WTHEME_TYPE_PROGRESSBAR:
        case WTHEME_TYPE_PROGRESSBAR_HORIZONTAL:
        case WTHEME_TYPE_PROGRESSBAR_VERTICAL:
        {
            if (winfo.datatype == WTHEME_DATA_SCROLLBAR && winfo.data != nil)
            {
                MCWidgetScrollBarInfo *sb = (MCWidgetScrollBarInfo *)winfo.data;
                t_info.scrollbar.startvalue = sb->startvalue;
                t_info.scrollbar.thumbpos   = sb->thumbpos;
                t_info.scrollbar.endvalue   = sb->endvalue;
                t_info.scrollbar.thumbsize  = 0.0;
            }
            t_info.scrollbar.horizontal = (winfo.attributes & WTHEME_ATT_SBVERTICAL) == 0;
            dc->drawtheme(THEME_DRAW_TYPE_PROGRESS, &t_info);
            break;
        }

        // ── Text field / combo / listbox frame ────────────────────────
        case WTHEME_TYPE_TEXTFIELD_FRAME:
        case WTHEME_TYPE_COMBOTEXT:
        case WTHEME_TYPE_COMBOFRAME:
        case WTHEME_TYPE_LISTBOX_FRAME:
            dc->drawtheme(THEME_DRAW_TYPE_FRAME, &t_info);
            break;

        // ── Group box ─────────────────────────────────────────────────
        case WTHEME_TYPE_GROUP_FRAME:
        case WTHEME_TYPE_GROUP_FILL:
        case WTHEME_TYPE_SECONDARYGROUP_FRAME:
        case WTHEME_TYPE_SECONDARYGROUP_FILL:
            dc->drawtheme(THEME_DRAW_TYPE_GROUP, &t_info);
            break;

        // ── Scrollbar primitive sub-parts (drawn as part of the whole) ─
        case WTHEME_TYPE_SCROLLBAR_TRACK_VERTICAL:
        case WTHEME_TYPE_SCROLLBAR_TRACK_HORIZONTAL:
        case WTHEME_TYPE_SCROLLBAR_BUTTON_UP:
        case WTHEME_TYPE_SCROLLBAR_BUTTON_DOWN:
        case WTHEME_TYPE_SCROLLBAR_BUTTON_LEFT:
        case WTHEME_TYPE_SCROLLBAR_BUTTON_RIGHT:
        case WTHEME_TYPE_SCROLLBAR_THUMB_VERTICAL:
        case WTHEME_TYPE_SCROLLBAR_THUMB_HORIZONTAL:
        case WTHEME_TYPE_SCROLLBAR_GRIPPER_VERTICAL:
        case WTHEME_TYPE_SCROLLBAR_GRIPPER_HORIZONTAL:
        {
            // Scrollbar sub-parts delegate to the full scrollbar draw path.
            if (winfo.datatype == WTHEME_DATA_SCROLLBAR && winfo.data != nil)
            {
                MCWidgetScrollBarInfo *sb = (MCWidgetScrollBarInfo *)winfo.data;
                t_info.scrollbar.startvalue = sb->startvalue;
                t_info.scrollbar.thumbpos = sb->thumbpos;
                t_info.scrollbar.endvalue = sb->endvalue;
                t_info.scrollbar.thumbsize = sb->thumbsize;
            }
            t_info.scrollbar.horizontal = (winfo.attributes & WTHEME_ATT_SBVERTICAL) == 0;
            dc->drawtheme(THEME_DRAW_TYPE_SCROLLBAR, &t_info);
            break;
        }

        // ── Stepper (spin) arrow buttons ─────────────────────────────────
        case WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_UP:
        case WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_DOWN:
        case WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_LEFT:
        case WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_RIGHT:
        {
            t_info.scrollbar.horizontal = (winfo.attributes & WTHEME_ATT_SBVERTICAL) == 0;
            dc->drawtheme(THEME_DRAW_TYPE_SPIN, &t_info);
            break;
        }

        // ── Stepper (spin buttons) ────────────────────────────────────────
        case WTHEME_TYPE_SPIN:
        {
            NSLog(@"MCNativeTheme: drawing SPIN widget type");
            // For now, fall back to scrollbar drawing
            if (winfo.datatype == WTHEME_DATA_SCROLLBAR && winfo.data != nil)
            {
                MCWidgetScrollBarInfo *sb = (MCWidgetScrollBarInfo *)winfo.data;
                t_info.scrollbar.startvalue = sb->startvalue;
                t_info.scrollbar.thumbpos   = sb->thumbpos;
                t_info.scrollbar.endvalue   = sb->endvalue;
                t_info.scrollbar.thumbsize  = sb->thumbsize;
            }
            t_info.scrollbar.horizontal = (winfo.attributes & WTHEME_ATT_SBVERTICAL) == 0;
            dc->drawtheme(THEME_DRAW_TYPE_SCROLLBAR, &t_info);
            break;
        }

        // ── All other types: accept but draw nothing ──────────────────
        default:
            // Return True so we suppress the broken IsMacEmulatedLF() path.
            break;
    }

    return True;
}

MCTheme *MCThemeCreateNative(void) { return new (nothrow) MCNativeTheme; }

// ── MCOSXCreateCGContextForBitmap ──────────────────────────────────────
// Real implementation for ARM64 — creates a CGBitmapContext that writes
// directly into an MCImageBitmap's pixel buffer.
bool MCOSXCreateCGContextForBitmap(MCImageBitmap *p_bitmap, CGContextRef &r_context)
{
    CGColorSpaceRef t_colorspace = nil;
    if (!MCMacPlatformGetImageColorSpace(t_colorspace))
        return false;

    CGBitmapInfo t_bitmap_info =
        MCGPixelFormatToCGBitmapInfo(kMCGPixelFormatNative, /*alpha=*/true);

    CGContextRef t_ctx = CGBitmapContextCreate(
        p_bitmap->data,
        p_bitmap->width,
        p_bitmap->height,
        8,
        p_bitmap->stride,
        t_colorspace,
        t_bitmap_info);

    CGColorSpaceRelease(t_colorspace);

    if (t_ctx == nil)
        return false;

    r_context = t_ctx;
    return true;
}

// ── MCThemeDraw ─────────────────────────────────────────────────────────
// Renders a native Aqua widget into p_context using AppKit drawing.
//
// Flow:
//   1. Allocate an MCImageBitmap the size of dest.
//   2. Wrap it in a CGBitmapContext.
//   3. Flip the y-axis so the NSGraphicsContext has a top-left origin.
//   4. Create an NSGraphicsContext from the CGContext and push it current.
//   5. Draw the appropriate NSCell or NSView.
//   6. Pop the NSGraphicsContext, release the CGContext.
//   7. Blit the pixel data back into p_context via MCGContextDrawPixels.
bool MCThemeDraw(MCGContextRef p_context, MCThemeDrawType p_type, MCThemeDrawInfo *p_info)
{
    if (p_info == nil)
        return false;

    MCRectangle t_dest = p_info->dest;
    if (t_dest.width <= 0 || t_dest.height <= 0)
        return false;

    // ── HiDPI / Retina scale ─────────────────────────────────────────
    // Render the off-screen buffer at device pixels (up to 2×) so widgets
    // look crisp on Retina displays.  The x86 path (osxtheme.mm) has done
    // this since 2014; here we mirror the same approach.
    MCGAffineTransform t_mcg_transform = MCGContextGetDeviceTransform(p_context);
    MCGFloat t_ui_scale = 1.0;
    if (MCGAffineTransformIsRectangular(t_mcg_transform))
    {
        MCGFloat t_scale = MCGAffineTransformGetEffectiveScale(t_mcg_transform);
        if (t_scale > 1.0)
            t_ui_scale = 2.0;
    }

    // Buffer is t_dest.size * t_ui_scale device pixels.
    // The destination rect in MCGContext space is always the logical rect —
    // MCGContextDrawPixels handles the scale mapping when blitting back.
    uint32_t t_buf_width  = (uint32_t)(t_dest.width  * (int)t_ui_scale);
    uint32_t t_buf_height = (uint32_t)(t_dest.height * (int)t_ui_scale);
    MCGRectangle t_dst_mcg = MCRectangleToMCGRectangle(t_dest);

    // ── Off-screen pixel buffer ──────────────────────────────────────
    MCImageBitmap *t_bitmap = nil;
    if (!MCImageBitmapCreate(t_buf_width, t_buf_height, t_bitmap))
        return false;
    MCImageBitmapClear(t_bitmap);

    CGContextRef t_cgcontext = nil;
    if (!MCOSXCreateCGContextForBitmap(t_bitmap, t_cgcontext))
    {
        MCImageFreeBitmap(t_bitmap);
        return false;
    }

    // CGBitmapContext has (0,0) at bottom-left; flip so (0,0) is top-left,
    // matching what AppKit expects when we wrap it with flipped:YES.
    // NOTE: we do NOT add an origin shift here.  All widgets are drawn at
    // NSMakeRect(0, 0, width, height) — local to the buffer.  The x86 path
    // needs a shift because MCMacDrawTheme uses the widget's on-screen
    // coordinates directly; arm64 always draws at (0,0) so no shift is needed.
    CGContextTranslateCTM(t_cgcontext, 0.0, (CGFloat)t_buf_height);
    CGContextScaleCTM   (t_cgcontext, 1.0, -1.0);

    // Apply the HiDPI scale so AppKit renders at device-pixel resolution.
    if (t_ui_scale != 1.0)
        CGContextScaleCTM(t_cgcontext, t_ui_scale, t_ui_scale);

    // ── NSGraphicsContext wrapper ────────────────────────────────────
    NSGraphicsContext *t_ns_ctx =
        [NSGraphicsContext graphicsContextWithCGContext:t_cgcontext flipped:YES];

    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:t_ns_ctx];

    // All widgets draw into (0, 0, t_dest.width, t_dest.height) — i.e. local
    // to this off-screen buffer.  The MCGContextDrawPixels call below places
    // the result at the correct position in the destination MCGContext.
    NSRect  t_frame = NSMakeRect(0, 0,
                                 (CGFloat)t_dest.width, (CGFloat)t_dest.height);
    NSView *t_view  = GetDummyView();

    bool t_disabled = (p_info->state & WTHEME_STATE_DISABLED)         != 0;
    bool t_hilited  = (p_info->state & WTHEME_STATE_HILITED)          != 0;
    bool t_pressed  = (p_info->state & WTHEME_STATE_PRESSED)          != 0;
    bool t_default  = ((p_info->state & WTHEME_STATE_HASDEFAULT)      != 0) &&
                      ((p_info->state & WTHEME_STATE_SUPPRESSDEFAULT) == 0);

    // Draw inside the app's current appearance (light / dark mode).
    // NSButtonCell drawWithFrame:inView: uses the inView's *window* appearance
    // to choose rendering style — not only the current drawing appearance.
    // Sync the dummy window's appearance here so NSCell rendering uses the
    // modern flat style instead of falling back to the old Aqua gradient.
    NSAppearance *t_appearance = [NSApp effectiveAppearance];
    if (t_appearance == nil)
        t_appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    [t_view.window setAppearance:t_appearance];

    switch (p_type)
    {
        // ── Push / bevel / checkbox / radio / option / pulldown ──────
        case THEME_DRAW_TYPE_BUTTON:
        {
            switch (p_info->widget_type)
            {
                case WTHEME_TYPE_CHECKBOX:
                {
                    NSButtonCell *t_cell = [[NSButtonCell alloc] init];
                    [t_cell setButtonType:NSButtonTypeSwitch];
                    [t_cell setTitle:@""];
                    [t_cell setEnabled:!t_disabled];
                    [t_cell setHighlighted:t_pressed];
                    [t_cell setState:t_hilited ? NSControlStateValueOn
                                               : NSControlStateValueOff];
                    [t_appearance performAsCurrentDrawingAppearance:^{
                        [t_cell drawWithFrame:t_frame inView:t_view];
                    }];
                    [t_cell release];
                    break;
                }
                case WTHEME_TYPE_RADIOBUTTON:
                {
                    NSButtonCell *t_cell = [[NSButtonCell alloc] init];
                    [t_cell setButtonType:NSButtonTypeRadio];
                    [t_cell setTitle:@""];
                    [t_cell setEnabled:!t_disabled];
                    [t_cell setHighlighted:t_pressed];
                    [t_cell setState:t_hilited ? NSControlStateValueOn
                                               : NSControlStateValueOff];
                    [t_appearance performAsCurrentDrawingAppearance:^{
                        [t_cell drawWithFrame:t_frame inView:t_view];
                    }];
                    [t_cell release];
                    break;
                }
                case WTHEME_TYPE_OPTIONBUTTON:
                {
                    NSPopUpButtonCell *t_cell =
                        [[NSPopUpButtonCell alloc] initTextCell:@"" pullsDown:NO];
                    [t_cell addItemWithTitle:@""];
                    [t_cell setEnabled:!t_disabled];
                    NSRect t_r = t_frame;
                    if (t_r.size.height > 22.0)
                    {
                        t_r.origin.y    = floor((t_r.size.height - 22.0) / 2.0);
                        t_r.size.height = 22.0;
                    }
                    t_r.size.height -= 2.0;
                    [t_appearance performAsCurrentDrawingAppearance:^{
                        [t_cell drawWithFrame:t_r inView:t_view];
                    }];
                    [t_cell release];
                    break;
                }
                case WTHEME_TYPE_COMBO:
                {
                    // Draw combo box - just white background with separator and arrow
                    NSRect t_main_rect = NSInsetRect(t_frame, 4, 4);
                    
                    // White background
                    NSBezierPath *t_bg = [NSBezierPath bezierPathWithRoundedRect:t_main_rect xRadius:4.0 yRadius:4.0];
                    [[NSColor whiteColor] setFill];
                    [t_bg fill];
                    
                    // Separator between text and dropdown
                    CGFloat t_sep_x = t_main_rect.origin.x + t_main_rect.size.width - 24;
                    NSBezierPath *t_sep = [NSBezierPath bezierPath];
                    [t_sep moveToPoint:NSMakePoint(t_sep_x, t_main_rect.origin.y + 2)];
                    [t_sep lineToPoint:NSMakePoint(t_sep_x, t_main_rect.origin.y + t_main_rect.size.height - 2)];
                    [[NSColor separatorColor] setStroke];
                    [t_sep setLineWidth:1.0];
                    [t_sep stroke];
                    
                    // Dropdown arrow (chevron)
                    CGFloat t_ax = t_sep_x + 12;
                    CGFloat t_ay = t_main_rect.origin.y + t_main_rect.size.height / 2;
                    NSBezierPath *t_arrow = [NSBezierPath bezierPath];
                    [t_arrow setLineWidth:1.5];
                    [t_arrow setLineCapStyle:NSLineCapStyleRound];
                    [t_arrow setLineJoinStyle:NSLineJoinStyleRound];
                    [t_arrow moveToPoint:NSMakePoint(t_ax - 3.5, t_ay - 1.5)];
                    [t_arrow lineToPoint:NSMakePoint(t_ax, t_ay + 2.0)];
                    [t_arrow lineToPoint:NSMakePoint(t_ax + 3.5, t_ay - 1.5)];
                    [[NSColor controlTextColor] setStroke];
                    [t_arrow stroke];
                    
                    // Border
                    [[NSColor separatorColor] setStroke];
                    [t_bg setLineWidth:6.0];
                    [t_bg stroke];
                    break;
                }
                case WTHEME_TYPE_PULLDOWN:
                {
                    // Draw pulldown button with accent-colored chevron area
                    NSRect t_r = t_frame;
                    t_r.origin.y    += 1.0;
                    t_r.size.height -= 3.0;
                    CGFloat t_radius = t_r.size.height / 2.0;
                    
                    [t_appearance performAsCurrentDrawingAppearance:^{
                        // Standard button background
                        NSColor *t_fill;
                        if (t_disabled) {
                            t_fill = [[NSColor controlColor] colorWithAlphaComponent:0.5];
                        } else {
                            t_fill = [NSColor controlColor];
                        }
                        
                        NSBezierPath *t_path = [NSBezierPath bezierPathWithRoundedRect:t_r xRadius:t_radius yRadius:t_radius];
                        
                        // Drop shadow
                        [NSGraphicsContext saveGraphicsState];
                        NSShadow *t_shadow = [[NSShadow alloc] init];
                        [t_shadow setShadowOffset:NSMakeSize(0.0, -1.0)];
                        [t_shadow setShadowBlurRadius:1.5];
                        [t_shadow setShadowColor:[NSColor colorWithWhite:0.0 alpha:0.20]];
                        [t_shadow set];
                        [t_fill setFill];
                        [t_path fill];
                        [t_shadow release];
                        [NSGraphicsContext restoreGraphicsState];
                        
                        // Accent-colored rounded rectangle for chevron (right side, inset)
                        if (!t_disabled) {
                            NSColor *t_accent = [NSColor controlAccentColor];
                            if (t_accent == nil) {
                                t_accent = [NSColor systemBlueColor];
                            }
                            
                            // Inset rounded rectangle for accent area
                            NSRect t_accent_rect = NSInsetRect(t_r, 2.0, 2.0);
                            NSRect t_chevron_bg = NSMakeRect(NSMaxX(t_accent_rect) - 16, NSMinY(t_accent_rect), 16, NSHeight(t_accent_rect));
                            NSBezierPath *t_accent_path = [NSBezierPath bezierPathWithRoundedRect:t_chevron_bg xRadius:3.0 yRadius:3.0];
                            [t_accent setFill];
                            [t_accent_path fill];
                            
                            // White chevron (down arrow): 7px wide, 4px high, 6px from bottom, centered in accent area
                            CGFloat t_ax = NSMidX(t_chevron_bg);
                            CGFloat t_ay = NSMinY(t_chevron_bg) + NSHeight(t_chevron_bg) - 6;
                            NSBezierPath *t_arrow = [NSBezierPath bezierPath];
                            [t_arrow setLineWidth:1.5];
                            [t_arrow setLineCapStyle:NSLineCapStyleRound];
                            [t_arrow setLineJoinStyle:NSLineJoinStyleRound];
                            [t_arrow moveToPoint:NSMakePoint(t_ax - 3.5, t_ay - 2)];
                            [t_arrow lineToPoint:NSMakePoint(t_ax, t_ay + 2)];
                            [t_arrow lineToPoint:NSMakePoint(t_ax + 3.5, t_ay - 2)];
                            [[NSColor whiteColor] setStroke];
                            [t_arrow stroke];
                        }
                    }];
                    break;
                }
                case WTHEME_TYPE_RECTANGLE_BUTTON:
                {
                    // Rectangle button - no rounded corners
                    NSRect t_r = t_frame;
                    t_r.origin.y    += 1.0;
                    t_r.size.height -= 3.0;
                    
                    // Resolve accent color
                    NSColor *t_accent = nil;
                    NSColor *t_candidate = [NSColor controlAccentColor];
                    if (t_candidate != nil) {
                        t_accent = [t_candidate colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
                    }
                    if (t_accent == nil && [NSColor respondsToSelector:@selector(tintedControlColor)]) {
                        t_candidate = [NSColor performSelector:@selector(tintedControlColor)];
                        if (t_candidate != nil) {
                            t_accent = [t_candidate colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
                        }
                    }
                    if (t_accent == nil) {
                        t_accent = [[NSColor systemBlueColor] colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
                    }
                    if (t_accent == nil) {
                        t_accent = [NSColor systemBlueColor];
                    }
                    
                    [t_appearance performAsCurrentDrawingAppearance:^{
                        NSColor *t_fill;
                        if (t_disabled) {
                            t_fill = [[NSColor controlColor] colorWithAlphaComponent:0.5];
                        } else if (t_pressed || t_hilited) {
                            if (t_default) {
                                t_fill = [t_accent shadowWithLevel:0.15];
                            } else {
                                t_fill = [[NSColor controlColor] shadowWithLevel:0.12];
                            }
                        } else if (t_default) {
                            t_fill = t_accent;
                        } else {
                            t_fill = [NSColor controlColor];
                        }
                        
                        // Draw rectangle button without rounded corners
                        NSBezierPath *t_path = [NSBezierPath bezierPathWithRect:t_r];
                        
                        // Drop shadow
                        [NSGraphicsContext saveGraphicsState];
                        NSShadow *t_shadow = [[NSShadow alloc] init];
                        [t_shadow setShadowOffset:NSMakeSize(0.0, -1.0)];
                        [t_shadow setShadowBlurRadius:1.5];
                        [t_shadow setShadowColor:[NSColor colorWithWhite:0.0 alpha:0.20]];
                        [t_shadow set];
                        [t_fill setFill];
                        [t_path fill];
                        [t_shadow release];
                        [NSGraphicsContext restoreGraphicsState];
                        
                        // Draw border
                        NSColor *t_border = [NSColor colorWithCalibratedWhite:0.55 alpha:1.0];
                        [t_border setStroke];
                        [t_path setLineWidth:1.0];
                        [t_path stroke];
                    }];
                    break;
                }
                default:   // push button, bevel button, combo button
                {
                    // NSButtonCell with NSBezelStyleRounded uses Core Animation /
                    // Metal for its modern flat rendering; it silently falls back
                    // to the old Aqua gradient when given a plain CGBitmapContext.
                    // Draw the bezel directly with NSBezierPath + semantic colours
                    // instead — these work correctly in any graphics context and
                    // automatically adapt to light / dark mode.
                    NSRect t_r = t_frame;
                    t_r.origin.y    += 1.0;
                    t_r.size.height -= 3.0;   // 1pt top gap + 2pt for drop-shadow
                    CGFloat t_radius = t_r.size.height / 2.0;

                    [t_appearance performAsCurrentDrawingAppearance:^{
                        NSBezierPath *t_path =
                            [NSBezierPath bezierPathWithRoundedRect:t_r
                                                           xRadius:t_radius
                                                           yRadius:t_radius];

                        // Resolve the user's accent colour to a concrete sRGB
                        // value *inside* this appearance block.  Dynamic colours
                        // like controlAccentColor carry an app-active-state
                        // dimension: they desaturate to grey when the app loses
                        // focus.  colorUsingColorSpace: collapses the dynamic
                        // colour into a plain RGBA value in the current appearance
                        // context — stable in both foreground and background.
                        NSColor *t_accent = nil;
                        
                        // Try controlAccentColor first (macOS 10.14+)
                        NSColor *t_candidate = [NSColor controlAccentColor];
                        if (t_candidate != nil) {
                            t_accent = [t_candidate colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
                        }
                        
                        // Fallback to tintedControlColor (macOS 15+)
                        if (t_accent == nil && [NSColor respondsToSelector:@selector(tintedControlColor)]) {
                            t_candidate = [NSColor performSelector:@selector(tintedControlColor)];
                            if (t_candidate != nil) {
                                t_accent = [t_candidate colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
                            }
                        }
                        
                        // Fallback to systemBlueColor
                        if (t_accent == nil) {
                            t_accent = [[NSColor systemBlueColor] colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
                        }
                        
                        // Final fallback
                        if (t_accent == nil) {
                            t_accent = [NSColor systemBlueColor];
                        }

                        // Background colour for each state.
                        // Priority: disabled → pressed/hilited → default → normal.
                        // Pressed is checked BEFORE default so that clicking a
                        // default button produces a visible darkening.
                        NSColor *t_fill;
                        if (t_disabled) {
                            t_fill = [[NSColor controlColor]
                                          colorWithAlphaComponent:0.5];
                        } else if (t_pressed || t_hilited) {
                            // Use shadowWithLevel for proper macOS button press effect
                            if (t_default) {
                                t_fill = [t_accent shadowWithLevel:0.15];
                            } else {
                                t_fill = [[NSColor controlColor] shadowWithLevel:0.12];
                            }
                        } else if (t_default) {
                            t_fill = t_accent;
                        } else {
                            t_fill = [NSColor controlColor];
                        }

                        // First pass: fill + drop-shadow.
                        // NSShadow offset uses unflipped coordinates regardless
                        // of the context; (0,-1) places the shadow 1pt below.
                        [NSGraphicsContext saveGraphicsState];
                        NSShadow *t_shadow = [[NSShadow alloc] init];
                        [t_shadow setShadowOffset:NSMakeSize(0.0, -1.0)];
                        [t_shadow setShadowBlurRadius:1.5];
                        [t_shadow setShadowColor:
                            [NSColor colorWithWhite:0.0 alpha:0.20]];
                        [t_shadow set];
                        [t_fill setFill];
                        [t_path fill];
                        [t_shadow release];
                        [NSGraphicsContext restoreGraphicsState];

                        // Second pass: solid fill on top so the shadow blur
                        // doesn't bleed into the button face.
                        [t_fill setFill];
                        [t_path fill];

                        // Thin border (skipped when disabled, always drawn when pressed for visibility).
                        if (!t_disabled) {
                            if (t_pressed || t_hilited) {
                                [[NSColor controlColor] setStroke];
                            } else {
                                [[NSColor separatorColor] setStroke];
                            }
                            [t_path setLineWidth:0.5];
                            [t_path stroke];
                        }
                    }];
                    break;
                }
            }
            break;
        }

        // ── Scrollbar ────────────────────────────────────────────────────────
        // HXT: Use NSScrollerCell (via NSScroller) to obtain the native NSScroller appearance,
        // then apply a CGContext scale in the thickness axis so the rendering fills the actual
        // scrollbarWidth frame rather than always appearing at the native ~15 pt width.
        case THEME_DRAW_TYPE_SCROLLBAR:
        {
            double t_range = p_info->scrollbar.endvalue - p_info->scrollbar.startvalue;
            CGFloat t_pos = 0.0f, t_proportion = 1.0f;
            
            // HXT: If thumbsize >= range, there's no content to scroll - hide the knob completely
            // This happens when the field is empty (thumbsize equals field height = range)
            bool t_has_content = (p_info->scrollbar.thumbsize < t_range);
            if (!t_has_content)
            {
                t_pos = 0.0f;
                t_proportion = 0.0f;
            }
            else if (t_range > 0.0)
            {
                double t_scrollable = t_range - p_info->scrollbar.thumbsize;
                if (t_scrollable > 0.0)
                    t_pos = (CGFloat)((p_info->scrollbar.thumbpos - p_info->scrollbar.startvalue)
                                      / t_scrollable);
                t_proportion = (CGFloat)(p_info->scrollbar.thumbsize / t_range);
                if (t_pos < 0.0f)        t_pos = 0.0f;
                if (t_pos > 1.0f)        t_pos = 1.0f;
                if (t_proportion < 0.0f) t_proportion = 0.0f;
                if (t_proportion > 1.0f) t_proportion = 1.0f;
            }

            bool t_sb_vertical = ((p_info->attributes & WTHEME_ATT_SBVERTICAL) != 0);

            // Preferred native thickness for NSScrollerStyleLegacy (typically ~15 pt).
            // NSScrollerStyleLegacy is deprecated but still renders; suppress the warning.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            CGFloat t_native_w = [NSScroller scrollerWidthForControlSize:NSControlSizeRegular
                                                            scrollerStyle:NSScrollerStyleLegacy];
            if (t_native_w <= 0.0f)
                t_native_w = 15.0f;

            // Requested thickness (width for vertical, height for horizontal).
            CGFloat t_thickness = t_sb_vertical ? t_frame.size.width : t_frame.size.height;
            // Scale factor to stretch native-width NSScroller to fill the actual frame.
            CGFloat t_sb_scale  = (t_thickness > 0.0f) ? (t_thickness / t_native_w) : 1.0f;

            // Build a temporary NSScroller so we can configure knobProportion (not on NSCell)
            // and then draw track + knob into our CGContext via its own drawing methods.
            NSRect t_native_frame;
            if (t_sb_vertical)
                t_native_frame = NSMakeRect(0, 0, t_native_w, t_frame.size.height);
            else
                t_native_frame = NSMakeRect(0, 0, t_frame.size.width, t_native_w);

            NSScroller *t_scroller = [[NSScroller alloc] initWithFrame:t_native_frame];
            [t_scroller setScrollerStyle:NSScrollerStyleLegacy];
            [t_scroller setEnabled:!t_disabled];
            [t_scroller setFloatValue:(float)t_pos];
            [t_scroller setKnobProportion:t_proportion];
#pragma clang diagnostic pop

            [t_appearance performAsCurrentDrawingAppearance:^{
                CGContextSaveGState(t_cgcontext);

                // Pre-multiply a scale into the CTM so that drawing at t_native_w
                // in the thickness direction fills the actual scrollbarWidth.
                // The existing CTM already carries the HiDPI and flip transforms,
                // so we only need to add this one extra factor.
                if (t_sb_vertical)
                    CGContextScaleCTM(t_cgcontext, t_sb_scale, 1.0f);
                else
                    CGContextScaleCTM(t_cgcontext, 1.0f, t_sb_scale);

                // NSScrollerCell's drawWithFrame:inView: only draws the track slot.
                // The knob must be drawn separately.  Both methods draw into the
                // current NSGraphicsContext (already set to t_ns_ctx above).
                [t_scroller drawKnobSlotInRect:t_native_frame highlight:t_hilited];
                // Only draw knob if there's content to scroll
                if (t_proportion > 0.0f)
                    [t_scroller drawKnob];

                CGContextRestoreGState(t_cgcontext);
            }];
            [t_scroller release];
            break;
        }

        // ── Slider ────────────────────────────────────────────────────────
        // Draw native macOS slider using NSSlider
        case THEME_DRAW_TYPE_SLIDER:
        {
            double t_range = p_info->scrollbar.endvalue - p_info->scrollbar.startvalue;
            CGFloat t_value = 0.0f;
            if (t_range > 0.0)
            {
                t_value = (CGFloat)((p_info->scrollbar.thumbpos - p_info->scrollbar.startvalue) / t_range);
                if (t_value < 0.0f) t_value = 0.0f;
                if (t_value > 1.0f) t_value = 1.0f;
            }

            bool t_horizontal = ((p_info->attributes & WTHEME_ATT_SBVERTICAL) == 0);
            
            CGFloat t_track_thickness = 4.0f;
            CGFloat t_thumb_radius = 7.0f;
            
            // Use NSBezierPath for proper dynamic accent color support
            [t_appearance performAsCurrentDrawingAppearance:^{
                NSColor *t_accent = [NSColor controlAccentColor];
                NSColor *t_track_color = [NSColor colorWithCalibratedWhite:0.75 alpha:1.0];
                NSColor *t_thumb_color = [NSColor whiteColor];
                NSColor *t_thumb_border = [NSColor colorWithCalibratedWhite:0.5 alpha:1.0];
                
                if (t_horizontal)
                {
                    CGFloat t_track_y = t_frame.origin.y + (t_frame.size.height - t_track_thickness) / 2.0f;
                    CGFloat t_track_x = t_frame.origin.x + t_thumb_radius;
                    CGFloat t_track_w = t_frame.size.width - 2 * t_thumb_radius;
                    
                    // Draw track background
                    NSRect t_track_rect = NSMakeRect(t_track_x, t_track_y, t_track_w, t_track_thickness);
                    NSBezierPath *t_track_path = [NSBezierPath bezierPathWithRoundedRect:t_track_rect xRadius:2.0 yRadius:2.0];
                    [t_track_color setFill];
                    [t_track_path fill];
                    
                    // Draw filled portion (accent - dynamically changes with system appearance)
                    CGFloat t_fill_w = t_track_w * t_value;
                    if (t_fill_w > 0.0f)
                    {
                        NSRect t_fill_rect = NSMakeRect(t_track_x, t_track_y, t_fill_w, t_track_thickness);
                        NSBezierPath *t_fill_path = [NSBezierPath bezierPathWithRoundedRect:t_fill_rect xRadius:2.0 yRadius:2.0];
                        [t_accent setFill];
                        [t_fill_path fill];
                    }
                    
                    // Draw thumb
                    CGFloat t_thumb_x = t_track_x + t_track_w * t_value;
                    CGFloat t_thumb_y = t_frame.origin.y + t_frame.size.height / 2.0f;
                    NSRect t_thumb_rect = NSMakeRect(t_thumb_x - t_thumb_radius, t_thumb_y - t_thumb_radius, 
                                                     t_thumb_radius * 2, t_thumb_radius * 2);
                    NSBezierPath *t_thumb_path = [NSBezierPath bezierPathWithOvalInRect:t_thumb_rect];
                    [t_thumb_color setFill];
                    [t_thumb_path fill];
                    [t_thumb_border setStroke];
                    t_thumb_path.lineWidth = 1.0;
                    [t_thumb_path stroke];
                }
                else
                {
                    CGFloat t_track_x = t_frame.origin.x + (t_frame.size.width - t_track_thickness) / 2.0f;
                    CGFloat t_track_y = t_frame.origin.y + t_thumb_radius;
                    CGFloat t_track_h = t_frame.size.height - 2 * t_thumb_radius;
                    
                    // Draw track background
                    NSRect t_track_rect = NSMakeRect(t_track_x, t_track_y, t_track_thickness, t_track_h);
                    NSBezierPath *t_track_path = [NSBezierPath bezierPathWithRoundedRect:t_track_rect xRadius:2.0 yRadius:2.0];
                    [t_track_color setFill];
                    [t_track_path fill];
                    
                    // Draw filled portion (accent - dynamically changes with system appearance)
                    CGFloat t_fill_h = t_track_h * t_value;
                    if (t_fill_h > 0.0f)
                    {
                        NSRect t_fill_rect = NSMakeRect(t_track_x, t_track_y, t_track_thickness, t_fill_h);
                        NSBezierPath *t_fill_path = [NSBezierPath bezierPathWithRoundedRect:t_fill_rect xRadius:2.0 yRadius:2.0];
                        [t_accent setFill];
                        [t_fill_path fill];
                    }
                    
                    // Draw thumb
                    CGFloat t_thumb_x = t_frame.origin.x + t_frame.size.width / 2.0f;
                    CGFloat t_thumb_y = t_track_y + t_track_h * t_value;
                    NSRect t_thumb_rect = NSMakeRect(t_thumb_x - t_thumb_radius, t_thumb_y - t_thumb_radius,
                                                     t_thumb_radius * 2, t_thumb_radius * 2);
                    NSBezierPath *t_thumb_path = [NSBezierPath bezierPathWithOvalInRect:t_thumb_rect];
                    [t_thumb_color setFill];
                    [t_thumb_path fill];
                    [t_thumb_border setStroke];
                    t_thumb_path.lineWidth = 1.0;
                    [t_thumb_path stroke];
                }
            }];
            break;
        }

        // ── Progress bar ──────────────────────────────────────────────
        case THEME_DRAW_TYPE_PROGRESS:
        {
            [t_appearance performAsCurrentDrawingAppearance:^{
                CGFloat t_total = p_info->scrollbar.endvalue - p_info->scrollbar.startvalue;
                CGFloat t_current = p_info->scrollbar.thumbpos - p_info->scrollbar.startvalue;
                CGFloat t_fraction = (t_total > 0) ? (t_current / t_total) : 0;
                if (t_fraction < 0) t_fraction = 0;
                if (t_fraction > 1) t_fraction = 1;
                
                // Draw track (background)
                NSRect t_track = NSInsetRect(t_frame, 1, 1);
                NSBezierPath *t_track_path = [NSBezierPath bezierPathWithRoundedRect:t_track xRadius:3.0 yRadius:3.0];
                [[NSColor controlColor] setFill];
                [t_track_path fill];
                
                // Draw progress with accent color
                if (t_fraction > 0) {
                    CGFloat t_height = t_track.size.height;
                    CGFloat t_width = t_track.size.width * t_fraction;
                    NSRect t_fill_rect;
                    if (p_info->scrollbar.horizontal) {
                        t_fill_rect = NSMakeRect(t_track.origin.x, t_track.origin.y, t_width, t_height);
                    } else {
                        t_fill_rect = NSMakeRect(t_track.origin.x, t_track.origin.y + t_height * (1 - t_fraction), t_width, t_height * t_fraction);
                    }
                    
                    NSColor *t_accent = [NSColor controlAccentColor];
                    NSBezierPath *t_fill_path = [NSBezierPath bezierPathWithRoundedRect:t_fill_rect xRadius:2.5 yRadius:2.5];
                    [t_accent setFill];
                    [t_fill_path fill];
                }
            }];
            break;
        }

        // ── Tab button ────────────────────────────────────────────────
        // Both selected and unselected tabs are drawn with NSBezierPath:
        // NSButtonCell (any bezel style) goes transparent in a CGBitmapContext
        // because modern rendering requires a Metal/CA backing store.
        case THEME_DRAW_TYPE_TAB:
        {
            // Tab buttons are always 22 px high per the Aqua HIG.
            NSRect t_r = t_frame;
            t_r.size.height = 22.0;

            [t_appearance performAsCurrentDrawingAppearance:^{
                NSBezierPath *t_path =
                    [NSBezierPath bezierPathWithRoundedRect:t_r
                                                   xRadius:4.0
                                                   yRadius:4.0];

                if (t_hilited)
                {
                    if (MCappisactive)
                    {
                        // Active window: fill with the user's accent colour.
                        NSColor *t_accent =
                            [[NSColor controlAccentColor]
                                colorUsingColorSpace:[NSColorSpace sRGBColorSpace]]
                            ?: [NSColor systemBlueColor];

                        [t_accent setFill];
                        [t_path fill];

                        // Subtle lighter accent highlight stripe at the top.
                        NSRect t_hi = NSMakeRect(t_r.origin.x + 1.0,
                                                 t_r.origin.y + 1.0,
                                                 t_r.size.width  - 2.0,
                                                 3.0);
                        NSColor *t_hi_color = [t_accent colorWithAlphaComponent:0.3];
                        [t_hi_color setFill];
                        NSRectFillUsingOperation(t_hi,
                                                 NSCompositingOperationSourceOver);
                    }
                    else
                    {
                        // Inactive window: grey fill, matching macOS's standard
                        // appearance for selected tabs in background windows.
                        // The text colour is handled separately in buttondraw.cpp
                        // (also keyed on MCappisactive → getgray()).
                        // In light mode the fill (0.75) is lighter than the text
                        // (~0.5 grey), so the tab reads as a muted light pill.
                        // In dark mode we swap: use a dark fill (~0.32) so the
                        // text (~0.5 grey) appears relatively lighter — the correct
                        // visual relationship for a dark-mode inactive tab.
                        NSAppearanceName t_best_i =
                            [t_appearance bestMatchFromAppearancesWithNames:
                                @[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]];
                        bool t_inactive_dark =
                            [t_best_i isEqualToString:NSAppearanceNameDarkAqua];
                        NSColor *t_inactive =
                            t_inactive_dark
                            ? [NSColor colorWithCalibratedWhite:0.20f alpha:1.0f]
                            : [NSColor colorWithCalibratedWhite:0.75f alpha:1.0f];
                        [t_inactive setFill];
                        [t_path fill];
                    }
                }
                else
                {
                    // Unselected tab: use an opaque fill colour that matches the
                    // system appearance.  [NSColor controlColor] is intentionally
                    // avoided here because in dark mode it resolves to
                    // rgba(255,255,255,0.25) — a semi-transparent vibrancy tint
                    // designed for Metal/CA-backed views.  In a plain
                    // CGBitmapContext the alpha composites over whatever is already
                    // in the buffer, producing a visible top/bottom split where the
                    // top half (above the already-drawn tab pane) looks light and
                    // the bottom half looks dark.  Using an explicit opaque colour
                    // prevents this.
                    NSAppearanceName t_best =
                        [t_appearance bestMatchFromAppearancesWithNames:
                            @[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]];
                    bool t_is_dark =
                        [t_best isEqualToString:NSAppearanceNameDarkAqua];

                    // Light: ~NSColor.controlColor resolved (~0.93 white, opaque)
                    // Dark:  ~NSColor.controlBackgroundColor in dark (~0.22 white),
                    //        fully opaque — matches the standard unselected-tab look.
                    NSColor *t_base = t_is_dark
                        ? [NSColor colorWithCalibratedWhite:0.22f alpha:1.0f]
                        : [NSColor colorWithCalibratedWhite:0.93f alpha:1.0f];
                    NSColor *t_fill = t_disabled
                        ? [t_base colorWithAlphaComponent:0.5f]
                        : t_base;

                    [t_fill setFill];
                    [t_path fill];

                    [[NSColor separatorColor] setStroke];
                    [t_path setLineWidth:0.5];
                    [t_path stroke];
                }
            }];
            break;
        }

        // ── Tab pane background ───────────────────────────────────────
        // Draw a plain window-background fill with a 1-px border — close
        // enough to the HIThemeDrawTabPane appearance for layout purposes.
        case THEME_DRAW_TYPE_TAB_PANE:
        {
            [[NSColor windowBackgroundColor] setFill];
            NSRectFill(t_frame);
            NSRect t_border = NSInsetRect(t_frame, 0.5, 0.5);
            [[NSColor separatorColor] setStroke];
            [[NSBezierPath bezierPathWithRect:t_border] stroke];
            break;
        }

        // ── Text-field / combo / listbox frame ────────────────────────
        // Draw background with 2px gray border. Background adapts to light/dark mode.
        case THEME_DRAW_TYPE_FRAME:
        {
            [t_appearance performAsCurrentDrawingAppearance:^{
                // Determine if we're in dark mode
                NSAppearanceName t_best =
                    [[NSApp effectiveAppearance]
                        bestMatchFromAppearancesWithNames:
                            @[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]];
                bool t_is_dark = [t_best isEqualToString:NSAppearanceNameDarkAqua];
                
                // Background: white in light mode, dark gray in dark mode
                if (t_is_dark) {
                    [[NSColor colorWithCalibratedWhite:0.15 alpha:1.0] setFill];
                } else {
                    [[NSColor whiteColor] setFill];
                }
                NSRectFill(t_frame);
                
                // Border: gray that adapts slightly
                if (t_is_dark) {
                    [[NSColor colorWithCalibratedWhite:0.35 alpha:1.0] setStroke];
                } else {
                    [[NSColor colorWithCalibratedWhite:0.7 alpha:1.0] setStroke];
                }
                NSBezierPath *t_border = [NSBezierPath bezierPathWithRoundedRect:t_frame xRadius:3.0 yRadius:3.0];
                [t_border setLineWidth:2.0];
                [t_border stroke];
            }];
            break;
        }

        // ── Group box ─────────────────────────────────────────────────
        // NSBox gives us the native group-box appearance.
        case THEME_DRAW_TYPE_GROUP:
        {
            NSBox *t_box = [[NSBox alloc] initWithFrame:t_frame];
            [t_box setBoxType:NSBoxPrimary];
            [t_box setTitlePosition:NSNoTitle];
            [t_box setWantsLayer:NO];

            [t_view addSubview:t_box positioned:NSWindowBelow relativeTo:nil];

            [t_appearance performAsCurrentDrawingAppearance:^{
                [t_box drawRect:t_frame];
            }];

            [t_box removeFromSuperview];
            [t_box release];
            break;
        }

        // ── Focus ring ──────────────────────────────────────────────────────
        case THEME_DRAW_TYPE_FOCUS_RECT:
        {
            [t_appearance performAsCurrentDrawingAppearance:^{
                NSRect t_inner = NSInsetRect(t_frame, 1.5, 1.5);
                NSColor *t_accent = [NSColor keyboardFocusIndicatorColor];
                NSBezierPath *t_path = [NSBezierPath bezierPathWithRoundedRect:t_inner xRadius:2.5 yRadius:2.5];
                [t_accent setStroke];
                [t_path setLineWidth:2.0];
                [t_path stroke];
            }];
            break;
        }

        // ── Stepper (spin buttons) ─────────────────────────────────────────
        case THEME_DRAW_TYPE_SPIN:
        {
            [t_appearance performAsCurrentDrawingAppearance:^{
                bool t_vertical = ((p_info->attributes & WTHEME_ATT_SBVERTICAL) != 0);
                bool t_is_whole_stepper = (p_info->widget_type == WTHEME_TYPE_SMALLSCROLLBAR);
                
                // Check which part is being pressed
                bool t_is_up_pressed = (p_info->part == WTHEME_PART_ARROW_DEC);
                bool t_is_down_pressed = (p_info->part == WTHEME_PART_ARROW_INC);
                
                // Get accent color for pressed state
                NSColor *t_accent = [NSColor controlAccentColor];
                NSColor *t_arrow_color = t_disabled ? [[NSColor disabledControlTextColor] colorWithAlphaComponent:0.5] : [NSColor controlTextColor];
                
                if (t_is_whole_stepper) {
                    CGFloat t_height = t_frame.size.height;
                    CGFloat t_width = t_frame.size.width;
                    CGFloat t_mid_y = NSMinY(t_frame) + t_height / 2.0;
                    CGFloat t_mid_x = NSMinX(t_frame) + t_width / 2.0;
                    CGFloat t_button_height = t_height / 2.0;
                    
                    NSRect t_top_button, t_bottom_button;
                    if (t_vertical) {
                        t_top_button = NSMakeRect(t_frame.origin.x, t_mid_y, t_width, t_button_height);
                        t_bottom_button = NSMakeRect(t_frame.origin.x, t_frame.origin.y, t_width, t_button_height);
                    } else {
                        t_top_button = NSMakeRect(t_frame.origin.x, t_frame.origin.y, t_button_height, t_height);
                        t_bottom_button = NSMakeRect(t_mid_x, t_frame.origin.y, t_button_height, t_height);
                    }
                    
                    for (int i = 0; i < 2; i++) {
                        NSRect t_btn_rect = (i == 0) ? t_top_button : t_bottom_button;
                        bool t_btn_up = (i == 0);
                        bool t_btn_pressed = t_btn_up ? t_is_up_pressed : t_is_down_pressed;
                        
                        // Fill color based on state
                        NSColor *t_fill;
                        if (t_disabled) {
                            t_fill = [[NSColor controlColor] colorWithAlphaComponent:0.5];
                        } else if (t_btn_pressed) {
                            t_fill = t_accent;
                        } else {
                            t_fill = [NSColor controlColor];
                        }
                        
                        NSBezierPath *t_btn_path = [NSBezierPath bezierPathWithRoundedRect:t_btn_rect xRadius:3.0 yRadius:3.0];
                        [t_fill setFill];
                        [t_btn_path fill];
                        
                        if (!t_disabled && !t_btn_pressed) {
                            [[NSColor separatorColor] setStroke];
                            [t_btn_path setLineWidth:0.5];
                            [t_btn_path stroke];
                        }
                        
                        CGFloat t_arrow_size = MIN(t_btn_rect.size.width, t_btn_rect.size.height) - 4.0;
                        CGFloat t_x = NSMidX(t_btn_rect);
                        CGFloat t_y = NSMidY(t_btn_rect);
                        
                        [t_arrow_color setFill];
                        
                        if (t_vertical) {
                            NSPoint t_tip, t_base_left, t_base_right;
                            CGFloat t_half = t_arrow_size * 0.35;
                            if (t_btn_up) {
                                t_tip = NSMakePoint(t_x, NSMaxY(t_btn_rect) - t_half - 2);
                                t_base_left = NSMakePoint(t_x - t_half, NSMinY(t_btn_rect) + t_half + 2);
                                t_base_right = NSMakePoint(t_x + t_half, NSMinY(t_btn_rect) + t_half + 2);
                            } else {
                                t_tip = NSMakePoint(t_x, NSMinY(t_btn_rect) + t_half + 2);
                                t_base_left = NSMakePoint(t_x - t_half, NSMaxY(t_btn_rect) - t_half - 2);
                                t_base_right = NSMakePoint(t_x + t_half, NSMaxY(t_btn_rect) - t_half - 2);
                            }
                            NSPoint t_points[3] = {t_tip, t_base_left, t_base_right};
                            NSBezierPath *t_arrow_path = [NSBezierPath bezierPath];
                            [t_arrow_path moveToPoint:t_points[0]];
                            [t_arrow_path lineToPoint:t_points[1]];
                            [t_arrow_path lineToPoint:t_points[2]];
                            [t_arrow_path closePath];
                            [t_arrow_path fill];
                        } else {
                            NSPoint t_tip, t_base_top, t_base_bottom;
                            CGFloat t_half = t_arrow_size * 0.35;
                            if (t_btn_up) {
                                t_tip = NSMakePoint(NSMaxX(t_btn_rect) - t_half - 2, t_y);
                                t_base_top = NSMakePoint(NSMinX(t_btn_rect) + t_half + 2, t_y + t_half);
                                t_base_bottom = NSMakePoint(NSMinX(t_btn_rect) + t_half + 2, t_y - t_half);
                            } else {
                                t_tip = NSMakePoint(NSMinX(t_btn_rect) + t_half + 2, t_y);
                                t_base_top = NSMakePoint(NSMaxX(t_btn_rect) - t_half - 2, t_y + t_half);
                                t_base_bottom = NSMakePoint(NSMaxX(t_btn_rect) - t_half - 2, t_y - t_half);
                            }
                            NSPoint t_points[3] = {t_tip, t_base_top, t_base_bottom};
                            NSBezierPath *t_arrow_path = [NSBezierPath bezierPath];
                            [t_arrow_path moveToPoint:t_points[0]];
                            [t_arrow_path lineToPoint:t_points[1]];
                            [t_arrow_path lineToPoint:t_points[2]];
                            [t_arrow_path closePath];
                            [t_arrow_path fill];
                        }
                    }
                } else {
                    // Individual button drawing
                    bool t_is_this_up = (p_info->widget_type == WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_UP ||
                                        p_info->widget_type == WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_LEFT);
                    bool t_this_pressed = t_is_this_up ? t_is_up_pressed : t_is_down_pressed;
                    
                    NSColor *t_fill;
                    if (t_disabled) {
                        t_fill = [[NSColor controlColor] colorWithAlphaComponent:0.5];
                    } else if (t_this_pressed) {
                        t_fill = t_accent;
                    } else {
                        t_fill = [NSColor controlColor];
                    }
                    
                    NSRect t_arrow_rect = t_frame;
                    CGFloat t_arrow_size = MIN(t_arrow_rect.size.width, t_arrow_rect.size.height) - 4.0;
                    CGFloat t_x = NSMidX(t_arrow_rect);
                    CGFloat t_y = NSMidY(t_arrow_rect);
                    
                    NSBezierPath *t_path = [NSBezierPath bezierPathWithRoundedRect:t_arrow_rect xRadius:3.0 yRadius:3.0];
                    [t_fill setFill];
                    [t_path fill];
                    
                    if (!t_disabled && !t_this_pressed) {
                        [[NSColor separatorColor] setStroke];
                        [t_path setLineWidth:0.5];
                        [t_path stroke];
                    }
                    
                    [t_arrow_color setFill];
                    
                    if (t_vertical) {
                        NSPoint t_tip, t_base_left, t_base_right;
                        CGFloat t_half = t_arrow_size * 0.35;
                        if (t_is_this_up) {
                            t_tip = NSMakePoint(t_x, NSMaxY(t_arrow_rect) - t_half - 2);
                            t_base_left = NSMakePoint(t_x - t_half, NSMinY(t_arrow_rect) + t_half + 2);
                            t_base_right = NSMakePoint(t_x + t_half, NSMinY(t_arrow_rect) + t_half + 2);
                        } else {
                            t_tip = NSMakePoint(t_x, NSMinY(t_arrow_rect) + t_half + 2);
                            t_base_left = NSMakePoint(t_x - t_half, NSMaxY(t_arrow_rect) - t_half - 2);
                            t_base_right = NSMakePoint(t_x + t_half, NSMaxY(t_arrow_rect) - t_half - 2);
                        }
                        NSPoint t_points[3] = {t_tip, t_base_left, t_base_right};
                        NSBezierPath *t_arrow_path = [NSBezierPath bezierPath];
                        [t_arrow_path moveToPoint:t_points[0]];
                        [t_arrow_path lineToPoint:t_points[1]];
                        [t_arrow_path lineToPoint:t_points[2]];
                        [t_arrow_path closePath];
                        [t_arrow_path fill];
                    } else {
                        NSPoint t_tip, t_base_top, t_base_bottom;
                        CGFloat t_half = t_arrow_size * 0.35;
                        if (t_is_this_up) {
                            t_tip = NSMakePoint(NSMaxX(t_arrow_rect) - t_half - 2, t_y);
                            t_base_top = NSMakePoint(NSMinX(t_arrow_rect) + t_half + 2, t_y + t_half);
                            t_base_bottom = NSMakePoint(NSMinX(t_arrow_rect) + t_half + 2, t_y - t_half);
                        } else {
                            t_tip = NSMakePoint(NSMinX(t_arrow_rect) + t_half + 2, t_y);
                            t_base_top = NSMakePoint(NSMaxX(t_arrow_rect) - t_half - 2, t_y + t_half);
                            t_base_bottom = NSMakePoint(NSMaxX(t_arrow_rect) - t_half - 2, t_y - t_half);
                        }
                        NSPoint t_points[3] = {t_tip, t_base_top, t_base_bottom};
                        NSBezierPath *t_arrow_path = [NSBezierPath bezierPath];
                        [t_arrow_path moveToPoint:t_points[0]];
                        [t_arrow_path lineToPoint:t_points[1]];
                        [t_arrow_path lineToPoint:t_points[2]];
                        [t_arrow_path closePath];
                        [t_arrow_path fill];
                    }
                }
            }];
            break;
        }

        // All other draw types are not yet handled; the caller's fallback
        // or no-op will deal with them.
        default:
            break;
    }

    [NSGraphicsContext restoreGraphicsState];
    CGContextRelease(t_cgcontext);

    // ── Blit rendered pixels → MCGContext ────────────────────────────
    // t_dst_mcg is the widget's logical rect in MCGContext space.
    // The bitmap may be 2× wider/taller than that rect on Retina displays,
    // but MCGContextDrawPixels with kMCGImageFilterMedium handles the
    // mapping and renders at full device-pixel quality.
    MCGRaster t_raster;
    t_raster.width  = t_bitmap->width;
    t_raster.height = t_bitmap->height;
    t_raster.pixels = t_bitmap->data;
    t_raster.stride = t_bitmap->stride;
    t_raster.format = kMCGRasterFormat_ARGB;

    MCGContextDrawPixels(p_context, t_raster, t_dst_mcg, kMCGImageFilterMedium);

    MCImageFreeBitmap(t_bitmap);
    return true;
}

bool MCMacThemeGetBackgroundPattern(Window_mode p_mode, bool p_has_shadow, MCPatternRef &r_pattern) { return false; }

// ── Stack window ops ────────────────────────────────────────────────────

void MCStack::setgeom()
{
    if (!opened)
        return;

    if (window == NULL)
    {
        MCRedrawLockScreen();
        state &= ~CS_NEED_RESIZE;
        resize(rect.width, rect.height);
        MCRedrawUnlockScreen();
        mode_setgeom();
        return;
    }

    MCRectangle t_old_rect;
    t_old_rect = view_getstackviewport();

    rect = view_setstackviewport(rect);

    state &= ~CS_NEED_RESIZE;

    if (t_old_rect.x != rect.x || t_old_rect.y != rect.y ||
        t_old_rect.width != rect.width || t_old_rect.height != rect.height)
        resize(t_old_rect.width, t_old_rect.height);
}

void MCStack::sethints() {}
void MCStack::setsizehints() {}
void MCStack::enablewindow(bool p_enable) {}
void MCStack::redrawicon() {}
void MCStack::applyscroll() {}
void MCStack::clearscroll() {}
void MCStack::platform_openwindow(Boolean p_override)
{
    if (MCModeMakeLocalWindows() && window != NULL)
        MCscreen->openwindow(window, p_override);
}
void MCStack::release_window_buffer() {}

// ── HyperCard ───────────────────────────────────────────────────────────
// ARM64: Carbon Resource Manager APIs are not available. HC stacks imported on
// Apple Silicon simply have no icons/sounds/cursors from the resource fork —
// the data-fork content (cards, fields, buttons, bitmaps) still loads fine.
IO_stat MCHcstak::macreadresources(void) { return IO_NORMAL; }

#endif // __arm64__
