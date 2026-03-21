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
#define _MAC_DESKTOP
#include "hc.h"
#include "exec.h"

// ── ARM64 MCThemeDrawInfo ───────────────────────────────────────────────
// Replaces the Carbon/HITheme-based version in osxtheme.h (which is
// excluded on ARM64).  Only drawwidget() and MCThemeDraw() in this file
// ever populate or read this struct, so the layout only has to be
// self-consistent here.
struct MCThemeDrawInfo
{
    MCRectangle dest;          // target widget bounds
    Widget_Type widget_type;   // which widget to draw
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
        s_dummy_window = [[NSWindow alloc]
                initWithContentRect:NSMakeRect(-16000, -16000, 4096, 4096)
                          styleMask:NSWindowStyleMaskBorderless
                            backing:NSBackingStoreBuffered
                              defer:YES];
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

    virtual Boolean drawwidget(MCDC *dc, const MCWidgetInfo &winfo, const MCRectangle &d);
};

// ── MCNativeTheme::drawwidget ───────────────────────────────────────────
Boolean MCNativeTheme::drawwidget(MCDC *dc, const MCWidgetInfo &winfo, const MCRectangle &d)
{
    MCThemeDrawInfo t_info = {};
    t_info.dest        = d;
    t_info.widget_type = winfo.type;
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
        case WTHEME_TYPE_SMALLSCROLLBAR:
        case WTHEME_TYPE_SLIDER:
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
            MCThemeDrawType dt = (winfo.type == WTHEME_TYPE_SLIDER)
                                 ? THEME_DRAW_TYPE_SLIDER
                                 : THEME_DRAW_TYPE_SCROLLBAR;
            dc->drawtheme(dt, &t_info);
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
        case WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_UP:
        case WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_DOWN:
        case WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_LEFT:
        case WTHEME_TYPE_SMALLSCROLLBAR_BUTTON_RIGHT:
            // Sub-part primitives: no individual rendering needed.
            // Return True so the software fallback (which needs IsMacEmulatedLF)
            // is NOT invoked — the parent scrollbar draw handles everything.
            break;

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

    // ── Off-screen pixel buffer ──────────────────────────────────────
    MCImageBitmap *t_bitmap = nil;
    if (!MCImageBitmapCreate(t_dest.width, t_dest.height, t_bitmap))
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
    CGContextTranslateCTM(t_cgcontext, 0.0, (CGFloat)t_dest.height);
    CGContextScaleCTM   (t_cgcontext, 1.0, -1.0);

    // ── NSGraphicsContext wrapper ────────────────────────────────────
    NSGraphicsContext *t_ns_ctx =
        [NSGraphicsContext graphicsContextWithCGContext:t_cgcontext flipped:YES];

    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:t_ns_ctx];

    NSRect  t_frame = NSMakeRect(0, 0, (CGFloat)t_dest.width, (CGFloat)t_dest.height);
    NSView *t_view  = GetDummyView();

    bool t_disabled = (p_info->state & WTHEME_STATE_DISABLED)         != 0;
    bool t_hilited  = (p_info->state & WTHEME_STATE_HILITED)          != 0;
    bool t_pressed  = (p_info->state & WTHEME_STATE_PRESSED)          != 0;
    bool t_default  = ((p_info->state & WTHEME_STATE_HASDEFAULT)      != 0) &&
                      ((p_info->state & WTHEME_STATE_SUPPRESSDEFAULT) == 0);

    // Draw inside the app's current appearance (light / dark mode).
    NSAppearance *t_appearance = [NSApp effectiveAppearance];

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
                default:   // push button, bevel button, pulldown, combo button
                {
                    NSButtonCell *t_cell = [[NSButtonCell alloc] init];
                    [t_cell setButtonType:NSButtonTypeMomentaryPushIn];
                    [t_cell setBezelStyle:NSBezelStyleRounded];
                    [t_cell setTitle:@""];
                    [t_cell setEnabled:!t_disabled];
                    [t_cell setHighlighted:t_pressed || t_hilited];
                    if (t_default)
                        [t_cell setKeyEquivalent:@"\r"];
                    NSRect t_r = t_frame;
                    t_r.size.height -= 2.0;
                    [t_appearance performAsCurrentDrawingAppearance:^{
                        [t_cell drawWithFrame:t_r inView:t_view];
                    }];
                    [t_cell release];
                    break;
                }
            }
            break;
        }

        // ── Scrollbar / Slider ────────────────────────────────────────
        // Uses NSScroller with the legacy (always-visible) style so that
        // arrows and track are always drawn — regardless of the system's
        // default overlay-scroller preference.
        case THEME_DRAW_TYPE_SCROLLBAR:
        case THEME_DRAW_TYPE_SLIDER:
        {
            double t_range = p_info->scrollbar.endvalue - p_info->scrollbar.startvalue;
            // Normalised knob position within [0..1]: fraction of the way
            // through the scrollable range at which the knob START sits.
            CGFloat t_pos = 0.0f, t_proportion = 1.0f;
            if (t_range > 0.0)
            {
                double t_scrollable = t_range - p_info->scrollbar.thumbsize;
                if (t_scrollable > 0.0)
                    t_pos = (CGFloat)((p_info->scrollbar.thumbpos - p_info->scrollbar.startvalue)
                                      / t_scrollable);
                t_proportion = (CGFloat)(p_info->scrollbar.thumbsize / t_range);
                // Clamp to valid range.
                if (t_pos < 0.0f)        t_pos = 0.0f;
                if (t_pos > 1.0f)        t_pos = 1.0f;
                if (t_proportion < 0.0f) t_proportion = 0.0f;
                if (t_proportion > 1.0f) t_proportion = 1.0f;
            }

            NSScroller *t_scroller = [[NSScroller alloc] initWithFrame:t_frame];
            [t_scroller setScrollerStyle:NSScrollerStyleLegacy];
            [t_scroller setDoubleValue:(double)t_pos];
            [t_scroller setKnobProportion:t_proportion];
            [t_scroller setEnabled:!t_disabled];
            [t_scroller setWantsLayer:NO];

            // Add to the dummy view hierarchy so the scroller has a proper
            // window environment (appearance, colorspace, scale factor).
            [t_view addSubview:t_scroller positioned:NSWindowBelow relativeTo:nil];

            [t_appearance performAsCurrentDrawingAppearance:^{
                [t_scroller drawRect:t_frame];
            }];

            [t_scroller removeFromSuperview];
            [t_scroller release];
            break;
        }

        // ── Progress bar ──────────────────────────────────────────────
        case THEME_DRAW_TYPE_PROGRESS:
        {
            NSProgressIndicator *t_ind =
                [[NSProgressIndicator alloc] initWithFrame:t_frame];
            [t_ind setStyle:NSProgressIndicatorStyleBar];
            [t_ind setIndeterminate:NO];
            [t_ind setMinValue:p_info->scrollbar.startvalue];
            [t_ind setMaxValue:p_info->scrollbar.endvalue];
            [t_ind setDoubleValue:p_info->scrollbar.thumbpos];
            // NSProgressIndicator is NSView, not NSControl — no setEnabled:.
            // A disabled progress bar is visually handled by the appearance.
            [t_ind setWantsLayer:NO];

            [t_view addSubview:t_ind positioned:NSWindowBelow relativeTo:nil];

            [t_appearance performAsCurrentDrawingAppearance:^{
                [t_ind drawRect:t_frame];
            }];

            [t_ind removeFromSuperview];
            [t_ind release];
            break;
        }

        // ── Tab button ────────────────────────────────────────────────
        // Selected (hilited) tab: filled solid with the system accent/hilite
        // colour so the text rendered on top by buttondraw.cpp (white, via
        // DI_PSEUDO_BUTTON_TEXT_SEL) is legible.
        // Unselected tab: plain textured-square button in the off state.
        case THEME_DRAW_TYPE_TAB:
        {
            // Tab buttons are always 22 px high per the Aqua HIG.
            NSRect t_r = t_frame;
            t_r.size.height = 22.0;

            if (t_hilited)
            {
                // Use the system accent colour (= controlAccentColor on 10.14+,
                // selectedControlColor on older releases) as the "hilite colour".
                NSColor *t_accent;
                if ([NSColor respondsToSelector:@selector(controlAccentColor)])
                    t_accent = [NSColor performSelector:@selector(controlAccentColor)];
                else
                    t_accent = [NSColor selectedControlColor];

                [t_appearance performAsCurrentDrawingAppearance:^{
                    // Fill the tab area with the accent colour.
                    [t_accent setFill];
                    NSBezierPath *t_path =
                        [NSBezierPath bezierPathWithRoundedRect:t_r
                                                       xRadius:4.0
                                                       yRadius:4.0];
                    [t_path fill];

                    // Subtle top highlight (semi-transparent white stripe)
                    // gives the tab a slight 3-D lift.
                    NSRect t_highlight = NSMakeRect(t_r.origin.x + 1.0,
                                                    t_r.origin.y + 1.0,
                                                    t_r.size.width - 2.0,
                                                    3.0);
                    [[NSColor colorWithCalibratedWhite:1.0 alpha:0.25] setFill];
                    NSRectFillUsingOperation(t_highlight, NSCompositingOperationSourceOver);
                }];
            }
            else
            {
                // Unselected tab: textured-square push-button in the off state.
                NSButtonCell *t_cell = [[NSButtonCell alloc] init];
                [t_cell setButtonType:NSButtonTypePushOnPushOff];
                [t_cell setBezelStyle:NSBezelStyleTexturedSquare];
                [t_cell setTitle:@""];
                [t_cell setEnabled:!t_disabled];
                [t_cell setState:NSControlStateValueOff];
                [t_appearance performAsCurrentDrawingAppearance:^{
                    [t_cell drawWithFrame:t_r inView:t_view];
                }];
                [t_cell release];
            }
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
        // Draw a rounded-rectangle inset border with a white fill.
        case THEME_DRAW_TYPE_FRAME:
        {
            NSRect t_r = NSInsetRect(t_frame, 1.0, 1.0);
            [[NSColor controlBackgroundColor] setFill];
            [[NSBezierPath bezierPathWithRoundedRect:t_r
                                             xRadius:3.0
                                             yRadius:3.0] fill];
            [[NSColor separatorColor] setStroke];
            [[NSBezierPath bezierPathWithRoundedRect:t_r
                                             xRadius:3.0
                                             yRadius:3.0] stroke];
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

        // All other draw types are not yet handled; the caller's fallback
        // or no-op will deal with them.
        default:
            break;
    }

    [NSGraphicsContext restoreGraphicsState];
    CGContextRelease(t_cgcontext);

    // ── Blit rendered pixels → MCGContext ────────────────────────────
    MCGRaster t_raster;
    t_raster.width  = t_bitmap->width;
    t_raster.height = t_bitmap->height;
    t_raster.pixels = t_bitmap->data;
    t_raster.stride = t_bitmap->stride;
    t_raster.format = kMCGRasterFormat_ARGB;

    MCGRectangle t_dst = MCGRectangleMake(
        (MCGFloat)t_dest.x, (MCGFloat)t_dest.y,
        (MCGFloat)t_dest.width, (MCGFloat)t_dest.height);

    MCGContextDrawPixels(p_context, t_raster, t_dst, kMCGImageFilterMedium);

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
IO_stat MCHcstak::macreadresources(void) { return IO_ERROR; }

#endif // __arm64__
