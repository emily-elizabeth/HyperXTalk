/* Copyright (C) 2003-2015 LiveCode Ltd.

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

//
// ScreenDC virtual functions
//

#include "lnxprefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"



#include "dispatch.h"
#include "image.h"
#include "stack.h"
#include "util.h"
#include "variable.h"

#include "globals.h"

#include "printer.h"

#include "lnxdc.h"

#include "lnxpsprinter.h"

#include "mctheme.h"

#include "notify.h"

//#include <X11/extensions/Xinerama.h>

#include "graphics_util.h"
#include <fontconfig/fontconfig.h>
#include "font.h"
#include "redraw.h"
#include "resolution.h"

////////////////////////////////////////////////////////////////////////////////

static Boolean pserror;
Bool debugtest = False;

////////////////////////////////////////////////////////////////////////////////

MCGFloat MCResGetSystemScale(void)
{
	// IM-2013-08-12: [[ ResIndependence ]] Linux implementation currently returns 1.0
	return 1.0;
}

////////////////////////////////////////////////////////////////////////////////

MCScreenDC::MCScreenDC()
{
	m_application_has_focus = true ; // The application start's up having focus, one assumes.
}

MCScreenDC::~MCScreenDC()
{
	if (opened)
		close(True);
	if (ncolors != 0)
	{
		int2 i;
		for (i = 0 ; i < ncolors ; i++)
		{
            if (colornames[i] != NULL)
                MCValueRelease(colornames[i]);
		}
		delete colors;
		delete colornames;
		delete allocs;
	}
	
	while (pendingevents != NULL)
	{
		MCEventnode *tptr =(MCEventnode *)pendingevents->remove
		                   (pendingevents);
		
		if ( tptr != NULL ) 
			delete tptr;
	}
}


//TS : X11 Context creation
/*

  These functions are needed to create the context's that graphics are drawn with.
 
*/

bool MCScreenDC::hasfeature(MCPlatformFeature p_feature)
{
	switch(p_feature)
	{
	case PLATFORM_FEATURE_WINDOW_TRANSPARENCY:
		return is_composite_wm(0);
	break;
		
	case PLATFORM_FEATURE_OS_COLOR_DIALOGS:
		return m_has_native_color_dialogs;
	break;

		
	case PLATFORM_FEATURE_OS_FILE_DIALOGS:
		return m_has_native_file_dialogs;
	break;

	case PLATFORM_FEATURE_OS_PRINT_DIALOGS:
		return m_has_native_print_dialogs;
	break;
		
	case PLATFORM_FEATURE_NATIVE_THEMES:
		return m_has_native_theme;
	break;

	case PLATFORM_FEATURE_TRANSIENT_SELECTION:
		return true;
	break;

	default:
		assert(false);
	break;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////

bool MCX11GetWindowWorkarea(GdkDisplay *p_display, Window p_window, MCRectangle &r_workarea)
{
    x11::Atom t_ret;
	int t_format, t_status;
	unsigned long t_count, t_after;
	unsigned long *t_workarea = nil;

    x11::Atom XA_CARDINAL = x11::gdk_x11_atom_to_xatom_for_display(p_display, gdk_atom_intern_static_string("CARDINAL"));
    
    // -- tperry 12-11-2025: GTK3 removed gdk_x11_drawable_get_xid, use gdk_x11_window_get_xid
    t_status = x11::XGetWindowProperty(x11::gdk_x11_display_get_xdisplay(p_display),
                                       x11::gdk_x11_window_get_xid(p_window),
                                       x11::gdk_x11_atom_to_xatom_for_display(p_display, MCworkareaatom),
                                       0, 4, False, XA_CARDINAL, &t_ret, &t_format, &t_count, &t_after,
                                       (unsigned char**)&t_workarea);
	
	bool t_success;
	t_success = t_status == Success && t_ret == XA_CARDINAL && t_format == 32 && t_count == 4;
	
	if (t_success)
		r_workarea = MCRectangleMake(t_workarea[0], t_workarea[1], t_workarea[2], t_workarea[3]);
		
	if (t_workarea != nil)
		x11::XFree(t_workarea);
	
	return t_success;
}

// IM-2014-01-29: [[ HiDPI ]] Apply screen workarea to given MCDisplay array
bool MCScreenDC::apply_workarea(MCDisplay *p_displays, uint32_t p_display_count)
{
	bool t_success;
	t_success = true;
	
	MCRectangle t_workarea;
	t_success = MCX11GetWindowWorkarea(dpy, getroot(), t_workarea);
	
	if (t_success)
	{
		for (uint32_t i = 0; i < p_display_count; i++)
			p_displays[i].workarea = MCU_intersect_rect(t_workarea, p_displays[i].viewport);
	}
	
	return t_success;
}

// IM-2014-01-29: [[ HiDPI ]] Apply screen struts to given MCDisplay array
bool MCScreenDC::apply_partial_struts(MCDisplay *p_displays, uint32_t p_display_count)
{
	if (MCstrutpartialatom == None || MCclientlistatom == None)
		return false;
	
	bool t_success = true;
	
    x11::Atom t_ret;
	int t_format, t_status;
    x11::Window *t_clients = nil;
	unsigned long t_client_count, t_after;
	
    x11::Atom XA_WINDOW = x11::gdk_x11_atom_to_xatom_for_display(dpy, gdk_atom_intern_static_string("WINDOW"));
    x11::Atom XA_CARDINAL = x11::gdk_x11_atom_to_xatom_for_display(dpy, gdk_atom_intern_static_string("CARDINAL"));
    
    t_status = x11::XGetWindowProperty(x11::gdk_x11_display_get_xdisplay(dpy),
                                       // -- tperry 12-11-2025: GTK3 removed gdk_x11_drawable_get_xid
                                       x11::gdk_x11_window_get_xid(getroot()),
                                       x11::gdk_x11_atom_to_xatom_for_display(dpy, MCclientlistatom),
                                       0, -1, False,XA_WINDOW, &t_ret, &t_format, &t_client_count, &t_after,
                                       (unsigned char **)&t_clients);
    
	t_success = t_status == Success && t_ret == XA_WINDOW && t_format == 32;
	
	if (t_success)
	{
		int32_t t_screenwidth, t_screenheight;
		t_screenwidth = device_getwidth();
		t_screenheight = device_getheight();
		for (uindex_t i = 0; t_success && i < t_client_count; i++)
		{
			unsigned long t_strut_count;
			unsigned long *t_struts = nil;
			
            t_status = x11::XGetWindowProperty(x11::gdk_x11_display_get_xdisplay(dpy),
                                               t_clients[i],
                                               x11::gdk_x11_atom_to_xatom_for_display(dpy, MCstrutpartialatom),
                                               0, 12, False, XA_CARDINAL, &t_ret, &t_format, &t_strut_count, &t_after,
                                               (unsigned char **)&t_struts);

			if (t_status == Success && t_ret == XA_CARDINAL && t_format == 32 && t_strut_count == 12)
			{
				MCRectangle t_strut_rect = {0,0,0,0};
				MCRectangle t_strut_test = {0,0,0,0};
				
				if (t_struts[0] > 0)
				{
					// LEFT
					t_strut_rect.x = t_struts[0];
					t_strut_rect.y = 0;
					t_strut_rect.width = t_screenwidth - t_strut_rect.x;
					t_strut_rect.height = t_screenheight;
				
					t_strut_test = t_strut_rect;
					t_strut_test.y = t_struts[4];
					t_strut_test.height = t_struts[5] - t_strut_test.y;
				}
				else if (t_struts[1] > 0)
				{
					// RIGHT
					t_strut_rect.x = 0;
					t_strut_rect.y = 0;
					t_strut_rect.width = t_screenwidth - t_struts[1];
					t_strut_rect.height = t_screenheight;
				
					t_strut_test = t_strut_rect;
					t_strut_test.y = t_struts[6];
					t_strut_test.height = t_struts[7] - t_strut_test.y;
				}
				else if (t_struts[2] > 0)
				{
					// TOP
					t_strut_rect.x = 0;
					t_strut_rect.y = t_struts[2];
					t_strut_rect.width = t_screenwidth;
					t_strut_rect.height = t_screenheight - t_strut_rect.y;
				
					t_strut_test = t_strut_rect;
					t_strut_test.x = t_struts[8];
					t_strut_test.width = t_struts[9] - t_strut_test.x;
				}
				else if (t_struts[3] > 0)
				{
					// BOTTOM
					t_strut_rect.x = 0;
					t_strut_rect.y = 0;
					t_strut_rect.width = t_screenwidth;
					t_strut_rect.height = t_screenheight - t_struts[3];
				
					t_strut_test = t_strut_rect;
					t_strut_test.x = t_struts[10];
					t_strut_test.width = t_struts[11] - t_strut_test.x;
				}
				
				for (uindex_t s = 0; s < p_display_count; s++)
				{
					MCRectangle t_workarea = p_displays[s].workarea;

					MCRectangle t_test = MCU_intersect_rect(t_strut_test, t_workarea);
					if (t_test.width != 0 && t_test.height != 0)
						t_workarea = MCU_intersect_rect(t_strut_rect, t_workarea);
						
					p_displays[s].workarea = t_workarea;
				}
			}
			if (t_struts != nil)
				x11::XFree(t_struts);
		}
	}
	
	if (t_clients != nil)
		x11::XFree(t_clients);
		
	return t_success;
}

bool MCScreenDC::platform_getdisplays(bool p_effective, MCDisplay *&r_displays, uint32_t &r_display_count)
{
	return device_getdisplays(p_effective, r_displays, r_display_count);
}

// p_effective is not used here: both MCDisplay::viewport (full geometry) and
// MCDisplay::workarea (taskbar-excluded) are always populated.  The superclass
// (MCUIDC::getdisplays) uses p_effective solely for cache invalidation and
// callers read whichever field they need.  This matches all other platform
// implementations (Windows, Android, etc.).
bool MCScreenDC::device_getdisplays(bool /*p_effective*/, MCDisplay * &r_displays, uint32_t &r_display_count)
{
    // GTK3: enumerate monitors via the display, not via a GdkScreen
    gint t_monitor_count;
    t_monitor_count = gdk_display_get_n_monitors(dpy);

    // Allocate the list of monitors
    MCDisplay *t_displays;
    if (!MCMemoryNewArray(t_monitor_count, t_displays))
        return false;

    // Get the geometry of each monitor.
    // gdk_monitor_get_geometry() returns logical (application) pixels.
    for (gint i = 0; i < t_monitor_count; i++)
    {
        GdkRectangle t_rect;
        GdkMonitor *t_monitor = gdk_display_get_monitor(dpy, i);
        gdk_monitor_get_geometry(t_monitor, &t_rect);

        MCRectangle t_mc_rect;
        t_mc_rect = MCRectangleMake(t_rect.x, t_rect.y, t_rect.width, t_rect.height);

        t_displays[i].index = i;
        // MCDisplay::pixel_scale is read by the 'screenpixelscale(s)' property
        // and by getmaxdisplayscale().  While pixel scaling is disabled on
        // Linux (MCResPlatformSupportsPixelScaling() returns false), IDE scripts
        // that use 'the screenpixelscale' to position windows expect 1.0 here.
        // Setting it to gdk_monitor_get_scale_factor() breaks multi-monitor
        // placement because the script computes logical-pixel rects that no
        // longer match the GDK coordinate space.
        //
        // The signal handlers and MCLinuxGetLogicalToScreenScale() query GDK
        // directly and do not rely on this field, so they remain correct.
        //
        // TODO [[ HiDPI ]]: set pixel_scale = gdk_monitor_get_scale_factor(t_monitor)
        // once MCLinuxStackSurface renders at physical pixel dimensions and
        // MCResPlatformSupportsPixelScaling() returns true.
        t_displays[i].pixel_scale = 1.0;
        t_displays[i].viewport = t_displays[i].workarea = t_mc_rect;
    }
    
    if (t_monitor_count == 1)
    {
        apply_workarea(t_displays, t_monitor_count) || apply_partial_struts(t_displays, t_monitor_count);
    }
    else
    {
        apply_partial_struts(t_displays, t_monitor_count);
    }
    
    // All done
    r_displays = t_displays;
    r_display_count = t_monitor_count;
    return true;
}



#define LIST_PRINTER_SCRIPT "put \"\" into tPrinters;" \
 "repeat for each line tLine in shell(\"lpstat -a\");" \
 "put word 1 of tLine & return after tPrinters ;" \
 "end repeat;" \
 "delete the last char of tPrinters;" \
 "get tPrinters; return it" \



bool MCScreenDC::listprinters(MCStringRef& r_printers)
{
	MCdefaultstackptr->domess(MCSTR(LIST_PRINTER_SCRIPT));
    MCresult->copyasvalueref((MCValueRef&)r_printers);
	return true;
}



MCPrinter *MCScreenDC::createprinter(void)
{
	return ( new MCPSPrinter );
	
}

////////////////////////////////////////////////////////////////////////////////

// Return the HiDPI scale factor of the primary (or first) monitor.
// Mirrors MCWin32GetLogicalToScreenScale() on Windows.
// Returns 1.0 when pixel scaling is disabled so callers need no special case.
MCGFloat MCLinuxGetLogicalToScreenScale(void)
{
    if (!MCResGetUsePixelScaling())
        return 1.0;

    // MCResInitPixelScaling() is called twice in globals.cpp: once before
    // MCScreenDC::open() and once after.  The first call arrives before GTK
    // has been initialised, so gdk_display_get_default() returns NULL —
    // 1.0 is the correct safe default for that phase.  A spin-wait would
    // deadlock: gdk_display_get_default() only becomes non-NULL after
    // gdk_display_open() runs and there is no event loop to pump while
    // waiting.  The second call, made after open(), picks up the real monitor
    // scale.  We use the default display rather than MCScreenDC::dpy so this
    // free function works correctly in both phases.
    GdkDisplay *t_display = gdk_display_get_default();
    if (t_display == NULL)
        return 1.0;

    GdkMonitor *t_monitor = gdk_display_get_primary_monitor(t_display);
    if (t_monitor == NULL)
        t_monitor = gdk_display_get_monitor(t_display, 0);
    if (t_monitor == NULL)
        return 1.0;

    return (MCGFloat)gdk_monitor_get_scale_factor(t_monitor);
}

MCPoint MCScreenDC::logicaltoscreenpoint(const MCPoint &p_point)
{
    MCGFloat t_scale = MCLinuxGetLogicalToScreenScale();
    return MCPointTransform(p_point, MCGAffineTransformMakeScale(t_scale, t_scale));
}

MCPoint MCScreenDC::screentologicalpoint(const MCPoint &p_point)
{
    MCGFloat t_scale = 1.0 / MCLinuxGetLogicalToScreenScale();
    return MCPointTransform(p_point, MCGAffineTransformMakeScale(t_scale, t_scale));
}

MCRectangle MCScreenDC::logicaltoscreenrect(const MCRectangle &p_rect)
{
    return MCRectangleGetScaledFloorRect(p_rect, MCLinuxGetLogicalToScreenScale());
}

MCRectangle MCScreenDC::screentologicalrect(const MCRectangle &p_rect)
{
    return MCRectangleGetScaledCeilingRect(p_rect, 1.0 / MCLinuxGetLogicalToScreenScale());
}

bool MCScreenDC::platform_get_display_handle(void *&r_display)
{
	r_display = x11::gdk_x11_display_get_xdisplay(getDisplay());
	
	return true;
}

void *MCScreenDC::GetNativeWindowHandle(Window p_window)
{
	// -- tperry 13-11-2025: GTK3 - Window is now GdkWindow*, get X11 XID directly
	// x11 window handle - return the X11 Window id
	if (p_window == NULL)
		return NULL;
	return (void*)(uintptr_t)x11::gdk_x11_window_get_xid(p_window);
}

////////////////////////////////////////////////////////////////////////////////

void MCResPlatformInitPixelScaling(void)
{
    // GDK handles HiDPI awareness automatically on GTK3 — no explicit
    // process-level DPI awareness call is needed (unlike Windows where
    // SetProcessDPIAware() must be called at startup).
    // Runtime scale-change notifications are handled via GDK monitor
    // signals connected in lnxdcs.cpp.
}

// GTK3 (GDK 3.22+) exposes per-monitor scale factors via
// gdk_monitor_get_scale_factor(), so HiDPI detection infrastructure is
// in place.  However, MCLinuxStackSurface currently creates its pixbuf
// at *logical* pixel dimensions and blits via MCX11PutImage, bypassing
// GDK's scaling layer.  XWayland already handles logical→physical scaling
// at the compositor level, so enabling pixel scaling here causes the
// tilecache to render at Nx into a 1× raster, garbling the display.
//
// TODO [[ HiDPI ]]: return true once MCLinuxStackSurface creates surfaces
// at physical pixel dimensions (and coordinate math is updated accordingly).
bool MCResPlatformSupportsPixelScaling(void)
{
    return false;
}

// Scale factor is read from GDK at runtime; it cannot be set by the user
// from within the app (controlled by GNOME display settings / GDK_SCALE).
bool MCResPlatformCanChangePixelScaling(void)
{
    return false;
}

bool MCResPlatformCanSetPixelScale(void)
{
    return false;
}

// Return the primary monitor's GDK scale factor as the default.
MCGFloat MCResPlatformGetDefaultPixelScale(void)
{
    return MCLinuxGetLogicalToScreenScale();
}

// UI and device coordinates are the same on Linux (no separate UIKit-style
// layer), matching the Windows and desktop macOS behaviour.
MCGFloat MCResPlatformGetUIDeviceScale(void)
{
    return 1.0;
}

// Called by MCResSetPixelScale() when the global pixel scale changes.
// Iterates all open stacks, updates their backing scale, marks content
// dirty, and schedules a redraw — mirroring the WM_DPICHANGED handler
// on Windows (w32dcw32.cpp).
void MCResPlatformHandleScaleChange(void)
{
    MCGFloat t_new_scale = MCLinuxGetLogicalToScreenScale();

    MCdispatcher->foreachstack([](MCStack *p_stack, void *p_context) -> bool
    {
        MCGFloat t_scale = *(MCGFloat *)p_context;
        p_stack->view_setbackingscale(t_scale);
        p_stack->dirtyall();
        MCRedrawScheduleUpdateForStack(p_stack);
        return true; // continue iteration
    }, &t_new_scale);
}

////////////////////////////////////////////////////////////////////////////////

static bool s_fontconfig_resolved = false;
static bool s_can_use_fontconfig = true;

////////////////////////////////////////////////////////////////////////////////

// AL-2015-01-20 [[ DynamicFonts ]] Implement font loading on Linux
bool MCScreenDC::loadfont(MCStringRef p_path, bool p_globally, void*& r_loaded_font_handle)
{
    // We can't load fonts globally with fontconfig
    if (p_globally)
        return false;
    
    if (!FcInit())
        return false;
    
    FcConfig *t_config;
    t_config = FcInitLoadConfigAndFonts();
    
    if (t_config == nil)
        return false;
    
    MCAutoStringRefAsSysString t_font_file;
    t_font_file . Lock(p_path);
    
    if (FcFalse == FcConfigAppFontAddFile(t_config, (FcChar8*)*t_font_file))
        return false;
    
    if (FcFalse == FcConfigSetCurrent(t_config))
        return false;
    
    if (FcFalse == FcConfigBuildFonts(t_config))
        return false;
    
    // We don't actually do anything with the loaded font handle at the moment.
    // It is slightly awkward to create one, so we just set it to nil for now.
    r_loaded_font_handle = nil;
    return true;
}

bool MCScreenDC::unloadfont(MCStringRef p_path, bool p_globally, void *r_loaded_font_handle)
{
    // We can't unload fonts globally with fontconfig
    if (p_globally)
        return false;
    
    if (!s_can_use_fontconfig)
        return false;
    
    if (!FcInit())
        return false;
    
    FcConfig *t_config;
    t_config = FcInitLoadConfigAndFonts();
    
    if (t_config == nil)
        return false;
    
    // fontconfig does not currently supply a remove file function, so we need to
    // unload all of the fonts and reload all but the specified one.

    MCStringRef *t_font_files_in_use = nil;
    uindex_t t_count;
    
    if (!MCFontListLoaded(t_count, t_font_files_in_use))
        return false;
    
    FcConfigAppFontClear(t_config);
    
    for (uindex_t i = 0; i < t_count; i++)
    {
        if (!MCStringIsEqualTo(t_font_files_in_use[i], p_path, kMCStringOptionCompareCaseless))
        {
            MCAutoStringRefAsSysString t_to_load;
            t_to_load . Lock(t_font_files_in_use[i]);
            FcConfigAppFontAddFile(t_config, (FcChar8*)*t_to_load);
        }
    }
    
    if (!FcConfigSetCurrent(t_config))
        return false;
    
    if (!FcConfigBuildFonts(t_config))
        return false;
    
    return true;
}
