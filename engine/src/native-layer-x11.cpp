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

#include <gdk/gdk.h>
#include <gtk/gtk.h>

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
// m_child_window is now a GtkOffscreenWindow.  It has no X11 window and no
// Wayland surface — it is invisible by definition.  WebKit renders via software
// compositing (hardware acceleration disabled in libbrowser_webkitgtk.cpp) into
// the offscreen window's internal cairo_surface_t.  doPaint() reads that
// surface and blits it into HXT's own MCGContext, so the browser content
// appears as part of the stack's normal rendering hierarchy.
//
// Z-order: since there is no separate browser window, there is no Z-order
// conflict with palette windows.  Palettes are always above the stack window
// (which now contains all browser content as pixels), and no special WM hints,
// transient chains or stacking timers are required.
//
// Interactivity: a GDK event filter installed on the stack window intercepts
// pointer and keyboard events whose coordinates fall within m_rect and forwards
// them to the offscreen WebKit widget via gtk_main_do_event().  This keeps the
// browser fully interactive for clicks, scrolling, text input, etc.
//
// Performance: software rendering is slower for JS-heavy pages and media, but
// is acceptable for the typical HXT browser use case (forms, documentation,
// simple web content).  On native X11 (non-XWayland) hardware acceleration
// could be re-enabled in future once a clean Z-order solution exists.


MCNativeLayerX11::MCNativeLayerX11(MCObject *p_object, GtkWidget *p_view) :
  m_child_window(NULL),
  m_browser_widget(p_view),
  m_damage_signal_id(0),
  m_redraw_pending(false),
  m_browser_focused(false)
{
    m_object = p_object;
    m_intersect_rect = MCRectangleMake(0,0,0,0);
}

MCNativeLayerX11::~MCNativeLayerX11()
{
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
// the offscreen browser widget via gtk_main_do_event() with coordinates
// translated to be browser-widget-relative.
GdkFilterReturn MCNativeLayerX11::onStackWindowFilter(GdkXEvent * /*xevent*/,
                                                       GdkEvent   *event,
                                                       gpointer    user_data)
{
    if (event == NULL)
        return GDK_FILTER_CONTINUE;

    MCNativeLayerX11 *t_layer = static_cast<MCNativeLayerX11*>(user_data);

    // Only forward while the browser widget is visible in run mode.
    if (!t_layer->m_visible || !t_layer->m_show_for_tool ||
        t_layer->m_child_window == NULL)
        return GDK_FILTER_CONTINUE;

    const MCRectangle &r = t_layer->m_rect;

    // --- Keyboard events ---
    // No coordinates: forward if the browser holds logical focus.
    if (event->type == GDK_KEY_PRESS || event->type == GDK_KEY_RELEASE)
    {
        if (!t_layer->m_browser_focused)
            return GDK_FILTER_CONTINUE;

        GdkWindow *t_off_win =
            gtk_widget_get_window(GTK_WIDGET(t_layer->m_child_window));
        if (t_off_win == NULL)
            return GDK_FILTER_CONTINUE;

        GdkEvent *t_copy = gdk_event_copy(event);
        // Point the event at the offscreen window so GTK routes it there.
        g_object_ref(t_off_win);
        if (t_copy->type == GDK_KEY_PRESS)
        {
            g_object_unref(t_copy->key.window);
            t_copy->key.window = t_off_win;
        }
        else
        {
            g_object_unref(t_copy->key.window);
            t_copy->key.window = t_off_win;
        }
        gtk_main_do_event(t_copy);
        gdk_event_free(t_copy);
        return GDK_FILTER_REMOVE;
    }

    // --- Pointer events: check if in browser rect ---
    gdouble ex = 0, ey = 0;
    switch (event->type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
        ex = event->button.x;
        ey = event->button.y;
        break;
    case GDK_MOTION_NOTIFY:
        ex = event->motion.x;
        ey = event->motion.y;
        break;
    case GDK_SCROLL:
        ex = event->scroll.x;
        ey = event->scroll.y;
        break;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
        ex = event->crossing.x;
        ey = event->crossing.y;
        break;
    default:
        return GDK_FILTER_CONTINUE;
    }

    bool t_in_rect = (ex >= r.x && ex < r.x + r.width &&
                      ey >= r.y && ey < r.y + r.height);

    // Track logical focus state on button press.
    if (event->type == GDK_BUTTON_PRESS)
    {
        if (t_in_rect)
        {
            t_layer->m_browser_focused = true;
            // Give WebKit GTK focus within the offscreen widget tree.
            if (t_layer->m_browser_widget != NULL)
                gtk_widget_grab_focus(t_layer->m_browser_widget);
        }
        else
        {
            t_layer->m_browser_focused = false;
        }
    }

    if (!t_in_rect)
        return GDK_FILTER_CONTINUE;

    // Translate coordinates to browser-widget-relative and forward.
    GdkWindow *t_off_win =
        gtk_widget_get_window(GTK_WIDGET(t_layer->m_child_window));
    if (t_off_win == NULL)
        return GDK_FILTER_CONTINUE;

    GdkEvent *t_copy = gdk_event_copy(event);

    switch (t_copy->type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
        g_object_ref(t_off_win);
        g_object_unref(t_copy->button.window);
        t_copy->button.window = t_off_win;
        t_copy->button.x -= r.x;
        t_copy->button.y -= r.y;
        break;
    case GDK_MOTION_NOTIFY:
        g_object_ref(t_off_win);
        g_object_unref(t_copy->motion.window);
        t_copy->motion.window = t_off_win;
        t_copy->motion.x -= r.x;
        t_copy->motion.y -= r.y;
        break;
    case GDK_SCROLL:
        g_object_ref(t_off_win);
        g_object_unref(t_copy->scroll.window);
        t_copy->scroll.window = t_off_win;
        t_copy->scroll.x -= r.x;
        t_copy->scroll.y -= r.y;
        break;
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
        g_object_ref(t_off_win);
        g_object_unref(t_copy->crossing.window);
        t_copy->crossing.window = t_off_win;
        t_copy->crossing.x -= r.x;
        t_copy->crossing.y -= r.y;
        break;
    default:
        break;
    }

    gtk_main_do_event(t_copy);
    gdk_event_free(t_copy);
    return GDK_FILTER_REMOVE; // consumed — WebKit handles it, not HXT
}

////////////////////////////////////////////////////////////////////////////////
// Attach / Detach

void MCNativeLayerX11::doAttach()
{
    if (m_child_window == NULL)
    {
        MCRectangle t_rect = m_object->getrect();

        // Create an offscreen GTK window to host the browser widget.
        //
        // GtkOffscreenWindow renders its children into an internal
        // cairo_surface_t (software rendering) and has no on-screen presence:
        // no X11 window is created, no Wayland surface exists, and it never
        // participates in the compositor's Z-order.
        //
        // This is the key architectural change that permanently solves the
        // browser-above-palette problem: because the browser is no longer a
        // separate visible window, Z-order with palette (or any other) windows
        // is simply not a concern.
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

        // Realize creates the offscreen GdkWindow (GDK-internal, no X11 XID).
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
        gdk_window_add_filter(getStackGdkWindow(), onStackWindowFilter, this);
    }

    // Size / visibility sync (runs on both first-attach and re-attach).
    doSetViewportGeometry(m_viewport_rect);
    doSetGeometry(m_rect);
    doSetVisible(ShouldShowLayer());
}

void MCNativeLayerX11::doDetach()
{
    // Remove the stack event filter and the damage signal.
    gdk_window_remove_filter(getStackGdkWindow(), onStackWindowFilter, this);
    m_browser_focused = false;

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
// WebKit has rendered its content into GtkOffscreenWindow's internal
// cairo_surface_t (an ARGB32 image surface when software compositing is used).
// We read that surface's pixel data and blit it into the MCGContext that HXT
// provides for this widget's bounding rectangle.  HXT's rendering pipeline
// clips the result to the visible card area automatically.
//
// The pixel copy (memcpy) is necessary because MCGImageCreateWithRasterNoCopy
// keeps a pointer to the buffer rather than taking ownership; the offscreen
// surface buffer may be updated by WebKit at any time on the GTK main loop,
// so we need a snapshot copy to ensure the draw is coherent.
bool MCNativeLayerX11::doPaint(MCGContextRef p_context)
{
    // In edit mode (m_show_for_tool == false) let HXT draw its own placeholder.
    if (!m_show_for_tool || m_child_window == NULL)
        return false;

    // Retrieve the offscreen cairo surface.  NULL until WebKit has drawn at
    // least once (typically one GTK main-loop iteration after show).
    cairo_surface_t *t_surface =
        gtk_offscreen_window_get_surface(GTK_OFFSCREEN_WINDOW(m_child_window));
    if (t_surface == NULL)
        return false;

    // Only image surfaces give us direct pixel access.
    if (cairo_surface_get_type(t_surface) != CAIRO_SURFACE_TYPE_IMAGE)
        return false;

    // Ensure pending GPU→CPU pixel transfers are complete.
    cairo_surface_flush(t_surface);

    int t_width  = cairo_image_surface_get_width(t_surface);
    int t_height = cairo_image_surface_get_height(t_surface);
    if (t_width <= 0 || t_height <= 0)
        return false;

    unsigned char *t_data   = cairo_image_surface_get_data(t_surface);
    int            t_stride = cairo_image_surface_get_stride(t_surface);

    // Snapshot the pixel data.
    void *t_pixels = NULL;
    if (!MCMemoryAllocate((size_t)t_height * t_stride, t_pixels))
        return false;
    memcpy(t_pixels, t_data, (size_t)t_height * t_stride);

    // Build an MCGRaster from the snapshot.
    // Cairo ARGB32 = premultiplied 32-bit ARGB in native byte order,
    // identical to kMCGRasterFormat_ARGB.
    MCGRaster t_raster;
    t_raster.format = kMCGRasterFormat_ARGB;
    t_raster.width  = (uint32_t)t_width;
    t_raster.height = (uint32_t)t_height;
    t_raster.stride = (uint32_t)t_stride;
    t_raster.pixels = t_pixels;

    MCGImageRef t_image = NULL;
    bool t_success = MCGImageCreateWithRasterNoCopy(t_raster, t_image);

    if (t_success)
    {
        MCGRectangle t_r;
        t_r.origin.x    = 0;
        t_r.origin.y    = 0;
        t_r.size.width  = MCGFloat(t_width);
        t_r.size.height = MCGFloat(t_height);
        MCGContextDrawImage(p_context, t_image, t_r, kMCGImageFilterNone);
        MCGImageRelease(t_image);
    }

    // Always free the pixel snapshot; MCGImageCreateWithRasterNoCopy does NOT
    // take ownership (see also native-layer-win32-wv2.cpp::doPaint).
    MCMemoryDeallocate(t_pixels);

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
