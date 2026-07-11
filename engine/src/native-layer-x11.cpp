/* Copyright (C) 2015 LiveCode Ltd.

 This file is part of LiveCode.

 LiveCode is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License v3 as published by the Free
 Software Foundation.

 LiveCode is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License
 along with LiveCode.  If not see <http://www.gnu.org/licenses/>.  */

#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"


#include "util.h"
#include "mcerror.h"
#include "sellst.h"
#include "stack.h"
#include "card.h"
#include "image.h"
#include "widget.h"
#include "param.h"
#include "osspec.h"
#include "cmds.h"
#include "scriptpt.h"
#include "hndlrlst.h"
#include "debug.h"
#include "redraw.h"
#include "font.h"
#include "chunk.h"
#include "graphicscontext.h"
#include "objptr.h"

#include "globals.h"
#include "context.h"
#include "stacklst.h"

#include "lnxdc.h"
#include "graphicscontext.h"
#include "graphics_util.h"

#include "native-layer-x11.h"

#include <stdio.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
// X11 symbols wrapped in namespace x11 to avoid clashes with Window/Pixmap/Drawable
// typedefs used elsewhere in the engine (same pattern as lnxprefix.h).
namespace x11 {
#include <gdk/gdkx.h>
}

#include <string.h>   // memcpy


// Design notes — offscreen rendering
// -----------------------------------
// Previous approach: m_child_window was a GTK_WINDOW_TOPLEVEL.  WebKit
// rendered into a visible X11 window whose Z-order was managed by Mutter/
// XWayland.  Under XWayland all client-side X11 stacking requests
// (XRaiseWindow, _NET_RESTACK_WINDOW, WM_TRANSIENT_FOR changes on already-
// mapped windows) are either ignored or not propagated to the Wayland
// compositor.  Consequently no combination of hints, transient chains or timer-
// driven re-stacking could reliably keep WM_PALETTE windows above the browser
// when the user clicked on the browser.
//
// Current approach: GtkOffscreenWindow.
// m_child_window is a GtkOffscreenWindow — it renders children into an
// off-screen surface with no visible X11 window.  GTK routes pointer events
// dispatched via gtk_main_do_event() to children by coordinate, preserving
// all internal WebKit state (focus, hit-test context, pointer tracking) that
// direct sub-widget targeting bypasses.  The event filter translates HXT
// stack-window coordinates to the offscreen window's coordinate space before
// dispatching.
//
// Z-order: the hidden window is off-screen, so it never overlaps any visible
// window, including palette windows.  Browser content is composited into HXT's
// own MCGContext by doPaint(), so it appears as part of the stack's normal
// rendering hierarchy with no separate on-screen browser window.
//
// Interactivity: a GDK event filter installed on the stack window intercepts
// pointer and keyboard events whose coordinates fall within m_rect and forwards
// them to the WebKit widget via gtk_main_do_event() with coordinates translated
// to be browser-widget-relative.  With a real X11 GdkWindow, GTK's normal event
// dispatch machinery works correctly for clicks, navigation, scrolling, etc.
//
// Performance: software rendering is slower for JS-heavy pages and media, but
// is acceptable for the typical HXT browser use case (forms, documentation,
// simple web content).  On native X11 (non-XWayland) hardware acceleration
// could be re-enabled in future once a clean Z-order solution exists.


// Module-global: the X11 native layer that currently owns keyboard focus for
// its embedded browser widget.  Set in OnMouseDown, cleared in doDetach and
// the destructor.  Read by hxt_browser_key_down/up called from lnxdclnx.cpp.
MCNativeLayerX11 *MCNativeLayerX11::s_focused_browser_layer = NULL;

// Called from lnxdclnx.cpp's GDK_KEY_PRESS/RELEASE handler immediately after
// the normal HXT wkdown/wkup dispatch.  Forwards the raw GDK key event to the
// focused offscreen WebKitWebView by synthesising a new GdkEventKey and
// dispatching it via gtk_main_do_event() to the GtkOffscreenWindow.  GTK
// routes it to the internally focused child (the WebKitWebView).
void hxt_browser_key_down(unsigned int p_keyval, unsigned int p_state,
                           unsigned short p_hwcode, unsigned char p_group)
{
    MCNativeLayerX11 *t_layer = MCNativeLayerX11::s_focused_browser_layer;
    if (t_layer == NULL || !t_layer->m_browser_focused)
        return;
    t_layer->dispatchKeyEvent(GDK_KEY_PRESS, p_keyval, p_state, p_hwcode, p_group);
}

void hxt_browser_key_up(unsigned int p_keyval, unsigned int p_state,
                         unsigned short p_hwcode, unsigned char p_group)
{
    MCNativeLayerX11 *t_layer = MCNativeLayerX11::s_focused_browser_layer;
    if (t_layer == NULL || !t_layer->m_browser_focused)
        return;
    t_layer->dispatchKeyEvent(GDK_KEY_RELEASE, p_keyval, p_state, p_hwcode, p_group);
}

// Synthesises a GdkEventKey and dispatches it to the GtkOffscreenWindow so
// GTK routes it to the focused WebKitWebView child.
void MCNativeLayerX11::dispatchKeyEvent(GdkEventType p_type,
                                         unsigned int  p_keyval,
                                         unsigned int  p_state,
                                         unsigned short p_hwcode,
                                         unsigned char  p_group)
{
    if (m_child_window == NULL || m_browser_widget == NULL)
        return;
    GdkWindow *t_win = gtk_widget_get_window(GTK_WIDGET(m_child_window));
    if (t_win == NULL)
        return;

    fprintf(stderr, "[HXT] dispatchKeyEvent type=%s keyval=0x%04x\n",
        p_type == GDK_KEY_PRESS ? "PRESS" : "RELEASE", p_keyval);

    GdkEvent *evt = gdk_event_new(p_type);
    evt->key.window          = t_win;
    g_object_ref(t_win);
    evt->key.send_event      = TRUE;
    evt->key.time            = GDK_CURRENT_TIME;
    evt->key.state           = (GdkModifierType)p_state;
    evt->key.keyval          = p_keyval;
    evt->key.hardware_keycode = p_hwcode;
    evt->key.group           = p_group;
    evt->key.is_modifier     = 0;
    evt->key.length          = 0;
    evt->key.string          = NULL;
    gtk_main_do_event(evt);
    gdk_event_free(evt);
}

MCNativeLayerX11::MCNativeLayerX11(MCObject *p_object, GtkWidget *p_view) :
  m_child_window(NULL),
  m_browser_widget(p_view),
  m_damage_signal_id(0),
  m_redraw_pending(false),
  m_paint_timer_id(0),
  m_browser_focused(false)
{
    m_object = p_object;
    m_intersect_rect = MCRectangleMake(0,0,0,0);
}

MCNativeLayerX11::~MCNativeLayerX11()
{
    if (s_focused_browser_layer == this)
        s_focused_browser_layer = NULL;
    if (m_paint_timer_id != 0)
    {
        g_source_remove(m_paint_timer_id);
        m_paint_timer_id = 0;
    }
    if (m_damage_signal_id != 0 && m_child_window != NULL)
    {
        g_signal_handler_disconnect(m_child_window, m_damage_signal_id);
        m_damage_signal_id = 0;
    }
    if (m_child_window != NULL)
    {
        gdk_window_remove_filter(getStackGdkWindow(), onStackWindowFilter, this);
        gtk_widget_destroy(GTK_WIDGET(m_child_window));
        m_child_window = NULL;
    }
}

void MCNativeLayerX11::OnToolChanged(Tool p_new_tool)
{
    // No input-shape management needed for offscreen rendering.
    MCNativeLayer::OnToolChanged(p_new_tool);
    fprintf(stderr, "[HXT] OnToolChanged: p_new_tool=%d T_BROWSE=%d show_for_tool=%d\n",
        (int)p_new_tool, (int)T_BROWSE, (int)m_show_for_tool);
}

// Called by MCWidget::mdown — coordinates are widget-relative.
void MCNativeLayerX11::OnMouseDown(int p_x, int p_y)
{
    fprintf(stderr, "[HXT] OnMouseDown(%d,%d) visible=%d child=%p widget=%p\n",
        p_x, p_y, (int)m_visible, (void*)m_child_window, (void*)m_browser_widget);
    if (!m_visible || m_child_window == NULL || m_browser_widget == NULL)
        return;
    // Record that this layer owns keyboard focus so hxt_browser_key_down/up
    // route subsequent key events here.
    m_browser_focused = true;
    s_focused_browser_layer = this;
    gtk_widget_grab_focus(m_browser_widget);

    // GtkOffscreenWindow never receives a real X11 focus event, so GTK's
    // "has-toplevel-focus" stays false and WebKit suppresses the text caret.
    // Synthesise a GDK_FOCUS_CHANGE(in=TRUE) so GTK sets that flag and
    // WebKit draws the insertion cursor in focused form fields.
    GdkWindow *t_win = gtk_widget_get_window(GTK_WIDGET(m_child_window));
    if (t_win)
    {
        GdkEvent *t_focus = gdk_event_new(GDK_FOCUS_CHANGE);
        t_focus->focus_change.window    = t_win;
        g_object_ref(t_win);
        t_focus->focus_change.send_event = TRUE;
        t_focus->focus_change.in         = TRUE;
        gtk_main_do_event(t_focus);
        gdk_event_free(t_focus);
        fprintf(stderr, "[HXT] synthetic GDK_FOCUS_CHANGE(in) sent to offscreen win\n");
    }
}

// Called by MCWidget::mup — coordinates are widget-relative.
// Invokes the SimulateClick bridge to find the anchor at (p_x, p_y) via JS
// and navigate to it.
void MCNativeLayerX11::OnMouseUp(int p_x, int p_y)
{
    fprintf(stderr, "[HXT] OnMouseUp(%d,%d) visible=%d child=%p widget=%p\n",
        p_x, p_y, (int)m_visible, (void*)m_child_window, (void*)m_browser_widget);
    if (!m_visible || m_child_window == NULL || m_browser_widget == NULL)
        return;

    typedef void (*HXTSimFn)(void*, int, int);
    HXTSimFn fn = (HXTSimFn)g_object_get_data(
        G_OBJECT(m_browser_widget), "hxt-sim-fn");
    void *ctx = g_object_get_data(
        G_OBJECT(m_browser_widget), "hxt-sim-ctx");
    fprintf(stderr, "[HXT] OnMouseUp bridge fn=%p ctx=%p\n", (void*)fn, ctx);
    if (fn && ctx)
        fn(ctx, p_x, p_y);
}

// The browser content can be composited directly into HXT's context via
// doPaint(), so this native layer supports render-to-context.
bool MCNativeLayerX11::GetCanRenderToContext()
{
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Damage → HXT repaint

// static
// Called by GtkOffscreenWindow when WebKit has new content in the offscreen
// surface.  Schedules a single idle-priority HXT Redraw() to repaint the
// widget area; m_redraw_pending prevents redundant idle callbacks.
gboolean MCNativeLayerX11::onDamage(GtkWidget * /*widget*/, GdkEvent * /*event*/,
                                     gpointer user_data)
{
    MCNativeLayerX11 *t_layer = static_cast<MCNativeLayerX11*>(user_data);
    if (!t_layer->m_redraw_pending && t_layer->m_object != NULL)
    {
        t_layer->m_redraw_pending = true;
        g_idle_add(onRedrawIdle, t_layer);
    }
    return FALSE; // do not suppress further handlers
}

// static
gboolean MCNativeLayerX11::onRedrawIdle(gpointer user_data)
{
    MCNativeLayerX11 *t_layer = static_cast<MCNativeLayerX11*>(user_data);
    t_layer->m_redraw_pending = false;
    if (t_layer->m_object != NULL)
        t_layer->m_object->Redraw();
    return G_SOURCE_REMOVE;
}

////////////////////////////////////////////////////////////////////////////////
// Event forwarding — stack window → offscreen browser

// static
// GDK event filter installed on the stack's GdkWindow while the browser is
// attached.  Intercepts pointer events whose (x,y) falls within m_rect and
// keyboard events when the browser has logical focus, then forwards them to
// the WebKit widget via gtk_main_do_event() (pointer) / gtk_widget_event()
// (keyboard) with coordinates translated to be browser-widget-relative.
GdkFilterReturn MCNativeLayerX11::onStackWindowFilter(GdkXEvent *p_xevent,
                                                       GdkEvent   * /*event*/,
                                                       gpointer    user_data)
{
    // The HXT stack GdkWindow is a foreign X11 window (created by raw X11, not
    // GTK).  GDK wraps it but translates all events to GDK_NOTHING (-1).
    // We must read the raw XEvent (first argument) directly.
    x11::XEvent *xev = (x11::XEvent*)p_xevent;
    if (xev == NULL)
        return GDK_FILTER_CONTINUE;

    // Only handle pointer events.
    bool t_is_press   = (xev->type == ButtonPress);
    bool t_is_release = (xev->type == ButtonRelease);
    bool t_is_motion  = (xev->type == MotionNotify);
    if (!t_is_press && !t_is_release && !t_is_motion)
        return GDK_FILTER_CONTINUE;

    fprintf(stderr, "[HXT] raw xev type=%d (Bp=%d Br=%d Mn=%d)\n",
        xev->type, ButtonPress, ButtonRelease, MotionNotify);

    MCNativeLayerX11 *t_layer = static_cast<MCNativeLayerX11*>(user_data);

    // Log gate state always; skip only if truly unusable (no window).
    fprintf(stderr, "[HXT] filter gate: visible=%d show_for_tool=%d child=%p\n",
        (int)t_layer->m_visible, (int)t_layer->m_show_for_tool,
        (void*)t_layer->m_child_window);
    if (!t_layer->m_visible || t_layer->m_child_window == NULL)
        return GDK_FILTER_CONTINUE;
    // NOTE: show_for_tool gate bypassed for debugging — proceed regardless.

    const MCRectangle &r = t_layer->m_rect;

    int ex = t_is_motion ? xev->xmotion.x  : xev->xbutton.x;
    int ey = t_is_motion ? xev->xmotion.y  : xev->xbutton.y;
    int tb = t_is_motion ? 0                : xev->xbutton.button;

    bool t_in_rect = (ex >= r.x && ex < r.x + r.width &&
                      ey >= r.y && ey < r.y + r.height);

    fprintf(stderr, "[HXT] ptr at (%d,%d) rect(%d,%d,%d,%d) in=%d\n",
        ex, ey, r.x, r.y, r.width, r.height, (int)t_in_rect);

    if (t_is_press)
    {
        t_layer->m_browser_focused = t_in_rect;
        if (t_in_rect && t_layer->m_browser_widget != NULL)
            gtk_widget_grab_focus(t_layer->m_browser_widget);
    }

    if (!t_in_rect)
        return GDK_FILTER_CONTINUE;

    int t_bx = ex - r.x;
    int t_by = ey - r.y;

    // Scroll wheel: X11 delivers scroll as ButtonPress with button 4 (up),
    // 5 (down), 6 (left), 7 (right).  Synthesize a GDK_SCROLL event targeted
    // at the browser widget and return GDK_FILTER_REMOVE so GDK never queues
    // the event for HXT — without this, HXT would scroll its own view instead.
    if (t_is_press && tb >= 4 && tb <= 7)
    {
        GdkScrollDirection t_dir;
        switch (tb)
        {
            case 4:  t_dir = GDK_SCROLL_UP;    break;
            case 5:  t_dir = GDK_SCROLL_DOWN;  break;
            case 6:  t_dir = GDK_SCROLL_LEFT;  break;
            default: t_dir = GDK_SCROLL_RIGHT; break; // 7
        }

        GdkWindow *t_bwin = gtk_widget_get_window(t_layer->m_browser_widget);
        if (t_bwin)
        {
            GdkEvent *t_scroll = gdk_event_new(GDK_SCROLL);
            t_scroll->scroll.window     = t_bwin;
            g_object_ref(t_bwin);
            t_scroll->scroll.send_event = TRUE;
            t_scroll->scroll.time       = xev->xbutton.time;
            t_scroll->scroll.x          = t_bx;
            t_scroll->scroll.y          = t_by;
            t_scroll->scroll.x_root     = xev->xbutton.x_root;
            t_scroll->scroll.y_root     = xev->xbutton.y_root;
            t_scroll->scroll.state      = (GdkModifierType)xev->xbutton.state;
            t_scroll->scroll.direction  = t_dir;
            t_scroll->scroll.delta_x    = 0.0;
            t_scroll->scroll.delta_y    = 0.0;
            fprintf(stderr, "[HXT] scroll btn=%d dir=%d → browser\n", tb, (int)t_dir);
            gtk_main_do_event(t_scroll);
            gdk_event_free(t_scroll);
        }
        return GDK_FILTER_REMOVE; // suppress from HXT
    }
    // ButtonRelease for scroll buttons: suppress the corresponding release too.
    if (t_is_release && tb >= 4 && tb <= 7)
        return GDK_FILTER_REMOVE;

    // On left-button release: invoke SimulateClick which runs JS to find the
    // anchor under (t_bx, t_by) and calls webkit_web_view_load_uri directly.
    if (t_is_release && tb == Button1)
    {
        typedef void (*HXTSimFn)(void*, int, int);
        HXTSimFn fn = (HXTSimFn)g_object_get_data(
            G_OBJECT(t_layer->m_browser_widget), "hxt-sim-fn");
        void *ctx = g_object_get_data(
            G_OBJECT(t_layer->m_browser_widget), "hxt-sim-ctx");
        fprintf(stderr, "[HXT] SimulateClick bridge fn=%p ctx=%p bx=%d by=%d\n",
            (void*)fn, ctx, t_bx, t_by);
        if (fn && ctx)
            fn(ctx, t_bx, t_by);
    }

    return GDK_FILTER_CONTINUE; // let HXT process it too
}

////////////////////////////////////////////////////////////////////////////////
// Attach / Detach

void MCNativeLayerX11::doAttach()
{
    if (m_child_window == NULL)
    {
        MCRectangle t_rect = m_object->getrect();

        // Create a GtkOffscreenWindow to host the browser widget.
        //
        // GtkOffscreenWindow renders its children into an off-screen surface
        // (no visible X11 window).  GTK synthesizes GDK events dispatched to
        // it via gtk_main_do_event() and routes them to children by coordinate,
        // preserving all internal WebKit state (focus, hit-test context, pointer
        // tracking) that direct sub-widget targeting bypasses.
        //
        // Browser content is composited into HXT's MCGContext by doPaint() via
        // gtk_widget_draw(), so nothing appears on-screen as a separate window.
        m_child_window = GTK_WINDOW(gtk_offscreen_window_new());

        if (m_browser_widget != NULL)
        {
            gtk_container_add(GTK_CONTAINER(m_child_window), m_browser_widget);
        }

        // Size the offscreen window to the widget rect.  No intersection with
        // the viewport is needed here — HXT's MCGContext clips the rendered
        // content to the visible area in doPaint().
        gtk_widget_set_size_request(GTK_WIDGET(m_child_window),
                                    MAX(1, t_rect.width),
                                    MAX(1, t_rect.height));

        // Realize creates the offscreen GdkWindow.
        gtk_widget_realize(GTK_WIDGET(m_child_window));

        // Show the offscreen container first.
        gtk_widget_show(GTK_WIDGET(m_child_window));

        if (m_browser_widget != NULL)
        {
            // Show the browser widget via a GLib idle to avoid forking inside
            // an active pointer grab (e.g. during DnD palette drag).
            g_idle_add([](gpointer data) -> gboolean {
                gtk_widget_show(GTK_WIDGET(data));
                return G_SOURCE_REMOVE;
            }, m_browser_widget);
        }

        // "damage-event" fires when WebKit has new content in the offscreen
        // surface.  We use it to schedule an HXT repaint.
        m_damage_signal_id = g_signal_connect(m_child_window, "damage-event",
                                               G_CALLBACK(onDamage), this);

        // Event filter on the stack window: makes the (invisible) browser
        // respond to pointer and keyboard input.
        {
            GdkWindow *t_sw = getStackGdkWindow();
            fprintf(stderr, "[HXT] doAttach: installing filter on stack_win=%p child_win=%p\n",
                (void*)t_sw, (void*)m_child_window);
            gdk_window_add_filter(t_sw, onStackWindowFilter, this);
        }
    }

    // 60 Hz repaint timer — drives continuous HXT Redraw() calls so WebKit
    // animations, auto-refreshing pages, and async tile deliveries always
    // appear within one frame.  Started here (not inside the m_child_window==NULL
    // guard) so it also runs on re-attach.
    if (m_paint_timer_id == 0)
    {
        m_paint_timer_id = g_timeout_add(16, [](gpointer data) -> gboolean {
            MCNativeLayerX11 *t_layer = static_cast<MCNativeLayerX11*>(data);
            if (t_layer->m_visible && t_layer->m_show_for_tool &&
                t_layer->m_object != NULL)
                t_layer->m_object->Redraw();
            return G_SOURCE_CONTINUE;
        }, this);
    }

    // Size / visibility sync (runs on both first-attach and re-attach).
    doSetViewportGeometry(m_viewport_rect);
    doSetGeometry(m_rect);
    doSetVisible(ShouldShowLayer());
}

void MCNativeLayerX11::doDetach()
{
    // Stop the 60 Hz repaint timer.
    if (m_paint_timer_id != 0)
    {
        g_source_remove(m_paint_timer_id);
        m_paint_timer_id = 0;
    }

    // Remove the stack event filter and the damage signal.
    gdk_window_remove_filter(getStackGdkWindow(), onStackWindowFilter, this);
    m_browser_focused = false;
    if (s_focused_browser_layer == this)
        s_focused_browser_layer = NULL;

    if (m_damage_signal_id != 0 && m_child_window != NULL)
    {
        g_signal_handler_disconnect(m_child_window, m_damage_signal_id);
        m_damage_signal_id = 0;
    }

    // Hide the offscreen window so WebKit stops rendering (saves CPU/RAM).
    // The window and widget hierarchy are preserved for a subsequent re-attach.
    if (m_child_window != NULL)
        gtk_widget_hide(GTK_WIDGET(m_child_window));
}

////////////////////////////////////////////////////////////////////////////////
// Paint — composite offscreen surface into HXT's context

// doPaint() is the heart of the offscreen approach.
//
// We ask WebKit to draw directly into a temporary cairo_image_surface via
// gtk_widget_draw(m_browser_widget, t_cr).  With software compositing enabled
// (WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER), WebKit's draw signal handler
// copies its software-rendered tile cache into whatever cairo_t it is given.
// This is more reliable than reading back gtk_offscreen_window_get_surface()
// (which returns an XLIB surface on X11/XWayland and thus has no directly
// accessible pixel data) or gtk_offscreen_window_get_pixbuf() (which requires
// the GTK draw cycle to have already run and populated the offscreen surface).
//
// The widget is shown via the GtkOffscreenWindow so it is realized and mapped
// (required for WebKit to render).  The GtkOffscreenWindow remains invisible —
// we never read its backing surface.  It exists solely to give WebKit a GDK
// window context and a draw cycle.
//
// Returns false in edit mode (m_show_for_tool == false) so the HXT widget
// framework falls back to the LCB widget's own OnPaint handler, which draws
// the globe placeholder via paintPlaceholderImage.
bool MCNativeLayerX11::doPaint(MCGContextRef p_context)
{
    // In edit mode let the LCB widget's OnPaint draw the globe placeholder.
    if (!m_show_for_tool || m_child_window == NULL || m_browser_widget == NULL)
        return false;

    int t_w = m_rect.width;
    int t_h = m_rect.height;
    if (t_w <= 0 || t_h <= 0)
        return false;

    // Allocate a software-backed ARGB32 image surface.
    cairo_surface_t *t_surf =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, t_w, t_h);
    if (cairo_surface_status(t_surf) != CAIRO_STATUS_SUCCESS)
    {
        cairo_surface_destroy(t_surf);
        return false;
    }

    // Paint the WebKit widget into the surface.
    //
    // • Fill white first so that the "loading" state looks like a blank browser
    //   page rather than transparent / garbage pixels.
    // • gtk_widget_draw() fires the "draw" signal on WebKitWebView; in
    //   WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER mode the draw handler blits
    //   WebKit's software-rendered tiles into our cairo_t.
    cairo_t *t_cr = cairo_create(t_surf);
    cairo_rectangle(t_cr, 0, 0, t_w, t_h);
    cairo_clip(t_cr);
    cairo_set_source_rgb(t_cr, 1.0, 1.0, 1.0);
    cairo_paint(t_cr);
    gtk_widget_draw(m_browser_widget, t_cr);
    cairo_destroy(t_cr);
    cairo_surface_flush(t_surf);

    unsigned char *t_data   = cairo_image_surface_get_data(t_surf);
    int            t_stride = cairo_image_surface_get_stride(t_surf);

    // MCGImageCreateWithRasterNoCopy does NOT take ownership of the buffer, so
    // snapshot the pixels into a separately allocated block.
    void *t_pixels = NULL;
    bool t_success = MCMemoryAllocate((size_t)t_h * t_stride, t_pixels);
    if (t_success)
    {
        memcpy(t_pixels, t_data, (size_t)t_h * t_stride);

        MCGRaster t_raster;
        t_raster.format = kMCGRasterFormat_ARGB;
        t_raster.width  = (uint32_t)t_w;
        t_raster.height = (uint32_t)t_h;
        t_raster.stride = (uint32_t)t_stride;
        t_raster.pixels = t_pixels;

        MCGImageRef t_image = NULL;
        t_success = MCGImageCreateWithRasterNoCopy(t_raster, t_image);
        if (t_success)
        {
            MCGRectangle t_r;
            t_r.origin.x    = 0;
            t_r.origin.y    = 0;
            t_r.size.width  = MCGFloat(t_w);
            t_r.size.height = MCGFloat(t_h);
            MCGContextDrawImage(p_context, t_image, t_r, kMCGImageFilterNone);
            MCGImageRelease(t_image);
        }
        MCMemoryDeallocate(t_pixels);
    }
    cairo_surface_destroy(t_surf);
    return t_success;
}

////////////////////////////////////////////////////////////////////////////////
// Geometry

void MCNativeLayerX11::updateContainerGeometry()
{
    m_intersect_rect = MCU_intersect_rect(m_viewport_rect, m_rect);

    if (m_child_window == NULL)
        return;

    // Resize the offscreen window to the full widget rect.  HXT's MCGContext
    // clips the rendered pixels to the visible viewport; we don't need to do
    // it here.
    if (m_rect.width > 0 && m_rect.height > 0)
    {
        gtk_widget_set_size_request(GTK_WIDGET(m_child_window),
                                    m_rect.width, m_rect.height);
        gtk_window_resize(GTK_WINDOW(m_child_window),
                          m_rect.width, m_rect.height);
    }
}

void MCNativeLayerX11::doSetViewportGeometry(const MCRectangle &p_rect)
{
    m_viewport_rect = p_rect;
    updateContainerGeometry();
}

void MCNativeLayerX11::doSetGeometry(const MCRectangle &p_rect)
{
    m_rect = p_rect;
    updateContainerGeometry();

    if (m_child_window == NULL || m_browser_widget == NULL)
        return;

    // Position the browser widget at (0,0) within the offscreen window and
    // give it the full widget dimensions.  No viewport-intersection offset is
    // needed because HXT clips through MCGContext, not by window clipping.
    if (m_rect.width > 0 && m_rect.height > 0)
    {
        gtk_widget_set_size_request(m_browser_widget, m_rect.width, m_rect.height);

        GtkAllocation t_alloc;
        t_alloc.x      = 0;
        t_alloc.y      = 0;
        t_alloc.width  = m_rect.width;
        t_alloc.height = m_rect.height;
        gtk_widget_size_allocate(m_browser_widget, &t_alloc);
    }

    // Store the widget's absolute screen position so libbrowser's
    // show-option-menu handler can position the <select> popup correctly.
    // GtkOffscreenWindow has no real screen origin, so we compute the
    // absolute position from the stack window's screen origin + m_rect offset.
    {
        GdkWindow *t_stack_win = getStackGdkWindow();
        int t_origin_x = 0, t_origin_y = 0;
        if (t_stack_win)
            gdk_window_get_origin(t_stack_win, &t_origin_x, &t_origin_y);
        g_object_set_data(G_OBJECT(m_browser_widget), "hxt-screen-x",
            GINT_TO_POINTER(t_origin_x + m_rect.x));
        g_object_set_data(G_OBJECT(m_browser_widget), "hxt-screen-y",
            GINT_TO_POINTER(t_origin_y + m_rect.y));
        g_object_set_data(G_OBJECT(m_browser_widget), "hxt-stack-win",
            (gpointer)t_stack_win);
    }
}

void MCNativeLayerX11::doSetVisible(bool p_visible)
{
    if (m_child_window == NULL)
        return;

    // Show/hide the offscreen window so WebKit renders only when needed.
    if (p_visible)
        gtk_widget_show(GTK_WIDGET(m_child_window));
    else
        gtk_widget_hide(GTK_WIDGET(m_child_window));

    if (p_visible)
        doSetGeometry(m_object->getrect());
}

void MCNativeLayerX11::doRelayer()
{
    // Nothing to do for offscreen rendering.
    // There is no X11 window to reorder.  Draw order between multiple browser
    // widgets on the same card is determined by HXT's object layer ordering,
    // which drives the sequence of doPaint() calls.
}

////////////////////////////////////////////////////////////////////////////////

bool MCNativeLayerX11::GetNativeView(void *&r_view)
{
    r_view = (void*)m_browser_widget;
    return true;
}

////////////////////////////////////////////////////////////////////////////////

GdkWindow* MCNativeLayerX11::getStackGdkWindow()
{
    return m_object->getstack()->getwindow();
}

////////////////////////////////////////////////////////////////////////////////

MCNativeLayer* MCNativeLayer::CreateNativeLayer(MCObject *p_object, void *p_native_view)
{
    return new MCNativeLayerX11(p_object, GTK_WIDGET(p_native_view));
}

bool MCNativeLayer::CreateNativeContainer(MCObject *p_object, void *&r_view)
{
    return false;
}

void MCNativeLayer::ReleaseNativeView(void *p_view)
{
}

////////////////////////////////////////////////////////////////////////////////
