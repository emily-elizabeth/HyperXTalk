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
#include <vector>     // s_all_layers_list
#include <algorithm>  // std::remove


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

// All currently attached native layers.  Used by hxt_find_browser_at() to
// route scroll events by mouse position when multiple browsers are present.
static std::vector<MCNativeLayerX11*> s_all_layers_list;

// Defined in lnxdclnx.cpp — closes any active HXT text field so that
// subsequent key events route to WebKit instead of the field.
// Called from OnMouseDown when the browser widget receives a click.
extern void hxt_browser_took_focus();

// Tracks the keyval of the key currently held down by the browser widget.
// 0 = no key currently pressed.
//
// WebKit2GTK (IPC architecture) asynchronously processes key events: after
// we emit GDK_KEY_PRESS via g_signal_emit_by_name(), the web process handles
// it and the UI process injects a follow-up synthetic GDK_KEY_PRESS (~28 ms
// later) via gdk_event_put() with send_event=FALSE.  This lands on the HXT
// stack window and re-enters our key dispatch, causing Tab to jump two DOM
// elements instead of one.  By tracking "key is down", any PRESS for a keyval
// already in flight is immediately discarded as a duplicate.
//
// X11 auto-repeat generates RELEASE+PRESS pairs (not bare PRESS), so the
// RELEASE clears the flag before the repeat PRESS arrives — correct repeat
// behaviour is preserved.
static unsigned int s_browser_key_down_keyval = 0;

// Set in OnMouseDown, reset by hxt_browser_reset_mousedown_flag() before each
// wmdown() call in lnxdclnx.cpp.  Lets the post-wmdown check distinguish a
// click on the browser widget (OnMouseDown fired → don't clear focus) from a
// click on an HXT field (OnMouseDown did NOT fire → clear browser focus).
static bool s_browser_mousedown_fired = false;

void hxt_browser_reset_mousedown_flag() { s_browser_mousedown_fired = false; }
bool hxt_browser_mousedown_fired()      { return s_browser_mousedown_fired; }

// Called from lnxdclnx.cpp's GDK_KEY_PRESS/RELEASE handler immediately after
// the normal HXT wkdown/wkup dispatch.  Forwards the raw GDK key event to the
// focused offscreen WebKitWebView by synthesising a new GdkEventKey and
// dispatching it via gtk_main_do_event() to the GtkOffscreenWindow.  GTK
// routes it to the internally focused child (the WebKitWebView).
bool hxt_browser_has_focus()
{
    MCNativeLayerX11 *t_layer = MCNativeLayerX11::s_focused_browser_layer;
    return t_layer != NULL && t_layer->m_browser_focused;
}

// Called from lnxdclnx.cpp when a ButtonPress causes wmdown() to set
// MCactivefield — i.e. the user clicked an HXT text field.  Clears
// browser keyboard focus so subsequent key events route to HXT, not WebKit.
void hxt_browser_clear_focus()
{
    MCNativeLayerX11 *t_layer = MCNativeLayerX11::s_focused_browser_layer;
    if (t_layer == NULL)
        return;

    t_layer->m_browser_focused = false;

    // Synthesise GDK_FOCUS_CHANGE(in=FALSE) so WebKit hides its text caret.
    // Mirrors the focus-in event sent by OnMouseDown.
    GdkWindow *t_win = (t_layer->m_child_window != NULL)
        ? gtk_widget_get_window(GTK_WIDGET(t_layer->m_child_window))
        : NULL;
    if (t_win && GDK_IS_WINDOW(t_win))
    {
        GdkDisplay *t_dpy  = gdk_display_get_default();
        GdkSeat    *t_seat = gdk_display_get_default_seat(t_dpy);
        GdkDevice  *t_kbd  = gdk_seat_get_keyboard(t_seat);

        GdkEvent *t_focus = gdk_event_new(GDK_FOCUS_CHANGE);
        t_focus->focus_change.window     = t_win;
        g_object_ref(t_win);
        t_focus->focus_change.send_event = TRUE;
        t_focus->focus_change.in         = FALSE;
        if (t_kbd)
            gdk_event_set_device(t_focus, t_kbd);
        gtk_main_do_event(t_focus);
        gdk_event_free(t_focus);
    }
}

void hxt_browser_key_down(unsigned int p_keyval, unsigned int p_state,
                           unsigned short p_hwcode, unsigned char p_group)
{
    MCNativeLayerX11 *t_layer = MCNativeLayerX11::s_focused_browser_layer;
    if (t_layer == NULL || !t_layer->m_browser_focused)
        return;

    // Suppress duplicate key-press events for a key already in the down state.
    // WebKit2GTK's async IPC processing injects an extra GDK_KEY_PRESS ~28 ms
    // after the first dispatch; this guard drops it.
    if (s_browser_key_down_keyval == p_keyval)
        return;
    s_browser_key_down_keyval = p_keyval;

    t_layer->dispatchKeyEvent(GDK_KEY_PRESS, p_keyval, p_state, p_hwcode, p_group);
}

void hxt_browser_key_up(unsigned int p_keyval, unsigned int p_state,
                         unsigned short p_hwcode, unsigned char p_group)
{
    // Clear the key-down tracker so the next PRESS for this keyval
    // (whether a new press or auto-repeat) is dispatched normally.
    if (s_browser_key_down_keyval == p_keyval)
        s_browser_key_down_keyval = 0;

    // Key-release is intentionally not forwarded to WebKit.
    //
    // Some WebKit GTK builds advance Tab/Shift+Tab focus on GDK_KEY_RELEASE
    // in addition to GDK_KEY_PRESS, which would produce double-advancement.
    // Text input and Tab traversal only require GDK_KEY_PRESS delivery.
    // Modifier key state (Shift, Ctrl, Alt) is already encoded in the state
    // field of the subsequent GDK_KEY_PRESS event so WebKit sees it correctly
    // without needing an explicit modifier-release event.
    (void)p_state; (void)p_hwcode; (void)p_group;
}

// Called from lnxdclnx.cpp's GDK_MOTION_NOTIFY handler while a button is
// held inside the browser rect.  p_x/p_y are in HXT's scaled stack-window
// coordinate space (same space as m_rect).  The function converts to
// browser-widget-relative coords and forwards to WebKit via
// forwardPointerEvent so that text-selection dragging works correctly.
void hxt_browser_forward_motion(int p_x, int p_y)
{
    MCNativeLayerX11 *t_layer = MCNativeLayerX11::s_focused_browser_layer;
    if (t_layer == NULL || !t_layer->m_pointer_button_down)
        return;

    int t_bx = p_x - (int)t_layer->m_rect.x;
    int t_by = p_y - (int)t_layer->m_rect.y;

    // Send the deferred button-press before the first motion so WebKit can
    // anchor the text-selection range at the original click point.
    // At this point we know it is a drag (not a quick click), so there is no
    // risk of a synchronous show-option-menu deadlock — <select> elements are
    // only activated by clicks, not drags.
    if (t_layer->m_button_press_pending)
    {
        t_layer->m_button_press_pending = false;
        t_layer->forwardPointerEvent(GDK_BUTTON_PRESS,
            t_layer->m_pending_press_bx, t_layer->m_pending_press_by,
            1, 0);
    }

    t_layer->forwardPointerEvent(GDK_MOTION_NOTIFY, t_bx, t_by,
                                 0, GDK_BUTTON1_MASK);
}

// Searches all attached native layers for one whose rect contains (p_x, p_y)
// in stack-window coordinates.  Returns true and fills r_widget / r_bx / r_by
// if found.  Used by lnxdclnx.cpp's scroll routing so that scroll events
// reach the browser the pointer is actually over, not just the focused one.
bool hxt_find_browser_at(int p_x, int p_y,
                          GtkWidget **r_widget,
                          int *r_bx, int *r_by)
{
    for (MCNativeLayerX11 *t_layer : s_all_layers_list)
    {
        if (!t_layer->m_visible || !t_layer->m_show_for_tool ||
            t_layer->m_browser_widget == NULL)
            continue;
        const MCRectangle &r = t_layer->m_rect;
        if (p_x >= (int)r.x && p_x < (int)(r.x + r.width) &&
            p_y >= (int)r.y && p_y < (int)(r.y + r.height))
        {
            *r_widget = t_layer->m_browser_widget;
            *r_bx     = (int)r.x;
            *r_by     = (int)r.y;
            return true;
        }
    }
    return false;
}

// Synthesises a GdkEventKey and delivers it directly to the WebKitWebView
// via g_signal_emit_by_name, bypassing GtkWindow's key_press_event handler.
//
// Why not gtk_main_do_event(GtkOffscreenWindow)?
// GtkWindow::key_press_event first calls gtk_window_propagate_key_event
// (which delivers the key to WebKit — 1 TAB advance) and then may also
// run gtk_window_move_focus / gtk_window_activate_key for Tab, resulting
// in a second traversal step.  Emitting directly on the browser widget
// skips all GtkWindow wrapping and gives WebKit exactly one delivery.
void MCNativeLayerX11::dispatchKeyEvent(GdkEventType p_type,
                                         unsigned int  p_keyval,
                                         unsigned int  p_state,
                                         unsigned short p_hwcode,
                                         unsigned char  p_group)
{
    if (m_browser_widget == NULL)
        return;

    // Use the GtkOffscreenWindow's GdkWindow as the event window; WebKit reads
    // the window only to derive screen coordinates, not for dispatch.
    GdkWindow *t_win = (m_child_window != NULL)
        ? gtk_widget_get_window(GTK_WIDGET(m_child_window))
        : gtk_widget_get_window(m_browser_widget);
    if (t_win == NULL)
        return;

    GdkEvent *evt = gdk_event_new(p_type);

    // GTK3 requires every synthesised event to carry the appropriate GdkDevice
    // or it emits "Event not holding a GdkDevice" warnings.
    GdkDisplay *t_dpy = gdk_display_get_default();
    GdkSeat    *t_seat = t_dpy ? gdk_display_get_default_seat(t_dpy) : NULL;
    GdkDevice  *t_kbd  = t_seat ? gdk_seat_get_keyboard(t_seat) : NULL;
    if (t_kbd)
        gdk_event_set_device(evt, t_kbd);

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


    // Emit directly on the WebKitWebView widget — one delivery, no GtkWindow wrapping.
    const char *t_signal = (p_type == GDK_KEY_PRESS) ? "key-press-event" : "key-release-event";
    gboolean t_handled = FALSE;
    g_signal_emit_by_name(m_browser_widget, t_signal, evt, &t_handled);

    gdk_event_free(evt);
}

// Synthesises a GdkEvent of the given pointer type and delivers it to the
// GtkOffscreenWindow via gtk_main_do_event().
//
// Why gtk_main_do_event on the offscreen window?
// GTK routes the event through its normal widget dispatch to the child
// (WebKitWebView) using coordinates.  Critically, for GDK_BUTTON_PRESS,
// GTK sets up an internal implicit device-grab so subsequent
// GDK_MOTION_NOTIFY events (even from outside m_rect) are still delivered
// to WebKit — this is the mechanism that makes text-selection dragging work.
// g_signal_emit_by_name bypasses the grab machinery and so cannot extend
// a drag-selection outside the initial click point.
void MCNativeLayerX11::forwardPointerEvent(GdkEventType p_type,
                                            int p_bx, int p_by,
                                            guint p_button, guint p_state)
{
    if (m_browser_widget == NULL)
        return;

    // doPaint() allocates a physical-resolution cairo surface (scale×w × scale×h)
    // so HXT logical pixel == CSS pixel. Forward coordinates as-is; no scale division.

    // Use the browser widget's own GdkWindow (inside the GtkOffscreenWindow)
    // as the event window so WebKit's coordinate-space assumptions stay correct.
    // Fall back to the offscreen window if the browser widget isn't yet realised.
    GdkWindow *t_win = gtk_widget_get_window(m_browser_widget);
    if (t_win == NULL)
    {
        if (m_child_window == NULL ||
            !gtk_widget_get_realized(GTK_WIDGET(m_child_window)))
            return;
        t_win = gtk_widget_get_window(GTK_WIDGET(m_child_window));
        if (t_win == NULL)
            return;
    }

    GdkDisplay *t_dpy  = gdk_display_get_default();
    GdkSeat    *t_seat = gdk_display_get_default_seat(t_dpy);
    GdkDevice  *t_dev  = gdk_seat_get_pointer(t_seat);

    GdkEvent *evt = gdk_event_new(p_type);
    gdk_event_set_device(evt, t_dev);

    const char *t_signal = NULL;

    if (p_type == GDK_MOTION_NOTIFY)
    {
        evt->motion.window     = t_win;
        g_object_ref(t_win);
        evt->motion.send_event = FALSE;
        evt->motion.time       = GDK_CURRENT_TIME;
        evt->motion.x          = p_bx;
        evt->motion.y          = p_by;
        evt->motion.x_root     = 0;
        evt->motion.y_root     = 0;
        evt->motion.state      = (GdkModifierType)p_state;
        evt->motion.is_hint    = 0;
        t_signal               = "motion-notify-event";
    }
    else
    {
        // GDK_BUTTON_PRESS or GDK_BUTTON_RELEASE
        evt->button.window     = t_win;
        g_object_ref(t_win);
        evt->button.send_event = FALSE;
        evt->button.time       = GDK_CURRENT_TIME;
        evt->button.x          = p_bx;
        evt->button.y          = p_by;
        evt->button.x_root     = 0;
        evt->button.y_root     = 0;
        evt->button.state      = (GdkModifierType)p_state;
        evt->button.button     = p_button;
        t_signal = (p_type == GDK_BUTTON_PRESS) ? "button-press-event"
                                                 : "button-release-event";
    }


    // Emit directly on the WebKitWebView widget so the signal reaches WebKit's
    // own handlers regardless of GdkWindow hierarchy or GTK grab state.
    // gtk_main_do_event() was tried first but routes via gdk_window_get_user_data
    // which resolves to the GtkOffscreenWindow (not WebKit), so the event never
    // descended to the child.  g_signal_emit_by_name bypasses that lookup and
    // hits WebKit's button/motion handlers directly.  WebKit tracks its own
    // button-down state from the GDK_BUTTON1_MASK in motion event state fields,
    // so no GTK/GDK implicit grab is required for drag-selection to work.
    gboolean t_handled = FALSE;
    g_signal_emit_by_name(m_browser_widget, t_signal, evt, &t_handled);

    gdk_event_free(evt);
}

MCNativeLayerX11::MCNativeLayerX11(MCObject *p_object, GtkWidget *p_view) :
  m_child_window(NULL),
  m_browser_widget(p_view),
  m_damage_signal_id(0),
  m_redraw_pending(false),
  m_paint_timer_id(0),
  m_browser_focused(false),
  m_pointer_button_down(false),
  m_button_press_pending(false),
  m_pending_press_bx(0),
  m_pending_press_by(0)
{
    m_object = p_object;
    m_intersect_rect = MCRectangleMake(0,0,0,0);
}

MCNativeLayerX11::~MCNativeLayerX11()
{
    if (s_focused_browser_layer == this)
        s_focused_browser_layer = NULL;
    s_all_layers_list.erase(
        std::remove(s_all_layers_list.begin(), s_all_layers_list.end(), this),
        s_all_layers_list.end());
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
}

// Called by MCWidget::mdown — coordinates are widget-relative.
void MCNativeLayerX11::OnMouseDown(int p_x, int p_y)
{
    if (!m_visible || m_child_window == NULL || m_browser_widget == NULL)
        return;

    // Signal to lnxdclnx.cpp's post-wmdown check that the click landed on
    // the browser widget (not an HXT field), so it won't clear our focus.
    s_browser_mousedown_fired = true;

    // Close any active HXT text field so keyboard focus moves to the browser.
    // MCWidget's mdown does not trigger kunfocus() on fields the way clicking
    // a non-browser control would, so we do it explicitly here.
    hxt_browser_took_focus();

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
        GdkDisplay *t_fdpy  = gdk_display_get_default();
        GdkSeat    *t_fseat = gdk_display_get_default_seat(t_fdpy);
        GdkDevice  *t_fkbd  = gdk_seat_get_keyboard(t_fseat);

        GdkEvent *t_focus = gdk_event_new(GDK_FOCUS_CHANGE);
        t_focus->focus_change.window    = t_win;
        g_object_ref(t_win);
        t_focus->focus_change.send_event = TRUE;
        t_focus->focus_change.in         = TRUE;
        // Attach the keyboard device so GDK doesn't warn "not holding a GdkDevice".
        if (t_fkbd)
            gdk_event_set_device(t_focus, t_fkbd);
        gtk_main_do_event(t_focus);
        gdk_event_free(t_focus);
    }

    // Record the button-press but do NOT forward it to WebKit yet.
    //
    // If forwarded synchronously here, WebKit sometimes fires show-option-menu
    // synchronously from within g_signal_emit_by_name("button-press-event") —
    // notably on the second click on a <select> element when WebKit has cached
    // the option list.  That signal handler calls gtk_main(), which cannot
    // establish a pointer grab because the X11 implicit grab created by the
    // physical click is still active.  gtk_main() then spins forever → lockup.
    //
    // Instead the press is forwarded lazily in hxt_browser_forward_motion on
    // the first MotionNotify event:
    //  • Quick click (no drag): press is never forwarded; SimulateClick's JS
    //    .click() handles link/button/<select> activation asynchronously and
    //    correctly (button already released, no X11 grab conflict).
    //  • Drag (text selection): press is forwarded on first motion — guaranteed
    //    before any motion event reaches WebKit, so selection anchoring works.
    m_pointer_button_down = true;
    m_button_press_pending = true;
    m_pending_press_bx = p_x;
    m_pending_press_by = p_y;
}

// Called by MCWidget::mup — coordinates are widget-relative.
// Forwards the button release to WebKit (finalises text selection) and then
// invokes the SimulateClick JS bridge to handle link navigation and set the
// SFNSP for subsequent Tab traversal.
void MCNativeLayerX11::OnMouseUp(int p_x, int p_y)
{
    if (!m_visible || m_child_window == NULL || m_browser_widget == NULL)
        return;

    if (m_pointer_button_down)
    {
        m_pointer_button_down = false;

        if (m_button_press_pending)
        {
            // Quick click (no drag): the button is now physically released, so
            // the X11 implicit grab on the HXT stack window is already gone.
            // It is safe to forward the button-press now — if WebKit fires
            // show-option-menu synchronously (e.g. cached <select> on second
            // click), gtk_menu_popup_at_rect can establish its own grab and
            // gtk_main() will run correctly without deadlocking.
            m_button_press_pending = false;
            forwardPointerEvent(GDK_BUTTON_PRESS,
                m_pending_press_bx, m_pending_press_by, 1, 0);
            // Send release so WebKit finalises any click/focus state.
            forwardPointerEvent(GDK_BUTTON_RELEASE, p_x, p_y, 1, GDK_BUTTON1_MASK);
        }
        else
        {
            // Drag: button-press was already forwarded on the first motion.
            // Forward the release so WebKit finalises the text selection.
            forwardPointerEvent(GDK_BUTTON_RELEASE, p_x, p_y, 1, GDK_BUTTON1_MASK);
        }
    }

    // SimulateClick: run JS to find the element at (p_x, p_y), call .click()
    // on it for link/button activation, and set the SFNSP for Tab navigation.
    typedef void (*HXTSimFn)(void*, int, int);
    HXTSimFn fn = (HXTSimFn)g_object_get_data(
        G_OBJECT(m_browser_widget), "hxt-sim-fn");
    void *ctx = g_object_get_data(
        G_OBJECT(m_browser_widget), "hxt-sim-ctx");
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

// static
// Called by libbrowser's SnapshotDone() via the "hxt-repaint-fn" bridge after
// a fresh webkit_web_view_get_snapshot() surface has been stored.  Schedules a
// Redraw() exactly as onDamage() would, so doPaint() runs and blits the snapshot.
void MCNativeLayerX11::TriggerRedraw(void *ctx)
{
    MCNativeLayerX11 *t_layer = static_cast<MCNativeLayerX11*>(ctx);
    if (!t_layer->m_redraw_pending && t_layer->m_object != NULL)
    {
        t_layer->m_redraw_pending = true;
        g_idle_add(onRedrawIdle, t_layer);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Event forwarding — stack window → offscreen browser

// static
// GDK event filter installed on the stack's GdkWindow while the browser is
// attached.  Pointer events are now forwarded to WebKit from three dedicated
// call sites instead:
//   • OnMouseDown  → forwardPointerEvent(GDK_BUTTON_PRESS)
//   • OnMouseUp    → forwardPointerEvent(GDK_BUTTON_RELEASE)
//   • lnxdclnx.cpp GDK_MOTION_NOTIFY handler → hxt_browser_forward_motion()
// This filter is therefore a no-op for all event types.  It remains installed
// in case future requirements need the raw-XEvent path.
GdkFilterReturn MCNativeLayerX11::onStackWindowFilter(GdkXEvent * /*p_xevent*/,
                                                       GdkEvent   * /*event*/,
                                                       gpointer    /*user_data*/)
{
    return GDK_FILTER_CONTINUE;
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

            // Register the snapshot-ready repaint bridge so that libbrowser's
            // SnapshotDone() can schedule a Redraw() after storing a snapshot.
            g_object_set_data(G_OBJECT(m_browser_widget), "hxt-repaint-fn",
                (gpointer)(void(*)(void*))&MCNativeLayerX11::TriggerRedraw);
            g_object_set_data(G_OBJECT(m_browser_widget), "hxt-repaint-ctx",
                (gpointer)this);
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

    // Register this layer in the global list so hxt_find_browser_at() can
    // route scroll events to whichever browser the mouse is over.
    if (std::find(s_all_layers_list.begin(), s_all_layers_list.end(), this)
            == s_all_layers_list.end())
        s_all_layers_list.push_back(this);

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

    // Unregister from the global layer list.
    s_all_layers_list.erase(
        std::remove(s_all_layers_list.begin(), s_all_layers_list.end(), this),
        s_all_layers_list.end());

    // Clear the scroll-redirect data stored on the stack window so that a
    // stale widget pointer is never dereferenced after detach.
    {
        GdkWindow *t_sw = getStackGdkWindow();
        if (t_sw != NULL)
            g_object_set_data(G_OBJECT(t_sw), "hxt-scr-widget", NULL);
    }

    // Remove the stack event filter and the damage signal.
    gdk_window_remove_filter(getStackGdkWindow(), onStackWindowFilter, this);
    m_browser_focused = false;
    m_pointer_button_down = false;
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

    // On HiDPI displays (GDK_SCALE > 1), gtk_widget_draw() asks WebKit to render
    // at gdk_window_get_scale_factor() × the widget's logical size.  With a 1×
    // cairo surface this clips the CSS viewport to 1/scale of the page width.
    // Fix: allocate a physical-resolution surface (t_w*scale × t_h*scale) and
    // set its device_scale so cairo user coordinates stay in logical pixels.
    // gtk_widget_draw then fills the full physical surface correctly.
    // MCGContextDrawImage scales the result back down to m_rect dimensions.
    GdkDisplay *t_dpy2 = gdk_display_get_default();
    GdkMonitor *t_mon2 = t_dpy2 ? gdk_display_get_primary_monitor(t_dpy2) : NULL;
    int t_scale = (t_mon2 ? gdk_monitor_get_scale_factor(t_mon2) : 1);
    if (t_scale < 1) t_scale = 1;

    int t_pw = t_w * t_scale;   // physical pixel width
    int t_ph = t_h * t_scale;   // physical pixel height

    cairo_surface_t *t_surf =
        cairo_image_surface_create(CAIRO_FORMAT_ARGB32, t_pw, t_ph);
    if (cairo_surface_status(t_surf) != CAIRO_STATUS_SUCCESS)
    {
        cairo_surface_destroy(t_surf);
        return false;
    }
    // Tell cairo the surface is at t_scale device pixels per user (logical) unit.
    // This keeps the fill/clip coordinates in logical pixels while the backing
    // store captures physical pixels.
    cairo_surface_set_device_scale(t_surf, (double)t_scale, (double)t_scale);

    // Paint the WebKit widget into the surface.
    //
    // • Fill white first so that the "loading" state looks like a blank browser
    //   page rather than transparent / garbage pixels.
    // • gtk_widget_draw() fires the "draw" signal on WebKitWebView; in
    //   WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER mode the draw handler blits
    //   WebKit's software-rendered tiles into our cairo_t.  With device_scale
    //   set, WebKit renders the full CSS viewport at physical resolution.
    cairo_t *t_cr = cairo_create(t_surf);
    cairo_rectangle(t_cr, 0, 0, t_w, t_h);  // logical coords → physical t_pw×t_ph
    cairo_clip(t_cr);
    cairo_set_source_rgb(t_cr, 1.0, 1.0, 1.0);
    cairo_paint(t_cr);
    gtk_widget_draw(m_browser_widget, t_cr);
    cairo_destroy(t_cr);
    cairo_surface_flush(t_surf);

    // If gtk_widget_draw() produced only the white fill (blank), try the
    // snapshot fallback for WebKit 2.44+ (Fedora) where the compositor
    // bypasses Cairo and the draw signal handler is a no-op.
    {
        unsigned char *d = cairo_image_surface_get_data(t_surf);
        // Detect blank: all-white (GTK background paint, no WebKit content) or
        // all-zero (transparent black — what we get on WebKit 2.44+ where
        // gtk_widget_draw() is a compositor no-op and leaves the surface untouched).
        bool t_blank = !d ||
                       (d[0] == 0xFF && d[1] == 0xFF && d[2] == 0xFF) ||
                       (d[0] == 0x00 && d[1] == 0x00 && d[2] == 0x00);
        if (t_blank)
        {
            // Use the most recent snapshot if one is ready.
            cairo_surface_t *t_snap = (cairo_surface_t*)g_object_get_data(
                G_OBJECT(m_browser_widget), "hxt-snapshot");
            if (t_snap)
            {
                cairo_t *t_cr2 = cairo_create(t_surf);
                cairo_set_source_surface(t_cr2, t_snap, 0, 0);
                cairo_paint(t_cr2);
                cairo_destroy(t_cr2);
            }
            // Request a fresh snapshot for the next frame.
            typedef void (*HXTSnapFn)(void*);
            HXTSnapFn snap_fn = (HXTSnapFn)g_object_get_data(
                G_OBJECT(m_browser_widget), "hxt-snap-fn");
            void *snap_ctx = g_object_get_data(
                G_OBJECT(m_browser_widget), "hxt-snap-ctx");
            if (snap_fn && snap_ctx)
                snap_fn(snap_ctx);
        }
    }

    unsigned char *t_data   = cairo_image_surface_get_data(t_surf);
    int            t_stride = cairo_image_surface_get_stride(t_surf);

    // MCGImageCreateWithRasterNoCopy does NOT take ownership of the buffer, so
    // snapshot the pixels into a separately allocated block.
    void *t_pixels = NULL;
    bool t_success = MCMemoryAllocate((size_t)t_ph * t_stride, t_pixels);
    if (t_success)
    {
        memcpy(t_pixels, t_data, (size_t)t_ph * t_stride);

        // Cairo ARGB32 on little-endian stores pixels as BGRA bytes
        // (the 32-bit int 0xAARRGGBB laid out in memory as B,G,R,A).
        // HXT's MCG/Skia layer expects RGBA (R=byte0, G=byte1, B=byte2, A=byte3)
        // because its backing GdkPixbuf is always RGBA.
        // Swap byte0 (B) ↔ byte2 (R) for every pixel.
        {
            uint8_t *p   = static_cast<uint8_t*>(t_pixels);
            uint8_t *end = p + (size_t)t_ph * t_stride;
            for (; p < end; p += 4)
            {
                uint8_t b = p[0];
                p[0] = p[2];
                p[2] = b;
            }
        }

        // Raster is at physical resolution (t_pw × t_ph).
        // MCGContextDrawImage scales it into the logical m_rect (t_w × t_h).
        MCGRaster t_raster;
        t_raster.format = kMCGRasterFormat_ARGB;
        t_raster.width  = (uint32_t)t_pw;
        t_raster.height = (uint32_t)t_ph;
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
        if (t_stack_win && GDK_IS_WINDOW(t_stack_win))
            gdk_window_get_origin(t_stack_win, &t_origin_x, &t_origin_y);
        g_object_set_data(G_OBJECT(m_browser_widget), "hxt-screen-x",
            GINT_TO_POINTER(t_origin_x + m_rect.x));
        g_object_set_data(G_OBJECT(m_browser_widget), "hxt-screen-y",
            GINT_TO_POINTER(t_origin_y + m_rect.y));
        g_object_set_data(G_OBJECT(m_browser_widget), "hxt-stack-win",
            (gpointer)t_stack_win);
    }

    // Store browser widget + rect on the stack GdkWindow so EnqueueGdkEvents
    // in lnxdclnx.cpp can redirect scroll events to the browser widget when
    // the mouse is within the browser's rect (the browser lives in an offscreen
    // GtkOffscreenWindow and never receives events from the stack window directly).
    {
        GdkWindow *t_sw = getStackGdkWindow();
        if (t_sw != NULL && m_browser_widget != NULL)
        {
            g_object_set_data(G_OBJECT(t_sw), "hxt-scr-widget",
                (gpointer)m_browser_widget);
            g_object_set_data(G_OBJECT(t_sw), "hxt-scr-x",
                GINT_TO_POINTER(m_rect.x));
            g_object_set_data(G_OBJECT(t_sw), "hxt-scr-y",
                GINT_TO_POINTER(m_rect.y));
            g_object_set_data(G_OBJECT(t_sw), "hxt-scr-w",
                GINT_TO_POINTER(m_rect.width));
            g_object_set_data(G_OBJECT(t_sw), "hxt-scr-h",
                GINT_TO_POINTER(m_rect.height));
        }
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
