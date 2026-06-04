//
// Linux toolbar backend for MCToolbar.
//
// Uses a child GdkWindow drawn entirely with Cairo/GdkPixbuf.  This is
// architecturally consistent with the rest of the engine, which uses raw
// GdkWindows throughout rather than wrapping everything in GtkWidgets.
//
// Design
// ------
//  • Create() creates a GdkWindow child of the stack's window, positioned at
//    y = getToolbarTopY() and sized width × kToolbarHeight.
//  • A per-window Xlib event filter (_filterFunc) handles Expose (draws
//    synchronously with Cairo) and ButtonPress (hit-tests items and fires
//    itemClicked).  Motion/Leave events drive the hover highlight.
//  • A second filter on the PARENT window (_parentResizeFunc) watches for
//    ConfigureNotify so we can resize the toolbar when the stack window resizes.
//  • No changes to lnxstack.cpp or lnxdclnx.cpp are required.
//
// Content offset
// --------------
//  The toolbar child window clips the parent window's drawing, so the top
//  kToolbarHeight pixels of the stack canvas are visually covered by the
//  toolbar.  Scripts should position card content below that offset.
//  (A future enhancement could expose getToolbarHeight() so the engine can
//   adjust the card viewport automatically.)
//
// GTK 4 note
// ----------
//  GdkWindow was replaced by GdkSurface in GTK 4; the whole file is guarded
//  with GTK_MAJOR_VERSION < 4.  A GtkBox+GtkButton replacement can be
//  written later for GTK 4 when needed.
//

#ifdef TARGET_PLATFORM_LINUX

// lnxprefix.h must come first: it includes GDK headers and wraps
// <gdk/gdkx.h> (and its X11 transitive includes) inside "namespace x11 {}"
// to prevent X11 macro/type clashes with the engine's own sysdefs.h.
// X11 struct types (XEvent, etc.) are therefore in the x11:: namespace;
// X11 macros (Expose, ButtonPress, …) are still global because #define
// ignores C++ namespaces.
#include "lnxprefix.h"

#include <gtk/gtk.h>        // GTK/GDK/Cairo types and helpers
#include "toolbar.h"

// ─────────────────────────────────────────────────────────────────────────────
// Tuneable constants
// ─────────────────────────────────────────────────────────────────────────────

static const int kToolbarHeight   = 48;   // fixed pixel height of the bar
static const int kIconSize        = 24;   // max icon dimension (square)
static const int kItemMinWidth    = 64;   // minimum width of a button item
static const int kSeparatorWidth  = 10;   // width of a separator item
static const int kSpaceWidth      = 16;   // width of a fixed-space item
static const int kFlexSpaceMin    = 4;    // minimum width of a flex-space item
static const int kLabelFontSize   = 10;   // points
static const int kIconLabelGap    = 2;    // pixels between icon bottom and label top

// ─────────────────────────────────────────────────────────────────────────────
// UTF-8 RAII helper
// ─────────────────────────────────────────────────────────────────────────────

struct MCAutoUTF8String
{
    char *m_str;
    MCAutoUTF8String() : m_str(nil) {}
    ~MCAutoUTF8String() { MCMemoryDelete(m_str); }

    bool Lock(MCStringRef p_src)
    {
        MCMemoryDelete(m_str);
        m_str = nil;
        if (!p_src || MCStringIsEmpty(p_src))
            return false;
        return MCStringConvertToUTF8String(p_src, m_str);
    }

    const char *operator*() const { return m_str ? m_str : ""; }
};

// ─────────────────────────────────────────────────────────────────────────────
// PNG → GdkPixbuf
// ─────────────────────────────────────────────────────────────────────────────

static GdkPixbuf *_pixbufFromPNGData(const void *p_bytes, uindex_t p_length)
{
    if (!p_bytes || p_length == 0)
        return NULL;

    GdkPixbufLoader *t_loader = gdk_pixbuf_loader_new_with_type("png", NULL);
    if (!t_loader)
        return NULL;

    GError *t_err = NULL;
    if (!gdk_pixbuf_loader_write(t_loader,
                                 (const guchar *)p_bytes, (gsize)p_length,
                                 &t_err))
    {
        if (t_err) g_error_free(t_err);
        gdk_pixbuf_loader_close(t_loader, NULL);
        g_object_unref(t_loader);
        return NULL;
    }
    gdk_pixbuf_loader_close(t_loader, NULL);

    GdkPixbuf *t_pixbuf = gdk_pixbuf_loader_get_pixbuf(t_loader);
    if (t_pixbuf)
        g_object_ref(t_pixbuf);
    g_object_unref(t_loader);
    return t_pixbuf;
}

// ─────────────────────────────────────────────────────────────────────────────
// Per-item storage (in-place constructed inside a fixed array)
// ─────────────────────────────────────────────────────────────────────────────

struct LnxItemData
{
    MCNewAutoNameRef   name;
    MCToolbarItemStyle style;
    bool               enabled;
    GdkPixbuf         *icon;    // caller-owned ref; NULL if absent
    char              *label;   // heap-allocated UTF-8; NULL if absent
    int                x;       // assigned by _relayout()
    int                width;   // assigned by _relayout()

    LnxItemData()
        : style(kMCToolbarItemStyleButton), enabled(true),
          icon(NULL), label(NULL), x(0), width(0) {}

    ~LnxItemData()
    {
        if (icon)  { g_object_unref(icon);  icon  = NULL; }
        if (label) { free(label);            label = NULL; }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Backend class
// ─────────────────────────────────────────────────────────────────────────────

class MCToolbarLinuxBackend : public MCToolbarBackend
{
public:
    MCToolbarLinuxBackend(MCToolbar *p_owner)
        : m_owner(p_owner), m_window(NULL), m_parent(NULL),
          m_item_count(0), m_visible(true),
          m_display_mode(kMCToolbarDisplayModeDefault),
          m_top_y(0), m_parent_width(0), m_hover_idx(-1)
    {}

    ~MCToolbarLinuxBackend() override { Destroy(); }

    // ── lifecycle ────────────────────────────────────────────────────────────

    void Create(void *p_window_handle) override
    {
#if GTK_MAJOR_VERSION < 4
        // Idempotent: clean up any previous window so that calling Create()
        // a second time (e.g. a second 'createToolbar' without a prior close)
        // starts from a known-good state instead of leaking the old window and
        // double-registering event filters.
        Destroy();

        if (!p_window_handle)
            return;

        m_parent = (GdkWindow *)p_window_handle;
        m_top_y  = m_owner->getToolbarTopY();
        m_parent_width = gdk_window_get_width(m_parent);

        GdkWindowAttr t_attr;
        memset(&t_attr, 0, sizeof(t_attr));
        t_attr.window_type = GDK_WINDOW_CHILD;
        t_attr.wclass      = GDK_INPUT_OUTPUT;
        t_attr.x           = 0;
        t_attr.y           = m_top_y;
        t_attr.width       = m_parent_width > 0 ? m_parent_width : 1;
        t_attr.height      = kToolbarHeight;
        // Match the parent's visual and colormap exactly.  The engine may use
        // an RGBA composite visual; if the child window uses a different visual
        // the compositor treats it as transparent.
        t_attr.visual   = gdk_drawable_get_visual((GdkDrawable *)m_parent);
        t_attr.colormap = gdk_drawable_get_colormap((GdkDrawable *)m_parent);

        m_window = gdk_window_new(m_parent, &t_attr,
                                  GDK_WA_X | GDK_WA_Y |
                                  GDK_WA_VISUAL | GDK_WA_COLORMAP);
        if (!m_window)
            return;

        // Hold extra GObject references on both windows.  When the engine
        // calls gdk_window_destroy(parent), GDK internally recurses into all
        // GDK-registered children and calls g_object_unref on each one.
        // Without these extra refs that drops the child's GdkWindowObject
        // refcount to zero, freeing the memory.  Then when MCToolbar::close()
        // → Destroy() runs (AFTER stack destroywindow()), m_window is a
        // dangling pointer and any GDK call on it crashes.  By holding an
        // extra ref here we guarantee the GdkWindowObjects stay allocated
        // until we explicitly release them at the bottom of Destroy().
        g_object_ref(m_window);
        g_object_ref(m_parent);

        // Request the events we need for drawing and interaction.
        gdk_window_set_events(m_window,
            GdkEventMask(GDK_EXPOSURE_MASK      |
                         GDK_BUTTON_PRESS_MASK   |
                         GDK_POINTER_MOTION_MASK |
                         GDK_ENTER_NOTIFY_MASK   |
                         GDK_LEAVE_NOTIFY_MASK));

        // Per-window Xlib filters.  The toolbar window filter handles all
        // toolbar events.  The parent window filter watches for ConfigureNotify
        // so we can resize the toolbar when the stack window resizes.
        gdk_window_add_filter(m_window, _filterFunc,       this);
        gdk_window_add_filter(m_parent, _parentResizeFunc, this);

        _relayout();

        if (m_visible)
            gdk_window_show(m_window);
        else
            gdk_window_hide(m_window);

        // Force an initial paint immediately — don't rely on the first Expose
        // event arriving before the user sees the window.
        if (m_visible)
            _redraw();
#endif
    }

    void Destroy() override
    {
#if GTK_MAJOR_VERSION < 4
        for (int i = 0; i < m_item_count; i++)
            _clearItem(i);
        m_item_count = 0;
        m_hover_idx  = -1;

        if (m_window)
        {
            gdk_window_remove_filter(m_window, _filterFunc, this);
            // gdk_window_destroy is a no-op if GDK has already marked this
            // window as destroyed (e.g. because the parent was destroyed first
            // and GDK recursively destroyed all children).
            // The tail-call g_object_unref inside gdk_window_destroy ALWAYS
            // runs though: it drops the GDK/engine ref (refcount 2→1).
            gdk_window_destroy(m_window);
            // Release our extra ref (taken in Create) — refcount 1→0, frees
            // the GdkWindowObject.  Safe to call because our extra ref kept
            // the object allocated through gdk_window_destroy above.
            g_object_unref(m_window);
            m_window = NULL;
        }
        if (m_parent)
        {
            gdk_window_remove_filter(m_parent, _parentResizeFunc, this);
            g_object_unref(m_parent);
            m_parent = NULL;
        }
#endif
    }

    // ── items ────────────────────────────────────────────────────────────────

    void AddItem(const MCToolbarItem *p_item) override
    {
#if GTK_MAJOR_VERSION < 4
        if (!m_window || m_item_count >= 256)
            return;

        int t_idx = m_item_count;
        LnxItemData *it = &m_items[t_idx];
        // slot is already default-constructed (part of the member array)
        it->name.Reset(p_item->GetName());
        it->style   = p_item->GetStyle();
        it->enabled = p_item->GetEnabled();

        // Icon from cached PNG bytes
        MCDataRef t_img = p_item->GetImageData();
        if (t_img && !MCDataIsEmpty(t_img))
            it->icon = _pixbufFromPNGData(MCDataGetBytePtr(t_img),
                                          (uindex_t)MCDataGetLength(t_img));

        // Label
        MCAutoUTF8String t_lbl;
        if (p_item->GetLabel() && !MCStringIsEmpty(p_item->GetLabel()) &&
            t_lbl.Lock(p_item->GetLabel()))
            it->label = strdup(*t_lbl);

        m_item_count++;
        _relayout();
        _redraw();
#endif
    }

    void RemoveItem(MCNameRef p_name) override
    {
#if GTK_MAJOR_VERSION < 4
        int t_found = -1;
        for (int i = 0; i < m_item_count; i++)
        {
            if (MCNameIsEqualTo(*m_items[i].name, p_name, kMCCompareCaseless))
            {
                t_found = i;
                break;
            }
        }
        if (t_found < 0)
            return;

        _clearItem(t_found);

        // Shift remaining items down.
        for (int j = t_found; j < m_item_count - 1; j++)
        {
            // slot j is already clear; transfer slot j+1 into it
            m_items[j].name.Reset(*m_items[j + 1].name);
            m_items[j].style   = m_items[j + 1].style;
            m_items[j].enabled = m_items[j + 1].enabled;
            m_items[j].icon    = m_items[j + 1].icon;    m_items[j + 1].icon  = NULL;
            m_items[j].label   = m_items[j + 1].label;   m_items[j + 1].label = NULL;
            _clearItem(j + 1);
        }

        m_item_count--;

        if (m_hover_idx >= m_item_count)
            m_hover_idx = -1;

        _relayout();
        _redraw();
#endif
    }

    void UpdateItem(const MCToolbarItem *p_item) override
    {
#if GTK_MAJOR_VERSION < 4
        for (int i = 0; i < m_item_count; i++)
        {
            if (!MCNameIsEqualTo(*m_items[i].name, p_item->GetName(),
                                 kMCCompareCaseless))
                continue;

            LnxItemData *it = &m_items[i];

            it->enabled = p_item->GetEnabled();

            // label
            if (it->label) { free(it->label); it->label = NULL; }
            MCAutoUTF8String t_lbl;
            if (p_item->GetLabel() && !MCStringIsEmpty(p_item->GetLabel()) &&
                t_lbl.Lock(p_item->GetLabel()))
                it->label = strdup(*t_lbl);

            // icon
            if (it->icon) { g_object_unref(it->icon); it->icon = NULL; }
            MCDataRef t_img = p_item->GetImageData();
            if (t_img && !MCDataIsEmpty(t_img))
                it->icon = _pixbufFromPNGData(MCDataGetBytePtr(t_img),
                                              (uindex_t)MCDataGetLength(t_img));

            _relayout();
            _redraw();
            return;
        }
#endif
    }

    void ClearItems() override
    {
#if GTK_MAJOR_VERSION < 4
        for (int i = 0; i < m_item_count; i++)
            _clearItem(i);
        m_item_count = 0;
        m_hover_idx  = -1;
        if (m_window)
            _redraw();
#endif
    }

    // ── state ────────────────────────────────────────────────────────────────

    void SetDisplayMode(MCToolbarDisplayMode p_mode) override
    {
        m_display_mode = p_mode;
#if GTK_MAJOR_VERSION < 4
        if (m_window)
            _redraw();
#endif
    }

    void SetVisible(bool p_visible) override
    {
        m_visible = p_visible;
#if GTK_MAJOR_VERSION < 4
        if (m_window)
        {
            if (p_visible)
                gdk_window_show(m_window);
            else
                gdk_window_hide(m_window);
        }
#endif
    }

    bool GetVisible() override { return m_visible; }

private:
    MCToolbar           *m_owner;
    GdkWindow           *m_window;       // toolbar child window
    GdkWindow           *m_parent;       // owning stack's GdkWindow
    int                  m_item_count;
    bool                 m_visible;
    MCToolbarDisplayMode m_display_mode;
    int                  m_top_y;        // y offset inside parent
    int                  m_parent_width;
    int                  m_hover_idx;    // -1 = none
    LnxItemData          m_items[256];

    // ── helpers ──────────────────────────────────────────────────────────────

    // Release and null all resources owned by slot i without ending the
    // object's lifetime.  After this call the slot is safe to overwrite or
    // to have its implicit destructor run (which will then be a no-op).
    void _clearItem(int i)
    {
        m_items[i].name.Reset();  // MCAutoValueRefBase::Reset(nil): releases and zeroes m_value
        if (m_items[i].icon)  { g_object_unref(m_items[i].icon);  m_items[i].icon  = NULL; }
        if (m_items[i].label) { free(m_items[i].label);            m_items[i].label = NULL; }
    }

    // ── layout ───────────────────────────────────────────────────────────────

    void _relayout()
    {
        // First pass: assign fixed widths.
        int t_fixed = 0;
        int t_nflex = 0;
        for (int i = 0; i < m_item_count; i++)
        {
            switch (m_items[i].style)
            {
                case kMCToolbarItemStyleSeparator:
                    m_items[i].width = kSeparatorWidth;  break;
                case kMCToolbarItemStyleSpace:
                    m_items[i].width = kSpaceWidth;      break;
                case kMCToolbarItemStyleFlexSpace:
                    m_items[i].width = kFlexSpaceMin;
                    t_nflex++;
                    break;
                default: // button
                    m_items[i].width = kItemMinWidth;    break;
            }
            t_fixed += m_items[i].width;
        }

        // Second pass: distribute leftover space to flex items.
        int t_remain = m_parent_width - t_fixed;
        if (t_remain > 0 && t_nflex > 0)
        {
            int t_extra = t_remain / t_nflex;
            for (int i = 0; i < m_item_count; i++)
                if (m_items[i].style == kMCToolbarItemStyleFlexSpace)
                    m_items[i].width += t_extra;
        }

        // Third pass: x positions.
        int t_x = 0;
        for (int i = 0; i < m_item_count; i++)
        {
            m_items[i].x = t_x;
            t_x += m_items[i].width;
        }
    }

    // ── hit testing ──────────────────────────────────────────────────────────

    int _hitTest(int p_x, int /*p_y*/)
    {
        for (int i = 0; i < m_item_count; i++)
        {
            if (m_items[i].style != kMCToolbarItemStyleButton)
                continue;
            if (p_x >= m_items[i].x &&
                p_x <  m_items[i].x + m_items[i].width)
                return i;
        }
        return -1;
    }

    // ── drawing ──────────────────────────────────────────────────────────────

    void _redraw()
    {
        if (!m_window)
            return;

        int t_w = gdk_window_get_width(m_window);
        int t_h = gdk_window_get_height(m_window);

        cairo_t *cr = gdk_cairo_create(m_window);
        if (!cr)
            return;

        // Background gradient
        {
            cairo_pattern_t *t_grad =
                cairo_pattern_create_linear(0, 0, 0, t_h);
            cairo_pattern_add_color_stop_rgb(t_grad, 0.0, 0.93, 0.93, 0.93);
            cairo_pattern_add_color_stop_rgb(t_grad, 1.0, 0.80, 0.80, 0.80);
            cairo_set_source(cr, t_grad);
            cairo_rectangle(cr, 0, 0, t_w, t_h);
            cairo_fill(cr);
            cairo_pattern_destroy(t_grad);
        }

        bool t_show_icon  = (m_display_mode != kMCToolbarDisplayModeLabelOnly);
        bool t_show_label = (m_display_mode != kMCToolbarDisplayModeIconOnly);

        for (int i = 0; i < m_item_count; i++)
            _drawItem(cr, i, (m_hover_idx == i), t_show_icon, t_show_label, t_h);

        // Bottom border line
        cairo_set_source_rgb(cr, 0.55, 0.55, 0.55);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, 0,   t_h - 0.5);
        cairo_line_to(cr, t_w, t_h - 0.5);
        cairo_stroke(cr);

        cairo_destroy(cr);
    }

    void _drawItem(cairo_t *cr, int idx, bool hovered,
                   bool show_icon, bool show_label, int bar_h)
    {
        LnxItemData *it = &m_items[idx];
        int ix = it->x, iw = it->width;

        // Separator
        if (it->style == kMCToolbarItemStyleSeparator)
        {
            cairo_set_source_rgb(cr, 0.55, 0.55, 0.55);
            cairo_set_line_width(cr, 1.0);
            double t_cx = ix + iw / 2.0;
            cairo_move_to(cr, t_cx, 6);
            cairo_line_to(cr, t_cx, bar_h - 6);
            cairo_stroke(cr);
            return;
        }

        // Spaces: nothing visible
        if (it->style != kMCToolbarItemStyleButton)
            return;

        // Hover highlight
        if (hovered && it->enabled)
        {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.55);
            cairo_rectangle(cr, ix + 2, 2, iw - 4, bar_h - 4);
            cairo_fill(cr);

            cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.12);
            cairo_set_line_width(cr, 1.0);
            cairo_rectangle(cr, ix + 2.5, 2.5, iw - 5, bar_h - 5);
            cairo_stroke(cr);
        }

        double t_alpha = it->enabled ? 1.0 : 0.38;
        int t_icon_bottom = 0;

        // Icon
        if (show_icon && it->icon)
        {
            int t_src_w = gdk_pixbuf_get_width(it->icon);
            int t_src_h = gdk_pixbuf_get_height(it->icon);

            double t_scale = 1.0;
            if (t_src_w > kIconSize || t_src_h > kIconSize)
                t_scale = (double)kIconSize /
                          (t_src_w > t_src_h ? t_src_w : t_src_h);
            int t_dw = (int)(t_src_w * t_scale);
            int t_dh = (int)(t_src_h * t_scale);

            int t_iy = (show_label && it->label && it->label[0])
                       ? 4
                       : (bar_h - t_dh) / 2;
            int t_icon_x = ix + (iw - t_dw) / 2;

            cairo_save(cr);
            cairo_translate(cr, t_icon_x, t_iy);
            if (t_scale != 1.0)
                cairo_scale(cr, t_scale, t_scale);
            gdk_cairo_set_source_pixbuf(cr, it->icon, 0, 0);
            cairo_paint_with_alpha(cr, t_alpha);
            cairo_restore(cr);

            t_icon_bottom = t_iy + t_dh;
        }

        // Label — use Pango for proper Unicode rendering
        if (show_label && it->label && it->label[0])
        {
            PangoLayout *t_layout = pango_cairo_create_layout(cr);
            PangoFontDescription *t_fd =
                pango_font_description_from_string("Sans 10");
            pango_layout_set_font_description(t_layout, t_fd);
            pango_font_description_free(t_fd);
            pango_layout_set_text(t_layout, it->label, -1);

            int t_pw = 0, t_ph = 0;
            pango_layout_get_pixel_size(t_layout, &t_pw, &t_ph);

            double t_lx = ix + (iw - t_pw) / 2.0;
            if (t_lx < ix + 2.0) t_lx = ix + 2.0;

            double t_ly;
            if (t_icon_bottom > 0)
                t_ly = (double)(t_icon_bottom + kIconLabelGap);
            else
                t_ly = (bar_h - t_ph) / 2.0;

            if (it->enabled)
                cairo_set_source_rgb(cr, 0.10, 0.10, 0.10);
            else
                cairo_set_source_rgba(cr, 0.10, 0.10, 0.10, t_alpha);

            cairo_move_to(cr, t_lx, t_ly);
            pango_cairo_show_layout(cr, t_layout);
            g_object_unref(t_layout);
        }
    }

    // ── Xlib event filters ───────────────────────────────────────────────────

    // Filter for the toolbar child window.
    static GdkFilterReturn _filterFunc(GdkXEvent *p_xevent,
                                       GdkEvent  * /*p_event*/,
                                       gpointer   p_data)
    {
        MCToolbarLinuxBackend *self =
            static_cast<MCToolbarLinuxBackend *>(p_data);
        x11::XEvent *xev = static_cast<x11::XEvent *>(p_xevent);

        switch (xev->type)
        {
            case DestroyNotify:
                // The toolbar child window has been destroyed — either by
                // our own Destroy() or because the parent window was
                // destroyed by the engine (which implicitly destroys all
                // child X windows).  NULL the pointer now so that Destroy()
                // does not call GDK functions on a freed GdkWindowObject.
                self->m_window = NULL;
                return GDK_FILTER_REMOVE;

            case Expose:
                // Redraw only on the last expose in a series (count == 0).
                if (xev->xexpose.count == 0)
                    self->_redraw();
                return GDK_FILTER_REMOVE;

            case ButtonPress:
                if (xev->xbutton.button == Button1)
                {
                    int t_idx = self->_hitTest(xev->xbutton.x, xev->xbutton.y);
                    if (t_idx >= 0 &&
                        self->m_items[t_idx].enabled &&
                        self->m_items[t_idx].style == kMCToolbarItemStyleButton)
                    {
                        self->m_owner->itemClicked(
                            *self->m_items[t_idx].name);
                    }
                }
                return GDK_FILTER_REMOVE;

            case MotionNotify:
            {
                int t_old = self->m_hover_idx;
                self->m_hover_idx =
                    self->_hitTest(xev->xmotion.x, xev->xmotion.y);
                if (self->m_hover_idx != t_old)
                    self->_redraw();
                return GDK_FILTER_REMOVE;
            }

            case LeaveNotify:
                if (self->m_hover_idx != -1)
                {
                    self->m_hover_idx = -1;
                    self->_redraw();
                }
                return GDK_FILTER_REMOVE;

            case EnterNotify:
                // Motion handler will update hover on the first move.
                return GDK_FILTER_REMOVE;

            default:
                return GDK_FILTER_CONTINUE;
        }
    }

    // Filter on the PARENT (stack) window: watch for ConfigureNotify.
    static GdkFilterReturn _parentResizeFunc(GdkXEvent *p_xevent,
                                             GdkEvent  * /*p_event*/,
                                             gpointer   p_data)
    {
        MCToolbarLinuxBackend *self =
            static_cast<MCToolbarLinuxBackend *>(p_data);
        x11::XEvent *xev = static_cast<x11::XEvent *>(p_xevent);

        if (xev->type == DestroyNotify)
        {
            // The parent (stack) window is being destroyed.  X automatically
            // destroys all child windows too, so our toolbar GdkWindow is
            // gone as well.  NULL both pointers so that Destroy() does not
            // call GDK functions on freed GdkWindowObjects.
            self->m_parent = NULL;
            self->m_window = NULL;
            // Fall through — let the engine's own DestroyNotify handler run.
            return GDK_FILTER_CONTINUE;
        }

        if (xev->type == ConfigureNotify && self->m_window)
        {
            int t_new_w = xev->xconfigure.width;
            if (t_new_w != self->m_parent_width)
            {
                self->m_parent_width = t_new_w;
                gdk_window_resize(self->m_window, t_new_w, kToolbarHeight);
                self->_relayout();
                self->_redraw();
            }
        }

        // Never consume parent events.
        return GDK_FILTER_CONTINUE;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Factory
// ─────────────────────────────────────────────────────────────────────────────

MCToolbarBackend *MCToolbarCreatePlatformBackend(MCToolbar *p_owner)
{
    return new MCToolbarLinuxBackend(p_owner);
}

#endif // TARGET_PLATFORM_LINUX
