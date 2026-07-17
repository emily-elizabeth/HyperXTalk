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
// X ScreenDC display specific functions
//
#include "lnxprefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"


#include "dispatch.h"
#include "image.h"
#include "stack.h"
#include "card.h"
#include "field.h"
#include "sellst.h"
#include "util.h"

#include "debug.h"
#include "osspec.h"
#include "stacklst.h"

#include "globals.h"

#include "lnxdc.h"
#include "lnxgtkthemedrawing.h"

#include "resolution.h"

// Declared in lnxstack.cpp — returns the realized GdkWindow of the GtkPopover
// widget so the dismiss check can walk the GdkWindow parent chain.
extern GdkWindow *MCLinuxPopoverGetGdkWindow(void);

// Declared in native-layer-x11.cpp — forward raw GDK key events to any
// offscreen WebKitWebView that currently owns browser keyboard focus.
// Called immediately after the normal HXT wkdown/wkup dispatch so the browser
// receives keys even though HXT's own kfocus mechanism is not involved.
extern bool hxt_browser_has_focus();
extern void hxt_browser_clear_focus();
extern void hxt_browser_reset_mousedown_flag();
extern bool hxt_browser_mousedown_fired();
extern void hxt_browser_key_down(unsigned int keyval, unsigned int state,
                                  unsigned short hwcode, unsigned char group);
extern void hxt_browser_key_up(unsigned int keyval, unsigned int state,
                                unsigned short hwcode, unsigned char group);

// Declared in native-layer-x11.cpp — forward GDK_MOTION_NOTIFY to the
// focused offscreen WebKitWebView while a mouse button is held (drag).
// p_x/p_y are in HXT's scaled stack-window coordinate space.  The function
// is a no-op if no browser has a button currently pressed (m_pointer_button_down).
extern void hxt_browser_forward_motion(int p_x, int p_y);

// Declared in native-layer-x11.cpp — searches all attached native layers for
// one whose rect contains (p_x, p_y) in stack-window coordinates.  Returns
// true and fills r_widget/r_bx/r_by when found.  Supports multiple browsers.
extern bool hxt_find_browser_at(int p_x, int p_y,
                                 GtkWidget **r_widget,
                                 int *r_bx, int *r_by);

// Called from native-layer-x11.cpp::OnMouseDown when the browser widget
// receives a click.  Closes any active HXT text field so that subsequent
// key events route to WebKit instead of the field.
// Defined here (not in native-layer-x11.cpp) to keep MCField/MCStack
// headers out of the native layer.
void hxt_browser_took_focus()
{
    if (!MCactivefield)
        return;

    MCactivefield->getstack()->kunfocus();

    if (MCactivefield)
    {
        // Mirror what MCField::kunfocus() does for the visual repaint, but
        // without firing scripts again (which would re-focus the field).
        // Save gettransient() BEFORE clearing CS_KFOCUSED so the layer knows
        // the rendering changed (focus ring gone) and repaints the whole field.
        uint2 t_old_trans = MCactivefield->gettransient();
        MCscreen->cancelmessageobject(MCactivefield, MCM_internal);      // cancel blink timer re-started by closeField's kfocus
        MCactivefield->removecursor();                                    // erase cursor (removecursor clears cursoron/cursorfield)
        MCactivefield->setstate(False, CS_KFOCUSED);                     // clear focus bit
        MCactivefield->layer_transientchangedandredrawall(t_old_trans);  // repaint: no focus ring
        MCactivefield = nil;
    }
}

#define XK_Window_L 0xFF6C
#define XK_Window_R 0xFF6D

#include <gdk/gdkkeysyms.h>


Boolean tripleclick = False;
static Boolean dragclick;


void MCScreenDC::setupcolors()
{
	// -- tperry 12-11-2025: GTK3 - GdkVisual is opaque, use gdk_visual_get_depth
	// In GTK3, we use a fixed color count based on depth
	gint depth = gdk_visual_get_depth(vis);
	ncolors = MCU_min(1 << depth, MAX_CELLS);
	colors = new (nothrow) MCColor[ncolors];
    colornames = new (nothrow) MCStringRef[ncolors];
	allocs = new (nothrow) int2[ncolors];
	int2 i;
	for (i = 0 ; i < ncolors ; i++)
	{
        colornames[i] = MCValueRetain(kMCEmptyString);
		allocs[i] = 0;
	}
}

GdkScreen* MCScreenDC::getscreen()
{
	return gdk_visual_get_screen(vis);
}

// -- tperry 12-11-2025: GTK3 removed GdkColormap - function removed from header

GdkVisual* MCScreenDC::getvisual()
{
	return vis;
}

KeySym MCScreenDC::translatekeysym(KeySym sym, uint4 keycode)
{
	// Major assumption: GDK keysyms are the same as X11 keysyms
    switch (sym)
	{
	case 0:
		switch (keycode)
		{
		case 115:
			return XK_Window_L;
		case 116:
			return XK_Window_R;
		case 117:

			return XK_Menu;
		}
		break;
	case XK_L4:
	case XK_Undo:
		return XK_osfUndo;
	case XK_L10:
	case XK_apCut:
	case XK_SunCut:
		return XK_osfCut;
	case XK_L6:
	case XK_apCopy:
	case XK_SunCopy:
		return XK_osfCopy;
	case XK_L8:
	case XK_apPaste:
	case XK_SunPaste:
		return XK_osfPaste;
	case XK_Help:
		return XK_osfHelp;
	case XK_osfInsert:
	case XK_hpInsertChar:
		return XK_Insert;
	case XK_osfDelete:
	case XK_hpDeleteChar:
		return XK_Delete;
	case XK_ISO_Left_Tab: /* X11 shift-tab keysym */
	case 0x1000FF74: // HP shift-tab keysysm
		return XK_Tab;
	}
	return sym;
}

static Boolean isKeyPressed(char *km, uint1 keycode)
{
	return (km[keycode >> 3] >> (keycode & 7)) & 1;
}

bool MCScreenDC::getkeysdown(MCListRef& r_list)
{
	MCAutoListRef t_list;
	if (!MCListCreateMutable(',', &t_list))
		return false;

    // GDK keymap for mapping hardware keycodes to symbols
    GdkKeymap *t_keymap;
    t_keymap = gdk_keymap_get_for_display(dpy);
    
    // GDK does not provide a wrapper for XQueryKeymap so we have to use it
    // directly if we want to get the key state from X11.
	char km[32];
	MCmodifierstate = querymods();
    x11::XQueryKeymap(x11::gdk_x11_display_get_xdisplay(dpy), km);
    
    // Translate the engine modifiers to GDK modifiers
    gint t_mods = 0;
    if (MCmodifierstate & (MS_SHIFT|MS_CAPS_LOCK))
        t_mods |= GDK_SHIFT_MASK;
    if (MCmodifierstate & MS_CONTROL)
        t_mods |= GDK_CONTROL_MASK;
    if (MCmodifierstate & MS_MOD1)
        t_mods |= GDK_MOD1_MASK;
    if (MCmodifierstate & MS_MOD2)
        t_mods |= GDK_MOD2_MASK;
    if (MCmodifierstate & MS_MOD3)
        t_mods |= GDK_MOD3_MASK;
    if (MCmodifierstate & MS_MOD4)
        t_mods |= GDK_MOD4_MASK;
    if (MCmodifierstate & MS_MOD5)
        t_mods |= GDK_MOD5_MASK;
    
    // Loop through the list of hardware keys
    bool t_success = true;
	for (int i = 0; i < 256; i++)
	{
		if (isKeyPressed(km, i))
		{
            guint t_keyval;
            t_success = gdk_keymap_translate_keyboard_state(t_keymap, i, GdkModifierType(t_mods), 0, &t_keyval, NULL, NULL, NULL);
            
            if (t_success && t_keyval > 0)
				if (!MCListAppendInteger(*t_list, t_keyval))
					t_success = false;
            
            if (!t_success)
                break;
		}
	}

	return t_success && MCListCopy(*t_list, r_list);
}

void MCScreenDC::setmods(guint state, KeySym sym,
                         uint2 button, Boolean release)
{
	if (lockmods)
		return;

    // Set the button state
    uint2 t_buttons = 0;
    if (state & GDK_BUTTON1_MASK)
        t_buttons |= 1;
    if (state & GDK_BUTTON2_MASK)
        t_buttons |= 2;
    if (state & GDK_BUTTON3_MASK)
        t_buttons |= 4;
    if (state & GDK_BUTTON4_MASK)
        t_buttons |= 8;
    if (state & GDK_BUTTON5_MASK)
        t_buttons |= 16;
    
	if (button)
    {
        // Update the particular button
        if (release)
			MCbuttonstate = t_buttons & ~(1 << (button-1));
		else
			MCbuttonstate = t_buttons |  (1 << (button-1));
    }
	else
    {
		// Update all the buttons
        MCbuttonstate = t_buttons;
    }
    
    // Assumption: GDK keysyms and X11 keysyms have the same values
	if (sym >= XK_Shift_L && sym <= XK_Hyper_R)
		MCmodifierstate = querymods();
	else
	{
		// Convert the GDK modifier flags
        MCmodifierstate = 0;
        if (state & GDK_SHIFT_MASK)
            MCmodifierstate |= MS_SHIFT;
        if (state & GDK_LOCK_MASK)
            MCmodifierstate |= MS_CAPS_LOCK;
        if (state & GDK_CONTROL_MASK)
            MCmodifierstate |= MS_CONTROL;
        if (state & GDK_MOD1_MASK)
            MCmodifierstate |= MS_MOD1;
        if (state & GDK_MOD2_MASK)
            MCmodifierstate |= MS_MOD2;
        if (state & GDK_MOD3_MASK)
            MCmodifierstate |= MS_MOD3;
        if (state & GDK_MOD4_MASK)
            MCmodifierstate |= MS_MOD4;
        if (state & GDK_MOD5_MASK)
            MCmodifierstate |= MS_MOD5;
	}
}

extern "C"
{
void gtk_main_do_event(GdkEvent*);
gboolean gdk_event_is_allocated(const GdkEvent *event);
}

static bool motion_event_filter_fn(GdkEvent *p_event, void*)
{
    return p_event->type == GDK_MOTION_NOTIFY;
}

// Safe replacement for gdk_event_free that avoids crashing on destroyed GObjects.
// Based on the actual GDK source (gtk+-3.24.43/gdk/gdkevents.c gdk_event_free).
// gdk_event_free calls g_object_unref on several GObject pointers inside the
// event. If these objects have been destroyed (e.g. dialog closed), the unref
// crashes on dangling pointers. This function nulls out every GObject pointer
// that gdk_event_free would unref, then calls gdk_event_free which will skip
// the unrefs (NULL checks) and just free non-GObject data and the struct.
//
// From GDK source, gdk_event_free unrefs these GObject pointers:
//   - private->device and private->source_device (via g_clear_object - safe)
//   - event->crossing.subwindow
//   - event->dnd.context
//   - event->owner_change.owner
//   - event->selection.requestor
//   - event->any.window
void safe_gdk_event_free(GdkEvent *event)
{
    if (event == NULL)
        return;
    
    // Null out the window (present in all event types via GdkEventAny).
    // Also prevents crash in event_get_display -> gdk_window_get_display.
    event->any.window = NULL;
    
    // Null out event-type-specific GObject pointer fields that gdk_event_free
    // would g_object_unref. Matches the switch in gdkevents.c exactly.
    switch (event->type)
    {
        case GDK_ENTER_NOTIFY:
        case GDK_LEAVE_NOTIFY:
            event->crossing.subwindow = NULL;
            break;
            
        case GDK_DRAG_ENTER:
        case GDK_DRAG_LEAVE:
        case GDK_DRAG_MOTION:
        case GDK_DRAG_STATUS:
        case GDK_DROP_START:
        case GDK_DROP_FINISHED:
            event->dnd.context = NULL;
            break;
            
        case GDK_OWNER_CHANGE:
            event->owner_change.owner = NULL;
            break;
            
        case GDK_SELECTION_CLEAR:
        case GDK_SELECTION_NOTIFY:
        case GDK_SELECTION_REQUEST:
            event->selection.requestor = NULL;
            break;
            
        case GDK_GRAB_BROKEN:
            event->grab_broken.grab_window = NULL;
            // If another client stole our pointer grab while a popover is open,
            // dismiss the popover so it doesn't stay visible with no grab.
            if (!event->grab_broken.keyboard && MCpopoverstack != nullptr)
                MCdispatcher->wclose(MCpopoverstack->getwindowalways());
            break;
            
        default:
            break;
    }
    
    // Note: private->device and private->source_device are handled by
    // g_clear_object() in gdk_event_free, which nulls before unreffing,
    // so they are already safe and don't need to be cleared here.
    
    // Now call gdk_event_free - with all GObject pointers NULL, it will only
    // free non-GObject data (strings, axes, regions, etc.) and the struct.
    gdk_event_free(event);
}

Boolean MCScreenDC::handle(Boolean dispatch, Boolean anyevent, Boolean& abort, Boolean& reset)
{
    // Event object. Note that GDK requires these to be disposed of after handling
    GdkEvent *t_event = NULL;
    
    // Loop until both the pending event queue and GDK event queue are empty
    abort = reset = False;
    bool t_handled = false;
    while (dispatch || g_main_context_pending(NULL) || gdk_events_pending())
    {
        // Place all events onto the pending event queue
        EnqueueGdkEvents();
        
        bool t_queue = false;
        if (dispatch && pendingevents != NULL)
        {
            // Get the next event from the queue - take ownership instead of
            // copying to avoid double-free of the GdkWindow inside the event.
            MCEventnode *tptr = (MCEventnode *)pendingevents->remove(pendingevents);
            t_event = tptr->event;
            tptr->event = NULL;
            delete tptr;
        }
        
        if (t_event == NULL)
        {
            break;
        }

        // What type of event are we dealing with?
        switch (t_event->type)
        {
            case GDK_DELETE:
            {
                MCdispatcher->wclose(t_event->any.window);
                break;
            }
            
            case GDK_EXPOSE:
            case GDK_DAMAGE:
            {
                // Handled separately
                //fprintf(stderr, "GDK_EXPOSE (window %p)\n", t_event->expose.window);
                MCEventnode *t_node = new (nothrow) MCEventnode(gdk_event_copy(t_event));
                t_node->appendto(pendingevents);
                expose();
                break;
            }
                
            case GDK_FOCUS_CHANGE:
            {
                // Was focus gained or lost?
                if (t_event->focus_change.in)
                {
                    // Focus was gained. If we do not currently have focus, we
                    // can now assume that we do.
                    if (!m_application_has_focus)
                    {
                        m_application_has_focus = true;
                        hidebackdrop(true);
                        if (MCdefaultstackptr)
                            MCdefaultstackptr->getcard()->message(MCM_resume);
                        
                        MCstacks->hidepalettes(false);
                    }
                    
                    if (dispatch)
                    {
                        if (t_event->focus_change.window != MCtracewindow)
                        {
                            // XFCE workaround: Limit focus event dispatching on XFCE
                            // XFCE has severe focus management issues with all window types
                            bool skip_focus = false;
                            const char *desktop = getenv("XDG_CURRENT_DESKTOP");
                            if (desktop && (strstr(desktop, "XFCE") || strstr(desktop, "xfce")))
                            {
                                MCStack *t_stack = MCdispatcher->findstackd(t_event->focus_change.window);
                                // Only dispatch focus for top-level windows, skip palettes and modeless
                                if (t_stack && t_stack->getrealmode() != WM_TOP_LEVEL && t_stack->getrealmode() != WM_TOP_LEVEL_LOCKED)
                                    skip_focus = true;
                            }
                            
                            if (!skip_focus)
                                MCdispatcher->wkfocus(t_event->focus_change.window);
                        }
                    }
                    else
                        t_queue = true;
                }
                else
                {
                    // Focus was lost. Was it to another LiveCode window or to
                    // a different application?
                    bool t_lostfocus = false;
                    x11::Window t_return_window;
                    int t_return_revert_window;
                    
                    if (m_application_has_focus)
                    {
                        // GDK doesn't let us get the focus window so we have
                        // to use Xlib to get it. Sigh.
                        x11::XGetInputFocus(x11::gdk_x11_display_get_xdisplay(dpy), &t_return_window, &t_return_revert_window);
                        
                        // Look up the X11 window XID in GDK's window table. If
                        // it isn't found, it definitely isn't one of ours.
                        GdkWindow *t_window;
                        if ((t_window = x11::gdk_x11_window_lookup_for_display(dpy, t_return_window)) == NULL)
                            t_lostfocus = true;
                        
                        // Even if we found it, it may not be ours. This is very
                        // unlikely but could happen if we've created a GdkWindow
                        // for it in the past (e.g. in import snapshot)
                        if ((backdrop != NULL && t_window == backdrop) || MCdispatcher->findstackd(t_window) != NULL)
                            t_lostfocus = false;
                        
                        if (t_lostfocus)
                        {
                            // Another application gained focus
                            m_application_has_focus = false;
                            hidebackdrop(true);
                            if (MCdefaultstackptr)
                                MCdefaultstackptr->getcard()->message(MCM_suspend);
                            
                            MCstacks->hidepalettes(true);
                        }
                    }
                    
                    if (dispatch)
                    {
                        if (t_event->focus_change.window != MCtracewindow)
                        {
                            // XFCE workaround: Limit unfocus event dispatching on XFCE
                            bool skip_unfocus = false;
                            const char *desktop = getenv("XDG_CURRENT_DESKTOP");
                            if (desktop && (strstr(desktop, "XFCE") || strstr(desktop, "xfce")))
                            {
                                MCStack *t_stack = MCdispatcher->findstackd(t_event->focus_change.window);
                                // Only dispatch unfocus for top-level windows
                                if (t_stack && t_stack->getrealmode() != WM_TOP_LEVEL && t_stack->getrealmode() != WM_TOP_LEVEL_LOCKED)
                                    skip_unfocus = true;
                            }
                            
                            if (!skip_unfocus)
                                MCdispatcher->wkunfocus(t_event->focus_change.window);
                        }
                    }
                    else
                        t_queue = true;
                }
                
                t_handled = true;
                break;
            }
                
            case GDK_KEY_PRESS:
            case GDK_KEY_RELEASE:
            {
                // We also want the key symbol for non-character keys etc
                uint32_t t_keysym = translatekeysym(t_event->key.keyval, t_event->key.hardware_keycode);
                
                // Update the current modifier state
                setmods(t_event->key.state, t_keysym, 0, False);
                
                // Check for the interrupt command
                if (t_event->type == GDK_KEY_PRESS && MCmodifierstate & MS_CONTROL)
                {
                    if (t_keysym == XK_Break || t_keysym == '.')
                    {
                        if (MCallowinterrupts && !MCdefaultstackptr->cantabort())
                            abort = True;
                        else
                            MCinterrupt = true;
                    }
                }
                
                if (dispatch)
                {
                    if (t_event->key.window != MCtracewindow)
                    {
                        // Let the IME have the key event first — but not when the
                        // browser owns focus.  If kunfocus() left MCactivefield set
                        // (e.g. closeField script re-opens the field), the IM must
                        // not eat printable chars that should go to WebKit.
                        bool t_ignore = false;
                        if (dispatch && MCactivefield && m_im_context != nil
                                && !hxt_browser_has_focus())
                        {
                            t_ignore = gtk_im_context_filter_keypress(m_im_context, &t_event->key);
                        }
                        
                        // No further processing of the event if the IME ate it
                        if (t_ignore)
                        {
                            t_handled = true;
                            break;
                        }
                        
                        // Convert the key event into a Unicode character
                        codepoint_t t_codepoint = gdk_keyval_to_unicode(t_event->key.keyval);
                        MCAutoStringRef t_text;
                        if (t_codepoint != 0)
                            /* UNCHECKED */ MCStringCreateWithBytes((byte_t*)&t_codepoint, sizeof(t_codepoint), kMCStringEncodingUTF32, false, &t_text);
                        else
                            t_text = MCValueRetain(kMCEmptyString);
                        
                        MCeventtime = t_event->key.time;
                        // Forward keys to the browser when it owns focus.
                        // Browser focus is set in OnMouseDown and cleared in
                        // hxt_browser_clear_focus() when wmdown() indicates the
                        // user clicked an HXT text field instead.
                        bool t_browser_active = hxt_browser_has_focus();

                        if (t_event->type == GDK_KEY_PRESS)
                        {
                            // When a browser widget owns keyboard focus, send ALL
                            // keys directly to WebKit and skip wkdown entirely.
                            // wkdown → MCObject::kdown calls kfocusnext for XK_Tab
                            // which conflicts with WebKit's own focus traversal,
                            // causing TAB to advance two elements instead of one.
                            if (t_browser_active)
                            {
                                hxt_browser_key_down(t_event->key.keyval,
                                                     (unsigned int)t_event->key.state,
                                                     t_event->key.hardware_keycode,
                                                     t_event->key.group);
                            }
                            else
                            {
                                MCdispatcher->wkdown(t_event->key.window, *t_text, t_keysym);
                            }
                        }
                        else
                        {
                            if (t_browser_active)
                            {
                                hxt_browser_key_up(t_event->key.keyval,
                                                   (unsigned int)t_event->key.state,
                                                   t_event->key.hardware_keycode,
                                                   t_event->key.group);
                            }
                            else
                            {
                                MCdispatcher->wkup(t_event->key.window, *t_text, t_keysym);
                            }
                        }
                    }
                }
                else
                {
                    t_queue = true;
                }
                
                t_handled = true;
                break;
            }
                
            case GDK_ENTER_NOTIFY:
            case GDK_LEAVE_NOTIFY:
            {
                if (t_event->type == GDK_ENTER_NOTIFY)
                {
                    // Update which stack currently contains the mouse
                    MCmousestackptr = MCdispatcher->findstackd(t_event->crossing.window);
                    if (MCmousestackptr)
                        MCmousestackptr->resetcursor(True);
                }
                else
                {
                    // The mouse is not within any of our stacks
                    MCmousestackptr = nil;
                }
                if (dispatch)
                {
                    if (t_event->crossing.window != MCtracewindow)
                    {
                        // XFCE workaround: Limit mouse focus events on XFCE
                        bool skip_mouse_focus = false;
                        const char *desktop = getenv("XDG_CURRENT_DESKTOP");
                        if (desktop && (strstr(desktop, "XFCE") || strstr(desktop, "xfce")))
                        {
                            MCStack *t_stack = MCdispatcher->findstackd(t_event->crossing.window);
                            // Only dispatch mouse focus for top-level windows
                            if (t_stack && t_stack->getrealmode() != WM_TOP_LEVEL && t_stack->getrealmode() != WM_TOP_LEVEL_LOCKED)
                                skip_mouse_focus = true;
                        }
                        
                        if (!skip_mouse_focus)
                        {
                            if (t_event->type == GDK_ENTER_NOTIFY)
                            {
                                if (MCmousestackptr)
                                {
                                    // Send a window focus event
                                    MCdispatcher->enter(t_event->crossing.window);
                                    MCdispatcher->wmfocus(t_event->crossing.window, t_event->crossing.x, t_event->crossing.y);
                                }
                            }
                            else
                            {
                                // Send a window unfocus event
                                MCdispatcher->wmunfocus(t_event->crossing.window);
                            }
                        }
                    }
                }
                else
                {
                    t_queue = true;
                }
                
                t_handled = true;
                break;
            }
                
            case GDK_MOTION_NOTIFY:
            {
                // Get the most up-to-date motion event
                GdkEvent *t_new_event;
                while (GetFilteredEvent(&motion_event_filter_fn, t_new_event, NULL))
                {
                    safe_gdk_event_free(t_event);
                    t_event = t_new_event;
                }
                
                // Update the modifier keys flags
                setmods(t_event->motion.state, 0, 0, False);
                
                // IM-2013-08-12: [[ ResIndependence ]] Scale mouse coords to user space
                MCGFloat t_scale;
                t_scale = MCResGetPixelScale();
                
                MCPoint t_mouseloc;
                t_mouseloc = MCPointMake(t_event->motion.x / t_scale, t_event->motion.y / t_scale);
                
                MCStack *t_mousestack;
                t_mousestack = MCdispatcher->findstackd(t_event->motion.window);
                
                // In certain types of modal loops (e.g. drag-and-drop) we may
                // be receiving events from other windows - in that case, the
                // window won't be found and some massaging is required
                if (t_mousestack == NULL && MCmousestackptr)
                {
                    // Retain the current mouse stack and adjust the coordinates
                    // to be relative to it
                    t_mousestack = MCmousestackptr;
                    gint ox, oy;
                    gdk_window_get_origin(MCmousestackptr->getwindow(), &ox, &oy);
                    t_mouseloc = MCPointMake((t_event->motion.x_root - ox) / t_scale, (t_event->motion.y_root - oy) / t_scale);
                }
                
                // IM-2013-10-09: [[ FullscreenMode ]] Update mouseloc with MCscreen getters & setters
                MCscreen->setmouseloc(t_mousestack, t_mouseloc);

                // Forward motion to WebKit during a browser button-drag so
                // text selection can be extended.  hxt_browser_forward_motion
                // is a no-op unless m_pointer_button_down is set (i.e. a
                // button was pressed inside the browser rect).
                hxt_browser_forward_motion((int)t_mouseloc.x, (int)t_mouseloc.y);

                // If this is a motion hint event, request the rest
                if (t_event->motion.is_hint)
                    gdk_event_request_motions(&t_event->motion);
                
                // Detect if we should start a drag
                if (!dragclick && (MCU_abs(MCmousex - MCclicklocx) > 4 || MCU_abs(MCmousey - MCclicklocy) > 4) && MCbuttonstate != 0)
                {
                    last_window = t_event->motion.window;
                    dragclick = true;
                    MCdispatcher->wmdrag(last_window);
                }
                
                if (dispatch)
                {
                    if (t_event->motion.window != MCtracewindow)
                    {
                        MCeventtime = t_event->motion.time;
                        MCdispatcher->wmfocus(t_event->motion.window, t_mouseloc.x, t_mouseloc.y);
                    }
                }
                else
                    t_queue = true;
                
                t_handled = true;
                break;
            }
             
            case GDK_SCROLL:
            case GDK_BUTTON_PRESS:
            {
                // WM_POPOVER dismiss: if a popover is open and the button press
                // lands outside its window, close the popover first then let the
                // event propagate normally.  We hold a seat grab while the popover
                // is open, so button events outside our application still reach us.
                //
                // NOTE: with GtkPopover's modal grab, clicks inside the popover
                // content area may arrive with event->window set to the GtkPopover's
                // own GdkWindow (W2) rather than the GtkDrawingArea's GdkWindow (W3).
                // We must therefore test whether the event window is anywhere within
                // the popover's GdkWindow subtree, not just check for exact equality
                // against W3.  Walk the GdkWindow parent chain upward: if we reach
                // the popover's GdkWindow the click is inside and we should NOT dismiss.
                if (t_event->type == GDK_BUTTON_PRESS && MCpopoverstack != nullptr)
                {
                    GdkWindow *t_popover_gdk = MCLinuxPopoverGetGdkWindow();
                    bool t_inside = false;
                    if (t_popover_gdk != nullptr)
                    {
                        GdkWindow *t_w = t_event->button.window;
                        while (t_w != nullptr)
                        {
                            if (t_w == t_popover_gdk)
                            {
                                t_inside = true;
                                break;
                            }
                            t_w = gdk_window_get_parent(t_w);
                        }
                    }
                    if (!t_inside)
                    {
                        Window t_popover_win = MCpopoverstack->getwindowalways();
                        GdkDisplay *t_dpy    = gdk_window_get_display(t_popover_win);
                        GdkSeat    *t_seat   = gdk_display_get_default_seat(t_dpy);
                        gdk_seat_ungrab(t_seat);      // no-op if no grab; harmless
                        MCdispatcher->wclose(t_popover_win);
                        // wclose() → MCLinuxPopoverClose() destroys the proxy and
                        // all its GdkWindows.  Do NOT fall through: the event's
                        // window pointer is now dangling.  The dismiss click is
                        // consumed (standard popover behaviour).
                        break;
                    }
                }

                // We're not dragging
                dragclick = false;

                // Update the mouse button status
                if (t_event->type == GDK_BUTTON_PRESS)
                    setmods(t_event->button.state, 0, t_event->button.button, False);
                else
                    setmods(t_event->scroll.state, 0, 0, False);
                
                // IM-2013-08-12: [[ ResIndependence ]] Scale mouse coords to user space
                MCGFloat t_scale;
                t_scale = MCResGetPixelScale();
                
                // NOTE: this depends on the offsets for the x and y positions
                // of the event being in the same place in the GdkEventButton
                // and GdkEventScroll structures.
                MCPoint t_clickloc;
                t_clickloc = MCPointMake(t_event->button.x / t_scale, t_event->button.y / t_scale);
                
                MCStack *t_mousestack;
                t_mousestack = MCdispatcher->findstackd(t_event->motion.window);
                
                // IM-2013-10-09: [[ FullscreenMode ]] Update mouseloc with MCscreen getters & setters
                // FG-2014-09-22: [[ Bugfix 13225 ]] Update the mouse position before the click
                MCscreen->setmouseloc(t_mousestack, t_clickloc);
                
                MCStack *t_old_clickstack;
                MCPoint  t_old_clickloc;
                MCscreen->getclickloc(t_old_clickstack, t_old_clickloc);
                
                // IM-2013-10-09: [[ FullscreenMode ]] Update clicklock with MCscreen getters & setters
                MCscreen->setclickloc(MCmousestackptr, t_clickloc);
                
                // Used for measuring double clicks
                static guint32 clicktime = -1;
                
                if (dispatch)
                {
                    if (t_event->button.window != MCtracewindow)
                    {
                        MCeventtime = t_event->button.time;
                        
                        // Is this a mouse scroll event?
                        if (t_event->type == GDK_SCROLL)
                        {
                            // Determine if a native GTK widget (e.g. WebKitWebView)
                            // should receive this scroll event.
                            //
                            // Scroll events from the trackpad arrive at the engine's
                            // stack window (t_mousestack != NULL) because the stack
                            // window holds GDK_ALL_EVENTS_MASK.  We must detect that
                            // the focused object is a native-layer widget and redirect
                            // the event to its own GdkWindow so GTK/WebKit handles it.
                            //
                            // If somehow the event arrived at a non-stack window
                            // (t_mousestack == NULL) we still forward via GTK.
                            GtkWidget *t_native_widget = nullptr;
                            // Origin of the found browser rect in stack-window coords.
                            // Set by hxt_find_browser_at; used when forwarding the event.
                            int t_scroll_bx = 0, t_scroll_by = 0;

                            if (!t_mousestack)
                            {
                                // Event window is not an engine stack — find the
                                // GTK widget that owns the GdkWindow.
                                // This path should normally be handled earlier in
                                // EnqueueGdkEvents, but acts as a safety net.
                                gpointer t_wdata = nullptr;
                                gdk_window_get_user_data(t_event->scroll.window, &t_wdata);
                                if (t_wdata != nullptr && GTK_IS_WIDGET(t_wdata))
                                {
                                    GtkWidget *t_w = GTK_WIDGET(t_wdata);
                                    // If the owning widget is a container (e.g. our popup
                                    // GtkWindow), the real scroll target is the first child
                                    // (the WebKitWebView).  GTK event propagation goes UP the
                                    // widget hierarchy, not down, so targeting the container
                                    // never reaches the browser widget.
                                    if (GTK_IS_CONTAINER(t_w))
                                    {
                                        GList *t_kids = gtk_container_get_children(GTK_CONTAINER(t_w));
                                        if (t_kids != nullptr)
                                        {
                                            t_native_widget = GTK_WIDGET(t_kids->data);
                                            g_list_free(t_kids);
                                        }
                                    }
                                    if (t_native_widget == nullptr)
                                        t_native_widget = t_w;
                                }
                            }
                            else if (MCmousestackptr)
                            {
                                // Event arrived at the stack window.  Iterate all
                                // attached browser layers to find whichever one the
                                // pointer is actually over — using focus here would
                                // wrongly redirect scrolls to the focused browser even
                                // when the mouse is over a different one.
                                GtkWidget *t_bw = nullptr;
                                if (hxt_find_browser_at((int)t_event->scroll.x,
                                                        (int)t_event->scroll.y,
                                                        &t_bw,
                                                        &t_scroll_bx, &t_scroll_by))
                                {
                                    t_native_widget = t_bw;
                                }
                            }

                            if (t_native_widget != nullptr)
                            {
                                // Forward scroll to the browser widget using
                                // g_signal_emit_by_name to avoid GTK's window-ancestry
                                // check (the WebView's GdkWindow is non-native and
                                // gtk_widget_event would trigger a GDK X11 warning).
                                // Coordinates are adjusted to be browser-widget-relative.
                                if (gtk_widget_get_realized(t_native_widget))
                                {
                                    GdkEvent *t_fwd = gdk_event_copy(t_event);
                                    t_fwd->scroll.x -= t_scroll_bx;
                                    t_fwd->scroll.y -= t_scroll_by;
                                    gboolean t_ret = FALSE;
                                    g_signal_emit_by_name(t_native_widget, "scroll-event", t_fwd, &t_ret);
                                    gdk_event_free(t_fwd);
                                }
                                t_handled = true;
                                break;
                            }

                            // No native widget — dispatch as key events for LiveCode
                            // objects (existing behaviour).
                            MCObject *mfocused = nullptr;
                            if (MCmousestackptr)
                            {
                                mfocused = MCmousestackptr->getcard()->getmfocused();
                                if (mfocused == nullptr)
                                    mfocused = MCmousestackptr->getcard();
                            }

                            if (mfocused != nullptr)
                            {
                                switch (t_event->scroll.direction)
                                {
                                    // GDK events are named for the 'natural scrolling' version
                                    case GDK_SCROLL_UP:
                                        mfocused->kdown(kMCEmptyString, XK_WheelDown);
                                        break;

                                    case GDK_SCROLL_DOWN:
                                        mfocused->kdown(kMCEmptyString, XK_WheelUp);
                                        break;

                                    case GDK_SCROLL_LEFT:
                                        mfocused->kdown(kMCEmptyString, XK_WheelRight);
                                        break;

                                    case GDK_SCROLL_RIGHT:
                                        mfocused->kdown(kMCEmptyString, XK_WheelLeft);
                                        break;

                                    default:
                                        break;
                                }
                            }
                        }
                        else
                        {
                            // Not a scroll, actually a button press
                            uint16_t t_delay;
                            
                            if (t_event->button.time < clicktime) /* 32-bit wrap */
                                t_delay = (t_event->button.time + 10000) - (clicktime + 10000);
                            else
                                t_delay = t_event->button.time - clicktime;
                            
                            clicktime = t_event->button.time;
                            
                            // Was the click on the background window?
                            if (backdrop != DNULL && t_event->button.window == backdrop)
                                MCdefaultstackptr->getcard()->message_with_args(MCM_mouse_down_in_backdrop, t_event->button.button);
                            else
                            {
                                // MM-2013-09-16: [[ Bugfix 11176 ]] Make sure we calculate the y delta correctly.
                                if (t_delay < MCdoubletime
                                    && MCU_abs(t_old_clickloc.x - t_clickloc.x) < MCdoubledelta
                                    && MCU_abs(t_old_clickloc.y - t_clickloc.y) < MCdoubledelta)
                                {
                                    // If we've already detected a double-click,
                                    // this must be a treble-click event.
                                    if (doubleclick)
                                    {
                                        doubleclick = False;
                                        tripleclick = True;
                                        hxt_browser_reset_mousedown_flag();
                                        MCdispatcher->wmdown(t_event->button.window, t_event->button.button);
                                        if (MCactivefield && !hxt_browser_mousedown_fired())
                                            hxt_browser_clear_focus();
                                    }
                                    else
                                    {
                                        // This is a double-click event
                                        doubleclick = True;
                                        MCdispatcher->wdoubledown(t_event->button.window, t_event->button.button);
                                    }
                                    
                                    reset = True;
                                }
                                else
                                {
                                    doubleclick = tripleclick = false;
                                    
                                    // XFCE workaround: Limit wmfocus on button press on XFCE
                                    bool skip_click_focus = false;
                                    const char *desktop = getenv("XDG_CURRENT_DESKTOP");
                                    if (desktop && (strstr(desktop, "XFCE") || strstr(desktop, "xfce")))
                                    {
                                        MCStack *t_stack = MCdispatcher->findstackd(t_event->button.window);
                                        // Only send wmfocus for top-level windows
                                        if (t_stack && t_stack->getrealmode() != WM_TOP_LEVEL && t_stack->getrealmode() != WM_TOP_LEVEL_LOCKED)
                                            skip_click_focus = true;
                                    }
                                    
                                    if (!skip_click_focus)
                                        MCdispatcher->wmfocus(t_event->button.window, t_clickloc.x, t_clickloc.y);
                                    
                                    hxt_browser_reset_mousedown_flag();
                                    MCdispatcher->wmdown(t_event->button.window, t_event->button.button);

                                    // If wmdown() caused an HXT text field to take
                                    // focus, clear browser keyboard focus so keys
                                    // route to HXT rather than WebKit.
                                    // Guard: if OnMouseDown fired during wmdown, the
                                    // click was on the browser widget — don't undo it.
                                    if (MCactivefield && !hxt_browser_mousedown_fired())
                                    {
                                        hxt_browser_clear_focus();
                                    }
                                }
                            }
                        }
                    }
                }
                else
                {
                    t_queue = true;
                }

                t_handled = true;
                break;
            }

            case GDK_BUTTON_RELEASE:
            {
                // No longer in a drag-and-drop situation
                dragclick = false;
                
                // Update the current button state
                setmods(t_event->button.state, 0, t_event->button.button, True);
                
                if (dispatch)
                {
                    if (backdrop != DNULL && t_event->button.window == backdrop)
                    {
                        // Don't send mouse events to the backdrop
                        MCdefaultstackptr->getcard()->message_with_args(MCM_mouse_up_in_backdrop, t_event->button.button);
                    }
                    else
                    {
                        if (t_event->button.window != MCtracewindow)
                        {
                            MCeventtime = t_event->button.time;
                            if (doubleclick)
                                MCdispatcher->wdoubleup(t_event->button.window, t_event->button.button);
                            else
                                MCdispatcher->wmup(t_event->button.window, t_event->button.button);
                            reset = True;
                        }
                    }
                }
                else
                {
                    t_queue = true;
                }
                
                t_handled = true;
                break;
            }
                
            // This replaces the need to check for state changes in the property
            // notify handler.
            case GDK_WINDOW_STATE:
            {
                // Which window underwent a state change?
                MCeventtime = gdk_event_get_time(t_event);
                MCStack *t_target = MCdispatcher->findstackd(t_event->window_state.window);
                if (t_target != NULL)
                {
                    // Which state flags changed?
                    if (t_event->window_state.changed_mask & GDK_WINDOW_STATE_ICONIFIED)
                    {
                        // Was the iconified flag set or cleared?
                        if (t_event->window_state.new_window_state & GDK_WINDOW_STATE_ICONIFIED)
                            t_target->iconify();
                        else
                            t_target->uniconify();
                    }
                }
                
                t_handled = true;
                break;
            }
                
            case GDK_PROPERTY_NOTIFY:
                // No longer required - only monitored for window state changes
                // which GDK provides more explicit events for.
                break;
                
            case GDK_CONFIGURE:
            {
                // Window geometry has changed
                // We may need to handle window geometry limits ourselves
                MCStack *t_stack = MCdispatcher->findstackd(t_event->configure.window);
                if (t_stack == nil)
                    break;
                
                GdkGeometry t_geom;
                gint t_new_width, t_new_height;
                // -- tperry 13-11-2025: GTK3 - cast flags to GdkWindowHints
                GdkWindowHints t_flags = (GdkWindowHints)(GDK_HINT_MIN_SIZE|GDK_HINT_MAX_SIZE);
 
                t_geom.min_width = t_stack->getminwidth();
                t_geom.max_width = t_stack->getmaxwidth();
                t_geom.min_height = t_stack->getminheight();
                t_geom.max_height = t_stack->getmaxheight();
                
                gdk_window_constrain_size(&t_geom, t_flags,
                                         t_event->configure.width, t_event->configure.height,
                                         &t_new_width, &t_new_height);
                
                if (t_new_width != t_event->configure.width || t_new_height != t_event->configure.height)
                {
                    gdk_window_unmaximize(t_event->configure.window);
                    gdk_window_resize(t_event->configure.window, t_new_width, t_new_height);
                }                        
                
                MCdispatcher->wreshape(t_event->configure.window);
                break;
            }
                
            case GDK_CLIENT_EVENT:
                // Hmm - do we still need to react to any of these?
                break;
                
            case GDK_SELECTION_CLEAR:
            {
                // Tell the appropriate clipboard that it lost ownership
                MCLinuxRawClipboard* t_clipboard = NULL;
                if (t_event->selection.selection == GDK_SELECTION_PRIMARY)
                    t_clipboard = static_cast<MCLinuxRawClipboard*> (MCselection->GetRawClipboard());
                else if (t_event->selection.selection == GDK_SELECTION_CLIPBOARD)
                    t_clipboard = static_cast<MCLinuxRawClipboard*> (MCclipboard->GetRawClipboard());
                else if (t_event->selection.selection == MCdndselectionatom)
                    t_clipboard = static_cast<MCLinuxRawClipboard*> (MCdragboard->GetRawClipboard());
                if (t_clipboard)
                    t_clipboard->LostSelection();
                
                if (t_event->selection.time != MCeventtime)
                {
                    // Clear the active selection
                    if (MCactivefield)
                        MCactivefield->unselect(False, False);
                }
                
                break;
            }
                
            case GDK_SELECTION_NOTIFY:
                // Handled as a drag-and-drop event
                DnDClientEvent(t_event);
                break;
                
            case GDK_SELECTION_REQUEST:
            {
                // Get the clipboard associated with the requested selection
                // Checking for ownership is unreliable in GDK so don't bother
                // -- we just fulfil the request anyway.
                MCLinuxRawClipboard* t_clipboard;
				if (t_event->selection.selection == GDK_SELECTION_PRIMARY)
                    t_clipboard = static_cast<MCLinuxRawClipboard*> (MCselection->GetRawClipboard());
				else if (t_event->selection.selection == GDK_SELECTION_CLIPBOARD)
                    t_clipboard = static_cast<MCLinuxRawClipboard*> (MCclipboard->GetRawClipboard());
                else if (t_event->selection.selection == MCdndselectionatom)
                    t_clipboard = static_cast<MCLinuxRawClipboard*> (MCdragboard->GetRawClipboard());
                else
                    t_clipboard = NULL;
                
                // Note: we don't use a secondary selection
                if (t_clipboard != NULL)
                {
                    // -- tperry 13-11-2025: GTK3 - requestor is already a GdkWindow*, not an XID
                    // Get the requestor window
                    GdkWindow *t_requestor;
                    t_requestor = t_event->selection.requestor;
                    
                    // -- tperry 16-11-2025: Check if requestor is valid (can be NULL or destroyed)
                    if (t_requestor == NULL || !GDK_IS_WINDOW(t_requestor))
                    {
                        // Requestor window is invalid, ignore this selection request
                        break;
                    }
                    
                    // There is a backwards-compatibility issue with the way the
                    // ICCCM deals with selections: older clients can request a
                    // selection but not supply a property name. In that case,
                    // the property set should be equal to the target name.
                    //
                    // The GDK manual does not say whether it works around this
                    // wrinkle so we might as well check ourselves.
                    GdkAtom t_property;
                    if (t_event->selection.property != GDK_NONE)
                        t_property = t_event->selection.property;
                    else
                        t_property = t_event->selection.target;
                    
                    // What type should the selection be converted to?
                    static GdkAtom s_targets = gdk_atom_intern_static_string("TARGETS");
                    static GdkAtom s_multiple = gdk_atom_intern_static_string("MULTIPLE");
                    static GdkAtom s_timestamp = gdk_atom_intern_static_string("TIMESTAMP");
                    if (t_event->selection.target == s_targets)
                    {
                        // Get the list of types we can convert to
                        MCAutoDataRef t_targets(t_clipboard->CopyTargets());
                        
                        if (*t_targets != NULL)
                        {
                            // Set a property on the requestor containing the
                            // list of targets we can convert to.
                            uindex_t t_atom_count = MCDataGetLength(*t_targets)/sizeof(gulong);
                            gdk_property_change(t_requestor, t_property,
                                                GDK_SELECTION_TYPE_ATOM,
                                                32,
                                                GDK_PROP_MODE_REPLACE,
                                                (const guchar*)MCDataGetBytePtr(*t_targets),
                                                t_atom_count);
                            
                            // Notify the requestor that we have replied
                            gdk_selection_send_notify(t_event->selection.requestor,
                                                      t_event->selection.selection,
                                                      t_event->selection.target,
                                                      t_property,
                                                      t_event->selection.time);
                        }
                        else
                        {
                            // We don't actually have anything to supply so
                            // reject the request without supplying any data
                            gdk_selection_send_notify(t_event->selection.requestor,
                                                      t_event->selection.selection,
                                                      t_event->selection.target,
                                                      GDK_NONE,
                                                      t_event->selection.time);
                        }
                    }
                    else if (t_event->selection.target == s_multiple)
                    {
                        // This should be handled by GDK
                        MCAssert(false);
                    }
                    else if (t_event->selection.target == s_timestamp)
                    {
                        // This should be handled by GDK
                        MCAssert(false);
                    }
                    else
                    {
                        // Turn the requested selection into a string
                        MCAutoStringRef t_atom_string(MCLinuxRawClipboard::CopyTypeForAtom(t_event->selection.target));
                        
                        // Get the requested representation of the data
                        const MCRawClipboardItemRep* t_rep = NULL;
                        MCAutoRefcounted<const MCLinuxRawClipboardItem> t_item = t_clipboard->GetSelectionItem();
                        if (t_item != NULL)
                            t_rep = t_item->FetchRepresentationByType(*t_atom_string);
                        
                        // Get the data in the requested form
                        MCAutoDataRef t_data;
                        if (t_rep != NULL)
                            t_data.Give(t_rep->CopyData());
                        
                        if (*t_data != NULL)
                        {
                            // Transfer the data to the requestor via the
                            // property that it specified
                            gdk_property_change(t_requestor, t_property,
                                                t_event->selection.target,
                                                8,
                                                GDK_PROP_MODE_REPLACE,
                                                (const guchar*)MCDataGetBytePtr(*t_data),
                                                MCDataGetLength(*t_data));
                            
                            // Notify the requestor that we have replied
                            gdk_selection_send_notify(t_event->selection.requestor,
                                                      t_event->selection.selection,
                                                      t_event->selection.target,
                                                      t_property,
                                                      t_event->selection.time);
                        }
                        else
                        {
                            // Could not convert the data to the format that was
                            // requested - reject the request.
                            gdk_selection_send_notify(t_event->selection.requestor,
                                                      t_event->selection.selection,
                                                      t_event->selection.target,
                                                      GDK_NONE,
                                                      t_event->selection.time);
                        }
                    }
                    
                    // We don't need the requestor window handle any longer
                    g_object_unref(t_requestor);
                }
                
                break;
            }
            
            case GDK_DRAG_ENTER:
            case GDK_DRAG_LEAVE:   
            case GDK_DRAG_MOTION:  
            case GDK_DRAG_STATUS:
            case GDK_DROP_START:
            case GDK_DROP_FINISHED:
                DnDClientEvent(t_event);
                break;

            default:
                // Any other event types are ignored
                break;
        }
        
        // Flush all pending messages to X11
        gdk_display_flush(dpy);
        
        // Queue the message if required. Otherwise, dispose of it
        if (t_queue)
        {
            MCEventnode *tptr = new (nothrow) MCEventnode(t_event);
            tptr->appendto(pendingevents);
            t_event = NULL;
        }
        else if (t_event != NULL)
        {
            // Use safe_gdk_event_free to avoid crash on destroyed GObjects
            safe_gdk_event_free(t_event);
            t_event = NULL;
        }
    }
    
    return t_handled;
}

void MCScreenDC::waitmessage(GdkWindow* w, int event_type)
{
	// Does nothing
}


GdkAtom MCworkareaatom;
GdkAtom MCstrutpartialatom;
GdkAtom MCclientlistatom;
GdkAtom MCdndselectionatom;


void MCScreenDC::EnqueueGdkEvents(bool p_block)
{
    while (true)
    {
        // Run the GLib main loop. We only block for the first iteration.
        //gdk_threads_leave();
        while (g_main_context_iteration(NULL, p_block))
            p_block = false;
        //gdk_threads_enter();

        // Enqueue any further GDK events
        GdkEvent *t_event = gdk_event_get();
        if (t_event == NULL)
            break;

        // GTK hasn't had a chance at this event yet
        //gtk_main_do_event(t_event);

        // GDK_SCROLL events from embedded native widgets (e.g. WebKitWebView)
        // arrive at a GdkWindow that is NOT an engine stack window.  Dispatch
        // them directly through GTK rather than enqueueing for the engine.
        //
        // Important: the event window may be:
        //   (a) WebKit's own sub-GdkWindow  → dispatch directly to that widget
        //   (b) m_child_window (our popup GtkWindow) → GtkWindow does NOT
        //       propagate scroll downward to children, so we must drill into
        //       the first child (the browser widget) and target it explicitly.
        if (t_event->type == GDK_SCROLL &&
            MCdispatcher->findstackd(t_event->any.window) == NULL)
        {
            gpointer t_wd = nullptr;
            gdk_window_get_user_data(t_event->any.window, &t_wd);

            if (t_wd != nullptr && GTK_IS_WINDOW(t_wd))
            {
                // Case (b): event at a GtkWindow container.
                // Target its first child (the browser widget) directly.
                GList *t_kids = gtk_container_get_children(GTK_CONTAINER(t_wd));
                if (t_kids != nullptr)
                {
                    GtkWidget  *t_child     = GTK_WIDGET(t_kids->data);
                    GdkWindow  *t_child_win = gtk_widget_get_window(t_child);
                    if (t_child_win != nullptr && gtk_widget_get_realized(t_child))
                    {
                        gint t_ox = 0, t_oy = 0, t_dx = 0, t_dy = 0;
                        gdk_window_get_origin(t_event->any.window, &t_ox, &t_oy);
                        gdk_window_get_origin(t_child_win,          &t_dx, &t_dy);
                        GdkEvent *t_fwd = gdk_event_copy(t_event);
                        t_fwd->scroll.x += t_ox - t_dx;
                        t_fwd->scroll.y += t_oy - t_dy;
                        g_object_ref(t_child_win);
                        g_object_unref(t_fwd->any.window);
                        t_fwd->any.window = t_child_win;
                        gtk_widget_event(t_child, t_fwd);
                        gdk_event_free(t_fwd);
                    }
                    else
                    {
                        // Child not yet realized; best-effort via GTK routing.
                        gtk_main_do_event(t_event);
                    }
                    g_list_free(t_kids);
                }
                else
                {
                    gtk_main_do_event(t_event);
                }
            }
            else if (t_wd != nullptr && GTK_IS_WIDGET(t_wd))
            {
                // Case (a): event window has its own GTK widget — dispatch directly.
                gtk_widget_event(GTK_WIDGET(t_wd), t_event);
            }
            else
            {
                gtk_main_do_event(t_event);
            }

            gdk_event_free(t_event);
            continue;
        }

        // Route ALL other events for non-HXT windows (e.g. GtkMenu popup,
        // GtkOffscreenWindow) via gtk_main_do_event so GTK can deliver them
        // to the correct widget.  Without this, GDK_ENTER_NOTIFY /
        // GDK_LEAVE_NOTIFY / GDK_MOTION_NOTIFY for the <select> popup menu
        // are consumed by HXT's event processor (which finds no matching stack
        // and drops them), leaving menu item hover highlights permanently stuck.
        if (t_event->any.window != NULL &&
            MCdispatcher->findstackd(t_event->any.window) == NULL)
        {
            gtk_main_do_event(t_event);
            gdk_event_free(t_event);
            continue;
        }

        // Redirect GDK_SCROLL events on HXT stack windows to whichever browser
        // widget the scroll position falls within.  Iterates all attached native
        // layers via hxt_find_browser_at so that multiple browsers are supported.
        // This ensures WebKit receives scroll events even though it lives in an
        // offscreen GtkOffscreenWindow that is not an X11 child of the stack window.
        if (t_event->type == GDK_SCROLL && t_event->any.window != NULL)
        {
            GtkWidget *t_bw = NULL;
            int t_sx = 0, t_sy = 0;
            gdouble t_ex = t_event->scroll.x;
            gdouble t_ey = t_event->scroll.y;
            if (hxt_find_browser_at((int)t_ex, (int)t_ey, &t_bw, &t_sx, &t_sy))
            {
                if (gtk_widget_get_realized(t_bw))
                {
                    GdkEvent *t_fwd = gdk_event_copy(t_event);
                    t_fwd->scroll.x = t_ex - t_sx;
                    t_fwd->scroll.y = t_ey - t_sy;
                    // Emit scroll-event directly on the browser widget rather than
                    // going through gtk_widget_event(): the latter does a window-ancestry
                    // check that fails here (the WebView's GdkWindow is a non-native
                    // client-side window and GDK would warn "not a native X11 window"
                    // if we tried to swap any.window to it).  Emitting the signal
                    // directly bypasses that check while still delivering the event.
                    gboolean t_handled = FALSE;
                    g_signal_emit_by_name(t_bw, "scroll-event", t_fwd, &t_handled);
                    gdk_event_free(t_fwd);
                }
                gdk_event_free(t_event);
                continue;
            }
        }

        // Skip synthetic key events that leaked back from our own dispatch.
        // dispatchKeyEvent() in native-layer-x11.cpp creates GdkEventKey
        // structs with send_event=TRUE and dispatches them via
        // g_signal_emit_by_name().  GDK's offscreen-window event-routing
        // machinery can re-inject these into the GDK queue with the event
        // window set to the HXT stack window.  Without this guard they would
        // be processed a second time, causing Tab to advance two DOM elements
        // instead of one.  Real X11 keyboard events always have send_event=FALSE.
        if ((t_event->type == GDK_KEY_PRESS || t_event->type == GDK_KEY_RELEASE)
            && t_event->any.send_event)
        {
            gdk_event_free(t_event);
            continue;
        }

        MCEventnode *t_eventnode = new (nothrow) MCEventnode(t_event);
        t_eventnode->appendto(pendingevents);
    }
}

bool MCScreenDC::GetFilteredEvent(bool (*p_filterfn)(GdkEvent*, void*), GdkEvent* &r_event, void *p_context, bool p_may_block)
{
    // Gather all events into the pending events queue. Because we are looking
    // for a particular event, we can allow blocking until it arrives if the
    // caller desires it.
    EnqueueGdkEvents(p_may_block);
    
    MCEventnode *t_eventnode = pendingevents;
    while (t_eventnode != NULL)
    {
        if (p_filterfn(t_eventnode->event, p_context))
        {
            r_event = gdk_event_copy(t_eventnode->event);
            t_eventnode = t_eventnode->remove(pendingevents);
            delete t_eventnode;
            return true;
        }
        
        // Remember that the list is circular
        if (t_eventnode->next() == pendingevents)
            t_eventnode = NULL;
        else
            t_eventnode = t_eventnode->next();
    }
    
    return false;
}

void MCScreenDC::EnqueueEvent(GdkEvent* p_event)
{
    MCEventnode *t_node = new (nothrow) MCEventnode(p_event);
    t_node->appendto(pendingevents);
}

void MCScreenDC::IME_OnCommit(GtkIMContext*, gchar *p_utf8_string)
{
    MCAutoStringRef t_text;
    /* UNCHECKED */ MCStringCreateWithBytes((byte_t*)p_utf8_string, strlen(p_utf8_string), kMCStringEncodingUTF8, false, &t_text);
    
    if (MCStringGetLength(*t_text) == 1)
    {
        if (MCStringIsNative(*t_text))
            MCdispatcher->wkdown(MCactivefield->getstack()->getwindow(), *t_text, MCStringGetCodepointAtIndex(*t_text, 0));
        else
            MCdispatcher->wkdown(MCactivefield->getstack()->getwindow(), *t_text, MCStringGetCodepointAtIndex(*t_text, 0)|XK_Class_codepoint);
    }
    else
    {
        // Insert the text from the IME into the active field
        MCactivefield->stopcomposition(True, False);
        MCactivefield->finsertnew(FT_IMEINSERT, *t_text, LCH_UNICODE);
    }
}

bool MCScreenDC::IME_OnDeleteSurrounding(GtkIMContext*, gint p_offset, gint p_length)
{
    return false;
}

void MCScreenDC::IME_OnPreeditChanged(GtkIMContext* p_context)
{
    if (!MCactivefield)
        return;
    
    // Get the string. We ignore the attributes list entirely.
    gchar *t_utf8_string;
    gint t_cursor_pos;
    gtk_im_context_get_preedit_string(p_context, &t_utf8_string, NULL, &t_cursor_pos);
    
    MCAutoStringRef t_string;
    /* UNCHECKED */ MCStringCreateWithBytes((byte_t*)t_utf8_string, strlen(t_utf8_string), kMCStringEncodingUTF8, false, &t_string);
    g_free(t_utf8_string);
    
    // Do the insert
    MCactivefield->startcomposition();
    MCactivefield->finsertnew(FT_IMEINSERT, *t_string, LCH_UNICODE);
    
    // Update the cursor position
    MCactivefield->setcompositioncursoroffset(t_cursor_pos);
}

void MCScreenDC::IME_OnPreeditEnd(GtkIMContext*)
{
    if (!MCactivefield)
        return;
    
    MCactivefield->stopcomposition(True, False);
}

void MCScreenDC::IME_OnPreeditStart(GtkIMContext*)
{
    if (!MCactivefield)
        return;
    
    MCactivefield->startcomposition();
}

void MCScreenDC::IME_OnRetrieveSurrounding(GtkIMContext*)
{
    ;
}

void MCScreenDC::clearIME(Window w)
{
    if (!m_has_gtk)
        return;
    
    gtk_im_context_reset(m_im_context);
}

void MCScreenDC::activateIME(Boolean activate)
{
    if (!m_has_gtk)
        return;
    
    // SN-2015-04-22: [[ Bug 14994 ]] Ensure that there is an activeField
    //  before starting the IME in it.
    if (activate && MCactivefield)
    {
        gtk_im_context_set_client_window(m_im_context, MCactivefield->getstack()->getwindow());
        gtk_im_context_focus_in(m_im_context);
        
        if (MCinlineinput)
            gtk_im_context_set_use_preedit(m_im_context, TRUE);
        else
            gtk_im_context_set_use_preedit(m_im_context, FALSE);
    }
    else
    {
        gtk_im_context_focus_out(m_im_context);
    }
}

void MCScreenDC::configureIME(int32_t x, int32_t y)
{
    if (!m_has_gtk)
        return;
    
    GdkRectangle t_cursor;
    t_cursor.x = x;
    t_cursor.y = y;
    t_cursor.width = t_cursor.height = 1;
    
    gtk_im_context_set_cursor_location(m_im_context, &t_cursor);
}

void init_xDnD()
{
    ;
}

void MCScreenDC::DnDClientEvent(GdkEvent* p_event)
{
    switch (p_event->type)
    {
        case GDK_EXPOSE:
        case GDK_DAMAGE:
        {
            // Handled separately
            //fprintf(stderr, "GDK_EXPOSE (window %p)\n", t_event->expose.window);
            MCEventnode *t_node = new (nothrow) MCEventnode(gdk_event_copy(p_event));
            t_node->appendto(pendingevents);
            expose();
            break;
        }
        
        case GDK_DRAG_ENTER:
        {
            //fprintf(stderr, "DND: drag enter\n");
            // Temporarily set the modifier state to the asynchronous state
            uint16_t t_old_modstate = MCmodifierstate;
            MCmodifierstate = MCscreen->querymods();
            
            // Ensure our dragboard ownership info is up-to-date
            MCLinuxRawClipboard* t_dragboard = static_cast<MCLinuxRawClipboard*>(MCdragboard->GetRawClipboard());
            if (!MCdispatcher->isdragsource())
               t_dragboard->LostSelection();
            t_dragboard->SetDragContext(p_event->dnd.context);
            
            // We use the destination window as the clipboard window for drag-
            // and-drop operations from outside LiveCode as some sources get
            // confused when the window requesting the data != the drag target
            // window.
            if (!MCdispatcher->isdragsource())
                t_dragboard->SetClipboardWindow(p_event->dnd.window);
            
            // Handle the event
            MCdispatcher->wmdragenter(p_event->dnd.window);
            
            // Also perform a motion so that we have some status to return. If
            // we don't do this, some drag sources will get confused.
            
            // Fall through to the GDK_DRAG_MOTION_CASE
        }
            
        case GDK_DRAG_MOTION:
        {
            //fprintf(stderr, "DND: drag motion\n");
            // Translate the position from root to relative coordinates
            uint32_t wx, wy;    // Window-relative coordinates
            gint ox, oy;        // Window origin in root coordinates
            gdk_window_get_origin(p_event->dnd.window, &ox, &oy);
            wx = p_event->dnd.x_root - ox;
            wy = p_event->dnd.y_root - oy;
            
            // Temporarily adopt the asynchronous modifier state
            uint16_t t_old_modstate = MCmodifierstate;
            MCmodifierstate = MCscreen->querymods();
            
            // Handle the event
            MCDragActionSet t_action;
            t_action = MCdispatcher->wmdragmove(p_event->dnd.window, wx, wy);
            
            // Restore modifier state
            MCmodifierstate = t_old_modstate;
            
            // Convert the selected drag action to the corresponding GDK value
            GdkDragAction t_gdk_action = GdkDragAction(0);
            if (t_action == DRAG_ACTION_COPY)
                t_gdk_action = GDK_ACTION_COPY;
            else if (t_action == DRAG_ACTION_MOVE)
                t_gdk_action = GDK_ACTION_MOVE;
            else if (t_action == DRAG_ACTION_LINK)
                t_gdk_action = GDK_ACTION_LINK;
            
            // Reply to the motion event
            gdk_drag_status(p_event->dnd.context, t_gdk_action, GDK_CURRENT_TIME);
            break;
        }
            
        case GDK_DRAG_LEAVE:
        {
            //fprintf(stderr, "DND: drag leave\n");
            // The drag is no longer relevant to us
            MCdispatcher->wmdragleave(p_event->dnd.window);
            static_cast<MCLinuxRawClipboard*>(MCdragboard->GetRawClipboard())->SetDragContext(NULL);
            MCdragboard->FlushData();
            break;
        }
            
        case GDK_DRAG_STATUS:
            // Only sent while we are in a D&D loop so shouldn't happen
            break;
            
        case GDK_DROP_START:
        {
            //fprintf(stderr, "DND: drop start\n");
            // GDK fires a synthetic GDK_DRAG_LEAVE immediately before
            // GDK_DROP_START, which wipes the dragboard (ReleaseData clears
            // m_item; SetDragContext(NULL) clears the context). Re-initialise
            // both so that dragData["files"] etc. can be read from inside the
            // dragDrop handler. The X11 selection data remains available from
            // the drop source until we call gdk_drop_finish below.
            if (!MCdispatcher->isdragsource())
            {
                MCLinuxRawClipboard* t_dragboard =
                    static_cast<MCLinuxRawClipboard*>(MCdragboard->GetRawClipboard());
                t_dragboard->SetDragContext(p_event->dnd.context);
                MCdragboard->PullUpdates();
            }

            // Temporarily adopt the asynchronous modifier state
            uint16_t t_old_modstate = MCmodifierstate;
            MCmodifierstate = MCscreen->querymods();

            // Something was dropped on us
            MCdispatcher->wmdragdrop(p_event->dnd.window);
            
            // Restore the modifier state
            MCmodifierstate = t_old_modstate;
            
            // Tell the source that we are now finished with it
            gdk_drop_finish(p_event->dnd.context, TRUE, p_event->dnd.time);
            break;
        }
            
        case GDK_DROP_FINISHED:
            // Only sent while we are in a D&D loop so shouldn't happen
            break;
    }
}
