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
// m_child_window is a GTK_WINDOW_POPUP (override_redirect=TRUE) whose
// underlying X11 window is reparented into the engine's stack window via
// gdk_window_reparent.  This makes it appear at the correct on-screen
// position while the WM never manages it.
//
// m_browser_widget (WebKitWebView) is added directly as a GTK child of
// m_child_window.  This keeps the full GTK widget hierarchy intact:
//
//   m_child_window (GtkWindow/popup) → m_browser_widget (WebKitWebView)
//
// m_child_window's GTK frame clock drives all rendering.  When WebKit
// queues a redraw (gtk_widget_queue_draw), the frame clock fires, GTK
// calls the draw signal on m_browser_widget, and WebKit composites its
// content.  Because m_child_window's X11 window is mapped inside the
// stack window, the content appears at the correct position on screen.
//
// Sizing: gtk_window_resize(m_child_window, w, h) triggers GTK's layout
// cascade which allocates m_browser_widget to fill the window.
//
// No GtkSocket, no GtkPlug, no XEMBED — those approaches broke the GTK3
// frame clock for the embedded widget.


MCNativeLayerX11::MCNativeLayerX11(MCObject *p_object, GtkWidget *p_view) :
  m_child_window(NULL),
  m_input_shape(NULL),
  m_browser_widget(p_view)
{
	m_object = p_object;
	m_intersect_rect = MCRectangleMake(0,0,0,0);
}

MCNativeLayerX11::~MCNativeLayerX11()
{
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

void MCNativeLayerX11::doAttach()
{
    if (m_child_window == NULL)
    {
        MCRectangle t_rect;
        t_rect = m_object->getrect();

        // Create a popup window to hold the browser widget.  GTK_WINDOW_POPUP
        // sets override_redirect=TRUE so the WM never manages it.
        m_child_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_POPUP));

        if (m_browser_widget != NULL)
        {
            // Add the browser widget as the only GTK child.  GTK will
            // allocate it to fill the window on every layout pass.
            gtk_container_add(GTK_CONTAINER(m_child_window), m_browser_widget);
        }

        // Realize m_child_window (and its child) while it is still an
        // unparented toplevel — GTK3 requires widgets to be anchored to a
        // GtkWindow before they can be realized.
        gtk_widget_realize(GTK_WIDGET(m_child_window));

        // Reparent the popup's X11 window into the stack window BEFORE
        // mapping it.  If we show first, the window briefly appears at root
        // coordinates (0,0) while any active pointer grab (e.g. from a
        // tools-palette drag) is in effect.  GTK may try to redirect grab
        // state to the newly-mapped root-level window, causing a crash.
        // Reparenting first means the first XMapWindow call places the window
        // directly inside the stack — it is never visible at root level.
        // GDK still thinks m_child_window is a root-level popup; that is
        // intentional — GTK's frame clock keeps ticking, driving WebKit's
        // draw cycle.
        gdk_window_reparent(gtk_widget_get_window(GTK_WIDGET(m_child_window)),
                            getStackGdkWindow(), t_rect.x, t_rect.y);

        // Show the window and child so the frame clock starts and WebKit's
        // rendering pipeline initialises.  The window maps inside the stack
        // (after the reparent above), so no root-level flash occurs.
        gtk_widget_show_all(GTK_WIDGET(m_child_window));

        // Create an empty region to act as an input mask while in edit mode.
        m_input_shape = cairo_region_create();
    }

    // Position and size everything correctly.
    doRelayer();
    doSetViewportGeometry(m_viewport_rect);
    doSetGeometry(m_rect);
    doSetVisible(ShouldShowLayer());
}

void MCNativeLayerX11::doDetach()
{
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

    // Move and resize the X11 window within the stack window.
    // Guard against zero size — X11 requires w > 0, h > 0.
    if (m_intersect_rect.width > 0 && m_intersect_rect.height > 0)
    {
        gdk_window_move_resize(gtk_widget_get_window(GTK_WIDGET(m_child_window)),
            m_intersect_rect.x, m_intersect_rect.y,
            m_intersect_rect.width, m_intersect_rect.height);

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
