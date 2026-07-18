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
    virtual void OnMouseDown(int p_x, int p_y);
    virtual void OnMouseUp(int p_x, int p_y);

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

    // 60 Hz repaint timer: drives continuous HXT Redraw() calls so WebKit
    // animations, auto-refreshing pages, and async tile deliveries always
    // show up within one frame.  doPaint() uses gtk_widget_draw() which
    // composites WebKit's current tile buffer synchronously each call.
    guint m_paint_timer_id;

    // Whether the browser currently has logical focus for keyboard forwarding.
    // Set on button-press inside m_rect; cleared on button-press outside.
    bool m_browser_focused;

    // True while a mouse button is held down inside the browser rect.
    // Used to keep forwarding MotionNotify events to WebKit during a drag
    // even when the pointer strays outside m_rect (for text-selection
    // extension to the edge of the browser area).
    bool m_pointer_button_down;

    // True when a button-press has been recorded (OnMouseDown) but not yet
    // forwarded to WebKit.  The press is sent lazily on the first motion
    // event so that quick clicks never trigger a synchronous show-option-menu
    // call (which deadlocks via gtk_main() while the X11 implicit grab is
    // still active).  For drags the press always arrives at WebKit before
    // the first motion event.
    bool m_button_press_pending;

    // Browser-relative coords of the deferred button-press.
    int m_pending_press_bx;
    int m_pending_press_by;

    // Fired by GtkOffscreenWindow when WebKit has new content.  Schedules an
    // HXT Redraw() via a GLib idle so we don't re-enter the draw pipeline.
    static gboolean onDamage(GtkWidget *widget, GdkEvent *event,
                             gpointer user_data);

    // Idle callback: calls m_object->Redraw() and clears m_redraw_pending.
    static gboolean onRedrawIdle(gpointer user_data);

    // Snapshot-ready bridge: called by libbrowser's SnapshotDone() (via
    // "hxt-repaint-fn" g_object_data) to schedule a repaint after a new
    // webkit_web_view_get_snapshot() result has been stored.
    static void TriggerRedraw(void *ctx);

    // GDK event filter on the stack window.  Intercepts pointer and keyboard
    // events that fall within m_rect and forwards them to the offscreen browser
    // via gtk_main_do_event(), keeping the browser interactive.
    static GdkFilterReturn onStackWindowFilter(GdkXEvent *xevent,
                                               GdkEvent  *event,
                                               gpointer   user_data);

    // Module-global keyboard focus: whichever X11 layer last received
    // OnMouseDown.  Set here so the free functions below can access it.
    static MCNativeLayerX11 *s_focused_browser_layer;

    // Helper called by hxt_browser_key_down/up (friend free functions defined
    // in native-layer-x11.cpp).
    void dispatchKeyEvent(GdkEventType p_type, unsigned int p_keyval,
                          unsigned int p_state, unsigned short p_hwcode,
                          unsigned char p_group);

    // Free functions in native-layer-x11.cpp that receive raw GDK key data
    // from lnxdclnx.cpp and forward it to the focused browser widget.
    friend bool hxt_browser_has_focus();
    friend void hxt_browser_clear_focus();
    friend void hxt_browser_key_down(unsigned int, unsigned int,
                                     unsigned short, unsigned char);
    friend void hxt_browser_key_up(unsigned int, unsigned int,
                                   unsigned short, unsigned char);

    // Called from lnxdclnx.cpp's GDK_MOTION_NOTIFY handler to forward drag
    // motion to WebKit so text selection can be extended while the pointer
    // moves across the browser rect with a button held down.
    friend void hxt_browser_forward_motion(int, int);

    // Iterates all active native layers and returns the browser widget whose
    // rect contains (p_x, p_y) in stack-window coordinates.  Used by
    // lnxdclnx.cpp's scroll routing to support multiple browser widgets.
    friend bool hxt_find_browser_at(int p_x, int p_y,
                                    GtkWidget **r_widget,
                                    int *r_bx, int *r_by);

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

    // Synthesises a GdkEvent of the given pointer type (GDK_BUTTON_PRESS,
    // GDK_BUTTON_RELEASE, or GDK_MOTION_NOTIFY) and delivers it to the
    // GtkOffscreenWindow via gtk_main_do_event().  Coordinates p_bx/p_by are
    // relative to the browser widget (0,0 = top-left of the browser rect).
    // GTK routes the event to the correct child widget (WebKitWebView) and
    // maintains its internal implicit-grab state so that motion events during
    // a button drag are routed to WebKit for text-selection extension.
    void forwardPointerEvent(GdkEventType p_type, int p_bx, int p_by,
                             guint p_button, guint p_state);
};

#endif // ifndef __MC_NATIVE_LAYER_X11__
