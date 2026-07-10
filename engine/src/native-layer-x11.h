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

#ifndef __MC_NATIVE_LAYER_X11__
#define __MC_NATIVE_LAYER_X11__

#include "native-layer.h"

#include <gtk/gtk.h>

namespace x11
{
#include <gdk/gdkx.h>
}

// -- tperry 12-11-2025: GTK3 requires explicit include for GtkSocket
// Must be after x11 namespace to avoid conflicts
#include <gtk/gtkx.h>

class MCNativeLayerX11 : public MCNativeLayer
{
public:
    virtual void OnToolChanged(Tool p_new_tool);

    // Returns true: with offscreen rendering we can composite the browser
    // content directly into HXT's paint context via doPaint().
    virtual bool GetCanRenderToContext();

    virtual bool GetNativeView(void *&r_view);

    MCNativeLayerX11(MCObject *p_object, GtkWidget *p_view);
    ~MCNativeLayerX11();

private:

    // Offscreen GTK window that hosts m_browser_widget.  Has no X11 window /
    // compositor surface: it never appears on screen and has no Z-order
    // interaction with any other window (palette, stack, etc.).
    // This eliminates the browser-above-palette Z-order problem permanently.
    GtkWindow* m_child_window;

    // The native browser widget (WebKitWebView) added as a GTK child of
    // m_child_window.  WebKit renders its content into m_child_window's
    // internal cairo_surface_t via software compositing (hardware acceleration
    // disabled in libbrowser_webkitgtk.cpp so Cairo is always used).
    GtkWidget* m_browser_widget;

    // Cached intersection rect (informational; HXT's rendering pipeline handles
    // the actual viewport clipping through the MCGContext passed to doPaint()).
    MCRectangle m_intersect_rect;

    // GLib signal ID for "damage-event" on m_child_window.  Fires when WebKit
    // has composited new content into the offscreen surface; used to schedule
    // an HXT repaint of the widget area.
    gulong m_damage_signal_id;

    // Prevents queuing multiple idle redraws when damage events arrive faster
    // than HXT's redraw cycle.
    bool m_redraw_pending;

    // Whether the browser currently has logical focus for keyboard forwarding.
    // Set on button-press inside m_rect; cleared on button-press outside.
    bool m_browser_focused;

    // Fired by GtkOffscreenWindow when WebKit has new content.  Schedules an
    // HXT Redraw() via a GLib idle so we don't re-enter the draw pipeline.
    static gboolean onDamage(GtkWidget *widget, GdkEvent *event,
                             gpointer user_data);

    // Idle callback: calls m_object->Redraw() and clears m_redraw_pending.
    static gboolean onRedrawIdle(gpointer user_data);

    // GDK event filter on the stack window.  Intercepts pointer and keyboard
    // events that fall within m_rect and forwards them to the offscreen browser
    // via gtk_main_do_event(), keeping the browser interactive.
    static GdkFilterReturn onStackWindowFilter(GdkXEvent *xevent,
                                               GdkEvent  *event,
                                               gpointer   user_data);

    // Returns the GdkWindow of the stack that owns this widget.
    GdkWindow* getStackGdkWindow();

    // Platform-specific implementations
    virtual void doAttach();
    virtual void doDetach();
    virtual bool doPaint(MCGContextRef p_context);
    virtual void doSetGeometry(const MCRectangle &p_rect);
    virtual void doSetViewportGeometry(const MCRectangle &p_rect);
    virtual void doSetVisible(bool p_visible);
    virtual void doRelayer();

    // Resizes the offscreen window to match the current m_rect dimensions.
    void updateContainerGeometry();
};

#endif // ifndef __MC_NATIVE_LAYER_X11__
