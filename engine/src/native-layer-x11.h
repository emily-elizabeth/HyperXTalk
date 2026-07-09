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
	
	virtual bool GetCanRenderToContext();
    
    virtual bool GetNativeView(void *&r_view);
    
    MCNativeLayerX11(MCObject *p_object, GtkWidget *p_view);
    ~MCNativeLayerX11();

private:

    GtkWindow* m_child_window;
    // -- tperry 12-11-2025: GTK3 uses cairo_region_t instead of GdkRegion
    cairo_region_t* m_input_shape;
    // The native browser widget (WebKitWebView) added directly as a GTK child
    // of m_child_window.  Received as a GtkWidget* from GetNativeLayer().
    GtkWidget* m_browser_widget;
	MCRectangle m_intersect_rect;

    // 60 Hz position timer: enforces the browser window's absolute screen
    // position every ~16 ms.  Replaces the ConfigureNotify filter approach,
    // which had a timing race when the browser held X11 focus: Mutter's
    // focused-transient repositioning fired after our snap-back idle, leaving
    // the window persistently detached.  The timer corrects any WM-induced
    // drift within one display frame regardless of focus state.
    // Loop safety: gdk_window_move_resize with the already-correct coordinates
    // produces no visual change and no ConfigureNotify.
    guint m_position_timer_id;  // g_timeout source id, 0 when not running

    static gboolean onPositionTimer(gpointer user_data);


    // Returns the handle for the stack containing this widget
    x11::Window getStackX11Window();
    GdkWindow* getStackGdkWindow();
    
    // Returns the GtkFixed used for layouts within the stack
    GtkFixed* getStackLayout();
    
    // Performs the attach/detach operations
    virtual void doAttach();
    virtual void doDetach();
	
	virtual bool doPaint(MCGContextRef p_context);
	virtual void doSetGeometry(const MCRectangle &p_rect);
	virtual void doSetViewportGeometry(const MCRectangle &p_rect);
	virtual void doSetVisible(bool p_visible);
    
    // Performs a relayering operation
    virtual void doRelayer();
    
    // Updates the input mask for the widget (used to implement edit mode)
    void updateInputShape();

	void updateContainerGeometry();
};

#endif // ifndef __MC_NATIVE_LAYER_X11__
