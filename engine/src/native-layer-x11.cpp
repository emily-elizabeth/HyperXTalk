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

#include "lnxdc.h"
#include "graphicscontext.h"
#include "graphics_util.h"

#include "native-layer-x11.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>


// Design notes
// ------------
// m_child_window is an undecorated GTK_WINDOW_TOPLEVEL whose underlying
// X11 window is positioned at absolute screen coordinates to overlap the
// widget's visible area in the engine's stack window.
//
// m_browser_widget (WebKitWebView) is added directly as a GTK child of
// m_child_window.  This keeps the full GTK widget hierarchy intact:
//
//   m_child_window (GtkWindow/toplevel, undecorated) → m_browser_widget (WebKitWebView)
//
// m_child_window's GTK frame clock drives all rendering.  When WebKit
// queues a redraw (gtk_widget_queue_draw), the frame clock fires, GTK
// calls the draw signal on m_browser_widget, and WebKit composites its
// content.
//
// Sizing: gtk_window_resize(m_child_window, w, h) triggers GTK's layout
// cascade which allocates m_browser_widget to fill the window.
//
// Z-ordering: Because m_child_window is GTK_WINDOW_TOPLEVEL (not POPUP),
// the WM/compositor manages it as an xdg_toplevel.  gdk_window_set_transient_for
// then correctly keeps it above the stack window but below other applications.
// GTK_WINDOW_POPUP (override_redirect) would make the compositor ignore all
// stacking hints, causing the popup to float above every other window.
//
// XWayland scroll events: We do NOT reparent m_child_window into the stack
// window's X11 hierarchy.  XWayland only assigns Wayland surfaces to
// root-level X11 windows; a reparented child has no surface, so Wayland's
// pointer/axis routing (trackpad scroll) never reaches it.  Keeping
// m_child_window as a root-level window and positioning it at absolute
// screen coordinates solves both the scroll and z-order problems.
//
// No GtkSocket, no GtkPlug, no XEMBED — those approaches broke the GTK3
// frame clock for the embedded widget.


MCNativeLayerX11::MCNativeLayerX11(MCObject *p_object, GtkWidget *p_view) :
  m_child_window(NULL),
  m_input_shape(NULL),
  m_browser_widget(p_view),
  m_stack_gdk_window(NULL)
{
	m_object = p_object;
	m_intersect_rect = MCRectangleMake(0,0,0,0);
}

MCNativeLayerX11::~MCNativeLayerX11()
{
    if (m_stack_gdk_window != NULL)
    {
        gdk_window_remove_filter(m_stack_gdk_window, onStackWindowFilter, this);
        m_stack_gdk_window = NULL;
    }
    if (m_child_window != NULL)
    {
        gtk_widget_destroy(GTK_WIDGET(m_child_window));
    }
    if (m_input_shape != NULL)
    {
        cairo_region_destroy(m_input_shape);
    }
}

void MCNativeLayerX11::OnToolChanged(Tool p_new_tool)
{
    updateInputShape();
	MCNativeLayer::OnToolChanged(p_new_tool);
}

void MCNativeLayerX11::updateInputShape()
{
    if (m_child_window == NULL)
        return;
    if (!m_show_for_tool)
        // In edit mode. Mask out all input events
        gdk_window_input_shape_combine_region(gtk_widget_get_window(GTK_WIDGET(m_child_window)), m_input_shape, 0, 0);
    else
        // In run mode. Unset the input event mask
        gdk_window_input_shape_combine_region(gtk_widget_get_window(GTK_WIDGET(m_child_window)), NULL, 0, 0);
}

// static
GdkFilterReturn MCNativeLayerX11::onStackWindowFilter(GdkXEvent *xevent,
                                                       GdkEvent  * /*event*/,
                                                       gpointer   user_data)
{
    // Watch for X11 ConfigureNotify (window moved or resized) and keep the
    // browser popup's absolute position in sync with the stack window.
    x11::XEvent *xe = static_cast<x11::XEvent*>(xevent);
    if (xe->type == 22 /* ConfigureNotify */)
        static_cast<MCNativeLayerX11*>(user_data)->updateContainerGeometry();
    return GDK_FILTER_CONTINUE;
}

void MCNativeLayerX11::doAttach()
{
    if (m_child_window == NULL)
    {
        MCRectangle t_rect;
        t_rect = m_object->getrect();

        // Create an undecorated toplevel window to hold the browser widget.
        // GTK_WINDOW_TOPLEVEL (not POPUP) is WM/compositor-managed, so
        // WM_TRANSIENT_FOR actually controls z-ordering relative to the stack
        // window.  POPUP (override_redirect) is invisible to the compositor
        // and floats above every other application window.
        m_child_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
        gtk_window_set_decorated(m_child_window, FALSE);
        gtk_window_set_accept_focus(m_child_window, FALSE);
        gtk_window_set_skip_taskbar_hint(m_child_window, TRUE);
        gtk_window_set_skip_pager_hint(m_child_window, TRUE);

        if (m_browser_widget != NULL)
        {
            // Add the browser widget as the only GTK child.  GTK will
            // allocate it to fill the window on every layout pass.
            gtk_container_add(GTK_CONTAINER(m_child_window), m_browser_widget);
        }

        // Pre-position the window at the correct absolute screen coordinates
        // BEFORE realize/show so it never flashes at (0,0).  For unmapped
        // windows gtk_window_move() stores the position; it is applied when
        // the window is first mapped by gtk_widget_show().
        {
            gint t_sx = 0, t_sy = 0;
            gdk_window_get_origin(getStackGdkWindow(), &t_sx, &t_sy);
            gtk_window_move(m_child_window, t_sx + t_rect.x, t_sy + t_rect.y);
            gtk_window_resize(m_child_window,
                              MAX(1, t_rect.width), MAX(1, t_rect.height));
        }

        // Realize m_child_window (and its child) while it is still an
        // unparented toplevel — GTK3 requires widgets to be anchored to a
        // GtkWindow before they can be realized.
        gtk_widget_realize(GTK_WIDGET(m_child_window));

        // Tell XWayland (and any compositing WM) that this popup belongs to
        // the stack window.  Setting WM_TRANSIENT_FOR before the first map
        // causes XWayland to assign the popup a Wayland surface that is kept
        // above the stack window but below other applications — exactly the
        // z-order we need.  Without this the popup is a free-floating surface
        // that appears above every other window on screen.
        gdk_window_set_transient_for(
            gtk_widget_get_window(GTK_WIDGET(m_child_window)),
            getStackGdkWindow());

        // Do NOT reparent into the stack window.
        //
        // We previously called gdk_window_reparent() to embed the popup into
        // the stack's X11 window tree.  Under XWayland this is fatal for scroll
        // (and other pointer axis) events: XWayland gives a Wayland surface only
        // to root-level X11 windows; a reparented child has no surface of its
        // own, so Wayland's pointer/axis routing never reaches it.  WebKit2GTK
        // then never receives GDK_SCROLL events regardless of how we dispatch
        // them through GTK.
        //
        // Keeping m_child_window as a root-level override-redirect popup lets
        // XWayland treat it as a proper Wayland sub-surface.  We position it
        // manually at the correct absolute screen coordinates (see
        // updateContainerGeometry) so it visually overlaps the widget area.

        // Show the container window (maps at the pre-set position, no flash).
        // The browser widget (WebKitWebView) is shown via a GLib idle so that
        // WebKit's subprocess fork/exec happens outside any active pointer grab.
        // A grab is held throughout the DnD tools-palette drag; forking inside
        // it corrupts GDK's grab state and crashes.  The idle fires on the
        // first main-loop iteration after the DnD loop exits and the grab is
        // released.
        gtk_widget_show(GTK_WIDGET(m_child_window));

        if (m_browser_widget != NULL)
        {
            g_idle_add([](gpointer data) -> gboolean {
                gtk_widget_show(GTK_WIDGET(data));
                return G_SOURCE_REMOVE;
            }, m_browser_widget);
        }

        // Create an empty region to act as an input mask while in edit mode.
        m_input_shape = cairo_region_create();

        // Install a GDK window filter on the stack window so we receive X11
        // ConfigureNotify events and can reposition the popup when HXT moves.
        if (m_stack_gdk_window == NULL)
        {
            m_stack_gdk_window = getStackGdkWindow();
            gdk_window_add_filter(m_stack_gdk_window, onStackWindowFilter, this);
        }
    }

    // Position and size everything correctly.
    doRelayer();
    doSetViewportGeometry(m_viewport_rect);
    doSetGeometry(m_rect);
    doSetVisible(ShouldShowLayer());
}

void MCNativeLayerX11::doDetach()
{
    // Remove the GDK window filter so we stop tracking stack moves.
    if (m_stack_gdk_window != NULL)
    {
        gdk_window_remove_filter(m_stack_gdk_window, onStackWindowFilter, this);
        m_stack_gdk_window = NULL;
    }

    // Just hide the container; leave the widget hierarchy intact for re-attach.
    if (m_child_window != NULL)
        gtk_widget_hide(GTK_WIDGET(m_child_window));
}

// We can't get a snapshot of X11 windows so override this to return false
bool MCNativeLayerX11::GetCanRenderToContext()
{
	return false;
}

bool MCNativeLayerX11::doPaint(MCGContextRef p_context)
{
    return false;
}

void MCNativeLayerX11::updateContainerGeometry()
{
	m_intersect_rect = MCU_intersect_rect(m_viewport_rect, m_rect);

    if (m_child_window == NULL)
        return;

    // Clear any minimum size hint so the resize below is authoritative.
    gtk_widget_set_size_request(GTK_WIDGET(m_child_window), -1, -1);

    // m_child_window is a root-level override-redirect popup (not reparented
    // into the stack).  We position it at absolute screen coordinates by
    // adding the stack window's screen origin to the intersect rect.
    // Guard against zero size — X11 requires w > 0, h > 0.
    if (m_intersect_rect.width > 0 && m_intersect_rect.height > 0)
    {
        gint t_sx = 0, t_sy = 0;
        gdk_window_get_origin(getStackGdkWindow(), &t_sx, &t_sy);

        gdk_window_move_resize(gtk_widget_get_window(GTK_WIDGET(m_child_window)),
            t_sx + m_intersect_rect.x, t_sy + m_intersect_rect.y,
            m_intersect_rect.width,    m_intersect_rect.height);

        // gtk_window_resize tells GTK the new logical size so the layout
        // cascade allocates m_browser_widget to fill the window.
        gtk_window_resize(GTK_WINDOW(m_child_window),
            m_intersect_rect.width, m_intersect_rect.height);
    }

    gtk_widget_set_size_request(GTK_WIDGET(m_child_window),
        m_intersect_rect.width, m_intersect_rect.height);
}

void MCNativeLayerX11::doSetViewportGeometry(const MCRectangle &p_rect)
{
	m_viewport_rect = p_rect;
	updateContainerGeometry();
}

// IM-2016-01-21: [[ NativeLayer ]] Place the widget window relative to its
//    container, so only the visible area (clipped by any containing groups)
//    is displayed.
void MCNativeLayerX11::doSetGeometry(const MCRectangle& p_rect)
{
	m_rect = p_rect;
	updateContainerGeometry();

	if (m_child_window == NULL || m_browser_widget == NULL)
        return;

	// Compute the browser widget's position within m_child_window.
	// When m_rect extends beyond the viewport, x/y may be negative,
	// which clips the browser to the visible area.
	MCRectangle t_rect;
	t_rect = m_rect;
	t_rect.x -= m_intersect_rect.x;
	t_rect.y -= m_intersect_rect.y;

    if (t_rect.width > 0 && t_rect.height > 0)
    {
        // Set the minimum size so WebKit knows its render surface dimensions.
        gtk_widget_set_size_request(m_browser_widget, t_rect.width, t_rect.height);

        // Directly allocate so GTK layout takes effect immediately rather
        // than waiting for the next frame clock cycle.
        GtkAllocation t_alloc;
        t_alloc.x      = t_rect.x;
        t_alloc.y      = t_rect.y;
        t_alloc.width  = t_rect.width;
        t_alloc.height = t_rect.height;
        gtk_widget_size_allocate(m_browser_widget, &t_alloc);
    }
}

void MCNativeLayerX11::doSetVisible(bool p_visible)
{
    if (m_child_window == NULL)
        return;
    if (p_visible)
        gtk_widget_show(GTK_WIDGET(m_child_window));
    else
        gtk_widget_hide(GTK_WIDGET(m_child_window));

	if (p_visible)
		doSetGeometry(m_object->getrect());

	updateInputShape();
}

void MCNativeLayerX11::doRelayer()
{
    // Ensure that the input mask for the widget is up to date
    updateInputShape();

    if (m_child_window == NULL)
        return;

    // Find which native layer this should be inserted below
    MCObject *t_before;
    t_before = findNextLayerAbove(m_object);

    // Insert the widget in the correct place (but only if the card is current)
    if (isAttached() && m_object->getstack()->getcard() == m_object->getstack()->getcurcard())
    {
        // If t_before_window == NULL, this will put the widget on the bottom layer
        MCNativeLayerX11 *t_before_layer;
        GdkWindow* t_before_window;
        if (t_before != NULL)
        {
            t_before_layer = reinterpret_cast<MCNativeLayerX11*>(t_before->getNativeLayer());
            t_before_window = gtk_widget_get_window(GTK_WIDGET(t_before_layer->m_child_window));
        }
        else
        {
            t_before_layer = NULL;
            t_before_window = NULL;
        }
        gdk_window_restack(gtk_widget_get_window(GTK_WIDGET(m_child_window)), t_before_window, FALSE);
    }
}

////////////////////////////////////////////////////////////////////////////////

bool MCNativeLayerX11::GetNativeView(void *&r_view)
{
    r_view = (void*)m_browser_widget;
    return true;
}

////////////////////////////////////////////////////////////////////////////////

x11::Window MCNativeLayerX11::getStackX11Window()
{
    // -- tperry 13-11-2025: GTK3 removed gdk_x11_drawable_get_xid, use gdk_x11_window_get_xid
    return x11::gdk_x11_window_get_xid(getStackGdkWindow());
}

GdkWindow* MCNativeLayerX11::getStackGdkWindow()
{
    return m_object->getstack()->getwindow();
}

////////////////////////////////////////////////////////////////////////////////

MCNativeLayer* MCNativeLayer::CreateNativeLayer(MCObject *p_object, void *p_native_view)
{
    // p_native_view is a GtkWidget* (WebKitWebView) returned by
    // MCWebKitGTKBrowser::GetNativeLayer().
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
