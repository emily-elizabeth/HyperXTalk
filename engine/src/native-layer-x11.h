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
    // position every ~16 ms.  Handles any residual WM-induced position drift.
    guint m_position_timer_id;  // g_timeout source id, 0 when not running

    static gboolean onPositionTimer(gpointer user_data);

    // Dynamic WM_TRANSIENT_FOR management.
    //
    // WM_TRANSIENT_FOR is needed for Z-order (Mutter keeps the browser above
    // the stack without it), but when the browser holds X11 focus Mutter
    // applies "focused-transient repositioning" — moving the browser to a
    // WM-computed position every compositor frame, overriding our timer.
    //
    // Fix: remove WM_TRANSIENT_FOR the moment the browser gets X11 focus (so
    // Mutter has no transient relationship to manage), and restore it when
    // focus leaves (so Mutter resumes Z-order management).  The 60 Hz timer
    // covers any position drift during the focus window.
    //
    // Uses a raw X11 FocusIn/FocusOut filter rather than GTK signals because
    // gtk_window_set_accept_focus(FALSE) suppresses GTK-level focus routing
    // while WebKit may still obtain focus via XSetInputFocus.
    GdkWindow* m_child_gdk_window;   // non-owning; NULL when filter not installed
    bool       m_browser_has_focus;  // true while WM_TRANSIENT_FOR is removed

    static GdkFilterReturn onChildFocusFilter(GdkXEvent *xevent,
                                              GdkEvent  *event,
                                              gpointer   user_data);


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
