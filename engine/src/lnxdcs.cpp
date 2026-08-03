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
// ScreenDC display specific functions
//

#include "lnxprefix.h"

#include "globdefs.h"

// -- tperry 15-11-2025: GTK2 theme engine initialization removed for GTK3
// GTK3 uses CSS for theming, not RC files or theme engines
// extern "C" void init_static_theme_engines();
#include "filedefs.h"
#include <signal.h>
#include <setjmp.h>
#include <stdarg.h>
#include "objdefs.h"
#include "parsedef.h"

#include "dispatch.h"
#include "image.h"
#include "stack.h"
#include "util.h"

#include "stacklst.h"

#include "sellst.h"

#include "globals.h"

#include "mctheme.h"
#include "platform.h"

#include "lnxdc.h"
#include "lnximagecache.h"

// File-static extra backdrop windows (monitors 1..N).  Kept here rather than
// in MCScreenDC to avoid changing the class layout (which would require a full
// rebuild of all TUs that include lnxdc.h).
#include <vector>
#include <algorithm>

static std::vector<GdkWindow*> s_extra_backdrops;
// Mirror of MCScreenDC::backdrop kept in sync so hxt_is_backdrop_window can
// check it without needing a full MCScreenDC* cast.
static GdkWindow *s_primary_backdrop = nullptr;

// Some WMs treat _NET_WM_STATE_ABOVE ClientMessages as "aggressively re-raise
// above everything on every click-to-raise event", which pushes HXT stacks
// behind the backdrop.  Known offenders: Muffin (Cinnamon) and Marco (MATE /
// Metacity).  On Mutter (GNOME/Ubuntu) the ClientMessage is required to
// maintain ABOVE layer membership after gdk_window_lower().
// Detect once at startup and branch accordingly.
// Variable kept as s_wm_is_muffin for brevity; it covers Marco too.
// Not static: lnxdclnx.cpp references it via extern.
bool s_wm_is_muffin = false;
// True only for Marco/Metacity (MATE).  Muffin (Cinnamon) is a Mutter fork
// and handles extra backdrop stacking correctly, so spanning-window workarounds
// must not activate there.
static bool s_wm_is_marco  = false;

// GObject-ref'd list of HXT stack windows currently registered with the
// backdrop via assignbackdrop().  Used by backdrop_focus_gained/lost() to
// batch-toggle keep_above so ALT+TAB can bring other apps to the front.
static std::vector<GdkWindow*> s_backdrop_stacks;

// Free function used by lnxdclnx.cpp to test any backdrop window.
bool hxt_is_backdrop_window(GdkWindow *w)
{
    if (!w) return false;
    if (w == s_primary_backdrop) return true;
    for (GdkWindow *b : s_extra_backdrops) if (b == w) return true;
    return false;
}

// Lower all backdrop windows within the ABOVE layer.
// HXT stacks are transient children of the backdrop, so Mutter atomically
// keeps them above the backdrop regardless of where the backdrop sits in the layer.
// Called from button-press on backdrop and the deferred backdrop show.
static void hxt_lower_backdrops()
{
    if (s_primary_backdrop && gdk_window_is_visible(s_primary_backdrop))
        gdk_window_lower(s_primary_backdrop);
    for (GdkWindow *bd : s_extra_backdrops)
        if (bd && gdk_window_is_visible(bd))
            gdk_window_lower(bd);
}

// Re-entry guard: true while a withdraw+remap cycle is in progress.
// Prevents remap-triggered GDK_CONFIGURE events from starting a second cycle.
static bool s_muffin_in_remap = false;

// GLib source ID for the stacking-order polling timer (0 = not running).
static guint s_stacking_timer_id = 0;

// GLib idle callback (very low priority) that clears the remap guard.
// Running at G_PRIORITY_LOW+10 ensures all pending X11 events on the I/O
// channel (default priority 0) are dispatched before this clears the flag.
static gboolean hxt_muffin_clear_remap_flag(gpointer)
{
    s_muffin_in_remap = false;
    return G_SOURCE_REMOVE;
}

// Returns the root-relative X11 position of a managed (reparented) window.
// gdk_window_get_position() returns client-area position relative to the WM
// frame, which is 0 or a tiny border offset — not screen coordinates.
// We use XTranslateCoordinates to get the true root-relative position.
static void hxt_get_root_pos(GdkWindow *p_win, int *rx, int *ry)
{
    x11::Display *t_xdpy = x11::gdk_x11_display_get_xdisplay(MCdpy);
    x11::Window   t_xwin = x11::gdk_x11_window_get_xid(p_win);
    x11::Window   t_root = x11::gdk_x11_window_get_xid(gdk_screen_get_root_window(
                               gdk_display_get_default_screen(MCdpy)));
    x11::Window t_child_ret;
    int lx = 0, ly = 0;
    x11::XTranslateCoordinates(t_xdpy, t_xwin, t_root, 0, 0, &lx, &ly, &t_child_ret);
    int t_scale = gdk_window_get_scale_factor(p_win);
    *rx = lx / (t_scale ? t_scale : 1);
    *ry = ly / (t_scale ? t_scale : 1);
}

// Returns true if p_win overlaps any extra (external-monitor) backdrop.
// Uses XTranslateCoordinates for root-relative positions (gdk_window_get_position
// returns frame-relative coordinates for reparented toplevel windows).
static bool hxt_overlaps_extra_backdrop(GdkWindow *p_win)
{
    if (s_extra_backdrops.empty()) return false;
    int wx, wy;
    hxt_get_root_pos(p_win, &wx, &wy);
    int ww = gdk_window_get_width(p_win);
    int wh = gdk_window_get_height(p_win);
    for (GdkWindow *bd : s_extra_backdrops)
    {
        if (!bd || !gdk_window_is_visible(bd)) continue;
        int bx, by;
        hxt_get_root_pos(bd, &bx, &by);
        int bw = gdk_window_get_width(bd);
        int bh = gdk_window_get_height(bd);
        if (wx < bx + bw && wx + ww > bx && wy < by + bh && wy + wh > by)
            return true;
    }
    return false;
}

// Raises p_win above any extra backdrop on Muffin (Cinnamon) by sending
// _NET_RESTACK_WINDOW to root.  Only triggers when p_win overlaps an extra
// backdrop (external monitor) to avoid unnecessary restacks on the primary.
// On Marco this function is a no-op since s_extra_backdrops is always empty
// (the spanning-window approach is used there instead).
void hxt_raise_above_backdrops(GdkWindow *p_win)
{
    if (!p_win || !s_wm_is_muffin || s_extra_backdrops.empty()) return;
    if (!gdk_window_is_visible(p_win)) return;
    if (!hxt_overlaps_extra_backdrop(p_win)) return;
    if (!MCdpy) return;
    x11::Display *t_xdpy = x11::gdk_x11_display_get_xdisplay(MCdpy);
    x11::Window   t_root = x11::gdk_x11_window_get_xid(
        gdk_screen_get_root_window(gdk_display_get_default_screen(MCdpy)));
    x11::Window   t_xwin = x11::gdk_x11_window_get_xid(p_win);
    x11::Atom t_wm_state = x11::XInternAtom(t_xdpy, "_NET_WM_STATE", False);
    x11::Atom t_above    = x11::XInternAtom(t_xdpy, "_NET_WM_STATE_ABOVE", False);
    x11::Atom t_restack  = x11::XInternAtom(t_xdpy, "_NET_RESTACK_WINDOW", False);

    // Step 1: ADD _NET_WM_STATE_ABOVE via raw X11 so it arrives at Marco
    // in the same X11 stream as step 2 and is processed first.
    {
        x11::XEvent t_ev = {};
        t_ev.xclient.type         = ClientMessage;
        t_ev.xclient.display      = t_xdpy;
        t_ev.xclient.window       = t_xwin;
        t_ev.xclient.message_type = t_wm_state;
        t_ev.xclient.format       = 32;
        t_ev.xclient.data.l[0]    = 1;            // _NET_WM_STATE_ADD
        t_ev.xclient.data.l[1]    = (long)t_above;
        t_ev.xclient.data.l[3]    = 2;            // source: pager
        x11::XSendEvent(t_xdpy, t_root, False,
                        SubstructureRedirectMask | SubstructureNotifyMask, &t_ev);
    }
    // Step 2: _NET_RESTACK_WINDOW — raises stack_position to max within the
    // ABOVE layer (which was just restored by step 1).
    {
        x11::XEvent t_ev = {};
        t_ev.xclient.type         = ClientMessage;
        t_ev.xclient.display      = t_xdpy;
        t_ev.xclient.window       = t_xwin;
        t_ev.xclient.message_type = t_restack;
        t_ev.xclient.format       = 32;
        t_ev.xclient.data.l[0]    = 2;  // source: pager
        t_ev.xclient.data.l[1]    = 0;  // no sibling (raise to absolute top)
        t_ev.xclient.data.l[2]    = 0;  // detail: Above
        x11::XSendEvent(t_xdpy, t_root, False,
                        SubstructureRedirectMask | SubstructureNotifyMask, &t_ev);
    }
    x11::XFlush(t_xdpy);
}

// GLib timeout callback — runs every 250ms while extra backdrops are active
// on Muffin (Cinnamon).  Detects HXT stacks buried below an extra backdrop
// and sends _NET_RESTACK_WINDOW(Below, sibling=stack) to ask the WM to lower
// the backdrop.  On Marco the spanning-window approach is used instead and
// this timer is never started.
static gboolean hxt_check_stacking(gpointer)
{
    if (s_extra_backdrops.empty() || !s_wm_is_muffin || !MCdpy)
        return G_SOURCE_CONTINUE;

    x11::Display *t_xdpy = x11::gdk_x11_display_get_xdisplay(MCdpy);
    x11::Window   t_root = x11::gdk_x11_window_get_xid(
        gdk_screen_get_root_window(gdk_display_get_default_screen(MCdpy)));

    // Skip during active drag (any mouse button held).
    {
        x11::Window t_qroot, t_qchild;
        int t_qrx = 0, t_qry = 0, t_qwx = 0, t_qwy = 0;
        unsigned int t_qmask = 0;
        x11::XQueryPointer(t_xdpy, t_root, &t_qroot, &t_qchild,
                           &t_qrx, &t_qry, &t_qwx, &t_qwy, &t_qmask);
        if (t_qmask & ((1u<<8)|(1u<<9)|(1u<<10)))
            return G_SOURCE_CONTINUE;
    }

    // Read bottom-to-top client stacking order.
    x11::Atom t_prop = x11::XInternAtom(t_xdpy, "_NET_CLIENT_LIST_STACKING", False);
    x11::Atom t_actual_type;
    int t_fmt;
    unsigned long t_nitems = 0, t_after = 0;
    unsigned char *t_data = nullptr;
    int t_rc = x11::XGetWindowProperty(t_xdpy, t_root, t_prop,
                                       0, 65536, False, (x11::Atom)33,
                                       &t_actual_type, &t_fmt, &t_nitems,
                                       &t_after, &t_data);
    if (t_rc != Success || !t_data)
        return G_SOURCE_CONTINUE;

    x11::Window *t_wins = reinterpret_cast<x11::Window *>(t_data);

    // Find the topmost extra backdrop: its stacking index and GdkWindow pointer.
    long      t_max_bd_idx = -1;
    GdkWindow *t_top_bd_win = nullptr;
    for (unsigned long i = 0; i < t_nitems; i++)
        for (GdkWindow *bd : s_extra_backdrops)
            if (bd && t_wins[i] == x11::gdk_x11_window_get_xid(bd))
            {
                t_max_bd_idx = (long)i;
                t_top_bd_win = bd;
            }

    if (t_max_bd_idx >= 0 && t_top_bd_win)
    {
        static x11::Atom s_restack_atom = 0;
        if (!s_restack_atom) s_restack_atom = x11::XInternAtom(t_xdpy, "_NET_RESTACK_WINDOW", False);

        // Find the lowest-indexed HXT stack buried under the extra backdrop.
        x11::Window t_first_buried = 0;
        for (long i = 0; i < t_max_bd_idx; i++)
        {
            MCStack *t_stk = MCdispatcher->findstackwindowid((uintptr_t)t_wins[i]);
            if (!t_stk) continue;
            GdkWindow *t_win = (GdkWindow *)t_stk->getwindowalways();
            if (!t_win || hxt_is_backdrop_window(t_win)) continue;
            if (!gdk_window_is_visible(t_win)) continue;
            if (!hxt_overlaps_extra_backdrop(t_win)) continue;
            t_first_buried = t_wins[i];
            break;
        }

        if (t_first_buried)
        {
            // Safety net: send _NET_RESTACK_WINDOW(Below, sibling=stack) to ask
            // the WM to lower the backdrop below the buried stack.
            x11::XEvent t_ev = {};
            t_ev.xclient.type         = ClientMessage;
            t_ev.xclient.display      = t_xdpy;
            t_ev.xclient.window       = x11::gdk_x11_window_get_xid(t_top_bd_win);
            t_ev.xclient.message_type = s_restack_atom;
            t_ev.xclient.format       = 32;
            t_ev.xclient.data.l[0]    = 2;                    // source: pager
            t_ev.xclient.data.l[1]    = (long)t_first_buried; // sibling = buried stack
            t_ev.xclient.data.l[2]    = 1;                    // detail: Below
            x11::XSendEvent(t_xdpy, t_root, False,
                            SubstructureRedirectMask | SubstructureNotifyMask, &t_ev);
            x11::XFlush(t_xdpy);
        }
    }

    x11::XFree(t_data);
    return G_SOURCE_CONTINUE;
}

void MCScreenDC::reraise_stacks_above_backdrop()
{
    // Called from the backdrop button-press handler: Mutter's click-to-raise
    // has already raised the clicked backdrop above HXT stacks on that monitor.
    // Re-raise everything to restore correct Z-order.
    backdrop_focus_gained();
}

// ---------------------------------------------------------------------------
// _NET_ACTIVE_WINDOW root-window filter
//
// GDK_FOCUS_CHANGE events are unreliable on GNOME/Mutter when keep_above
// windows hold X11 input focus — the WM changes _NET_ACTIVE_WINDOW but never
// delivers a FocusOut to our windows.  We monitor the root property directly.
// ---------------------------------------------------------------------------

static x11::Atom  s_net_active_window_atom = None;
static bool       s_active_win_filter_installed = false;
static x11::Window s_last_active_xwin = None;

static GdkFilterReturn hxt_active_win_filter(GdkXEvent *p_xevent,
                                              GdkEvent  * /*p_event*/,
                                              gpointer    /*p_data*/)
{
    x11::XEvent *xe = static_cast<x11::XEvent*>(p_xevent);
    if (xe->type != PropertyNotify) return GDK_FILTER_CONTINUE;
    if (xe->xproperty.atom != s_net_active_window_atom) return GDK_FILTER_CONTINUE;

    MCScreenDC *dc = static_cast<MCScreenDC*>(MCscreen);
    if (!dc) return GDK_FILTER_CONTINUE;

    GdkDisplay     *t_dpy  = dc->getDisplay();
    x11::Display   *t_xdpy = x11::gdk_x11_display_get_xdisplay(t_dpy);
    x11::Window     t_root = x11::XDefaultRootWindow(t_xdpy);

    // Read the current _NET_ACTIVE_WINDOW value.
    x11::Atom         t_type;
    int               t_format;
    unsigned long     t_nitems, t_bytes_after;
    unsigned char    *t_prop = nullptr;
    if (x11::XGetWindowProperty(t_xdpy, t_root, s_net_active_window_atom,
                                0, 1, False, (x11::Atom)33 /*XA_WINDOW*/,
                                &t_type, &t_format,
                                &t_nitems, &t_bytes_after, &t_prop) != Success
        || !t_prop)
    {
        return GDK_FILTER_CONTINUE;
    }

    x11::Window t_active = *(x11::Window*)t_prop;
    x11::XFree(t_prop);

    if (t_active == None)
    {
        // xwin=0 is a transient state the WM emits while selecting the next
        // window (e.g. during ALT+TAB).  Reacting here disrupts the WM's own
        // stacking decisions — skip it and wait for the real target XID.
        return GDK_FILTER_CONTINUE;
    }

    // Determine whether the PREVIOUS active window was ours.  This lets us
    // distinguish "focus returning from another app" (needs a stack re-raise)
    // from "focus moving between HXT windows" (Z-order is already correct —
    // raising stacks here would bury overlays like the autocomplete popup).
    bool t_prev_was_ours = false;
    if (s_last_active_xwin != None)
    {
        GdkWindow *t_prev_gdk = x11::gdk_x11_window_lookup_for_display(t_dpy, s_last_active_xwin);
        if (t_prev_gdk)
        {
            if (hxt_is_backdrop_window(t_prev_gdk))
                t_prev_was_ours = true;
            else if (MCdispatcher->findstackd(t_prev_gdk) != nullptr)
                t_prev_was_ours = true;
        }
    }

    s_last_active_xwin = t_active;

    // Look the new active XID up in GDK's window table.
    GdkWindow *t_gdk = x11::gdk_x11_window_lookup_for_display(t_dpy, t_active);

    bool t_is_ours = false;
    if (t_gdk)
    {
        if (hxt_is_backdrop_window(t_gdk))
            t_is_ours = true;
        else if (MCdispatcher->findstackd(t_gdk) != nullptr)
            t_is_ours = true;
    }

    if (t_is_ours)
    {
        // Only raise backdrops and stacks when focus is coming FROM outside
        // HXT.  When focus moves between HXT windows (e.g. autocomplete popup
        // shown then focus returned to the editor) the Z-order is already
        // correct and raising would bury any visible overlay windows.
        if (!t_prev_was_ours)
            dc->backdrop_focus_gained();
    }
    else
        dc->backdrop_focus_lost();

    return GDK_FILTER_CONTINUE;
}

static void hxt_install_active_window_filter(GdkDisplay *p_dpy)
{
    if (s_active_win_filter_installed) return;
    s_active_win_filter_installed = true;

    x11::Display *t_xdpy = x11::gdk_x11_display_get_xdisplay(p_dpy);
    x11::Window   t_root = x11::XDefaultRootWindow(t_xdpy);

    s_net_active_window_atom = x11::XInternAtom(t_xdpy, "_NET_ACTIVE_WINDOW", False);

    // Add PropertyChangeMask to the root window so GDK forwards PropertyNotify.
    x11::XWindowAttributes t_attrs;
    x11::XGetWindowAttributes(t_xdpy, t_root, &t_attrs);
    x11::XSelectInput(t_xdpy, t_root,
                      t_attrs.your_event_mask | PropertyChangeMask);

    GdkScreen *t_screen = gdk_display_get_default_screen(p_dpy);
    GdkWindow *t_root_gdk = gdk_screen_get_root_window(t_screen);
    gdk_window_add_filter(t_root_gdk, hxt_active_win_filter, nullptr);
}

// Called when HXT becomes the active application (_NET_ACTIVE_WINDOW points
// to one of our windows).  Raises backdrop and stacks above other apps.
// No keep_above — everything stays in NORMAL layer so ALT+TAB can work.
void MCScreenDC::backdrop_focus_gained()
{
    if (!backdrop_active && !backdrop_hard) return;

    auto do_raise = [](GdkWindow *w) {
        if (w && gdk_window_is_visible(w))
            gdk_window_raise(w);
    };

    do_raise(s_primary_backdrop);
    for (GdkWindow *bd : s_extra_backdrops) do_raise(bd);

    // Raise stacks explicitly on all WMs.
    // On Mutter, transient_for keeps stacks above the PRIMARY backdrop, but
    // extra backdrop windows on secondary monitors are separate and can appear
    // above a stack on that monitor.  Explicit raise guarantees stacks end up
    // above ALL backdrop windows regardless of monitor.
    for (GdkWindow *stk : s_backdrop_stacks)
        do_raise(stk);
}

// Called when a non-HXT window becomes active (_NET_ACTIVE_WINDOW changed).
// With no keep_above, the WM can freely raise the other app above our windows
// without any help from us.  Nothing to do here.
void MCScreenDC::backdrop_focus_lost()
{
}

// -- tperry 12-11-2025: GTK3/4 window wrapper
#include "lnxgtk-window.h"

#include "license.h"
#include "revbuild.h"
#include "resolution.h"

// Tears down the native GtkPopover hierarchy created in lnxstack.cpp::realize().
// MCpopoverstack must be nulled before calling so on_popover_closed() is a no-op.
extern void MCLinuxPopoverClose(void);

#include <langinfo.h>
#include <fcntl.h>
#include <sys/shm.h>

#ifdef HAVE_LIBGNOME
#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#endif

namespace x11
{
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
}

#include <gdk/gdkkeysyms.h>

#undef STARTMSGS

#define REQUEST_SIZE 32   // size of request header for X calls
#define MAX_POINTS 8096U   // number of points in polygon
#define ICON_SIZE 48
#define XYCUTOFF 4        // plane based or packed pixel

////////////////////////////////////////////////////////////////////////////////

bool MCImageBitmapCreateWithGdkPixbuf(GdkPixbuf *p_image, MCImageBitmap *&r_bitmap);

// Defined in lnxdc.cpp — returns the HiDPI scale of the primary monitor.
extern MCGFloat MCLinuxGetLogicalToScreenScale(void);

// HiDPI: GLib signal callbacks must be plain C function pointers —
// lambdas cannot be cast to GCallback in C++.

// Called when a monitor's scale factor changes at runtime.
static void MCLinuxOnMonitorScaleChanged(GdkMonitor *, GParamSpec *, gpointer)
{
    MCResSetPixelScale(MCLinuxGetLogicalToScreenScale());
}

// Called when a new monitor is added; connects the scale-factor signal to it.
static void MCLinuxOnMonitorAdded(GdkDisplay *, GdkMonitor *p_monitor, gpointer)
{
    g_signal_connect(p_monitor, "notify::scale-factor",
                     G_CALLBACK(MCLinuxOnMonitorScaleChanged), nullptr);
}

////////////////////////////////////////////////////////////////////////////////

GdkDisplay *MCdpy;

////////////////////////////////////////////////////////////////////////////////

extern "C" int initialise_required_weak_link_X11();
// Cairo, GDK, GObject, and GdkPixbuf are now statically linked

// GTK2 is now statically linked - no weak linking needed for GTK functions
#ifdef HAVE_LIBGNOME
extern "C" int initialise_weak_link_gnome_vfs ( void ) ;
extern "C" int initialise_weak_link_libgnome ( void ) ;
#endif
extern "C" int initialise_weak_link_libxv ( void ) ;

////////////////////////////////////////////////////////////////////////////////

static uint1 flip_table[] =
    {
        0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0,
        0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
        0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
        0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
        0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4,
        0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
        0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC,
        0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
        0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
        0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
        0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA,
        0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
        0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6,
        0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
        0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,

        0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
        0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1,
        0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
        0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9,
        0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
        0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5,
        0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
        0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED,
        0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
        0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3,
        0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
        0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB,
        0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
        0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7,
        0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
        0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF,
        0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF
    };

static uint4 cmap_scale[17] =
    {
        65535, 65535, 21845, 9362,
        4369,  2114,  1040,  516,
        257,   128,    64,   32,
        16,     8,     4,    2,   1
    };

////////////////////////////////////////////////////////////////////////////////

void MCScreenDC::setstatus(MCStringRef status)
{
}

//XDND
void init_xDnD(void);

Drawable MCScreenDC::getdest()
{
	return dest;
}

extern "C"
{
void gtk_main_do_event(GdkEvent*);
}

void gdk_event_fn(GdkEvent *p_event, gpointer dc)
{
    // Let GTK process the original event first so widgets (file chooser dialogs,
    // etc.) can draw and respond to input. gtk_main_do_event may destroy
    // GdkWindows (e.g. on GDK_DELETE), but that only affects the original
    // event's GObject refs which GDK's own dispatch handles.
    gtk_main_do_event(p_event);

    // Enqueue a copy for our own LiveCode event processing.
    // safe_gdk_event_free handles any dangling GObject pointers when we
    // free the copy later (in case gtk_main_do_event destroyed a window).
    ((MCScreenDC*)dc)->EnqueueEvent(gdk_event_copy(p_event));
}

void gdk_event_fn_lost(void*)
{
    ;
}

// Functions for re-directing GTK IM context signals
static void on_commit(GtkIMContext *p_context, gchar *p_str, gpointer p_data)
{
    ((MCScreenDC*)p_data)->IME_OnCommit(p_context, p_str);
}
static gboolean on_delete_surrounding(GtkIMContext *p_context, gint p_offset, gint p_count, gpointer p_data)
{
    return ((MCScreenDC*)p_data)->IME_OnDeleteSurrounding(p_context, p_offset, p_count);
}
static void on_preedit_changed(GtkIMContext *p_context, gpointer p_data)
{
    ((MCScreenDC*)p_data)->IME_OnPreeditChanged(p_context);
}
static void on_preedit_end(GtkIMContext *p_context, gpointer p_data)
{
    ((MCScreenDC*)p_data)->IME_OnPreeditEnd(p_context);
}
static void on_preedit_start(GtkIMContext *p_context, gpointer p_data)
{
    ((MCScreenDC*)p_data)->IME_OnPreeditStart(p_context);
}
static void on_retrieve_surrounding(GtkIMContext *p_context, gpointer p_data)
{
    ((MCScreenDC*)p_data)->IME_OnRetrieveSurrounding(p_context);
}

Boolean MCScreenDC::open()
{
	// We require X11 for windowing
    initialise_required_weak_link_X11();

    // Initialize GTK (only if not already initialized)
    // Note: GDK_BACKEND=x11 is set early in platform_main (dsklnxmain.cpp)
    static bool gtk_initialized = false;
    if (!gtk_initialized) {
        int argc = 0;
        char **argv = NULL;
        gtk_init_check(&argc, &argv);
        gtk_initialized = true;
    }

    // -- tperry 15-11-2025: GTK2 theme engine initialization removed for GTK3
    // GTK3 uses CSS for theming, not RC files or theme engines
    // init_static_theme_engines();

    // Get the current theme and parse its RC file
    // Use signal handler to catch crashes during theme parsing
    static volatile sig_atomic_t theme_parse_crashed = 0;
    static sigjmp_buf theme_parse_jmpbuf;

    // Signal handler for SIGSEGV during theme parsing
    struct sigaction old_segv_action;
    struct sigaction segv_action;
    segv_action.sa_handler = [](int sig) {
        theme_parse_crashed = 1;
        siglongjmp(theme_parse_jmpbuf, 1);
    };
    sigemptyset(&segv_action.sa_mask);
    segv_action.sa_flags = 0;

    GtkSettings *settings = gtk_settings_get_default();
    if (settings && G_IS_OBJECT(settings)) {
        gchar *theme_name = NULL;
        g_object_get(settings, "gtk-theme-name", &theme_name, NULL);

        if (theme_name && theme_name[0]) {
            // GTK3 uses CSS files, not gtkrc files
            gchar *theme_css = g_strdup_printf("/usr/share/themes/%s/gtk-3.0/gtk.css", theme_name);
            if (g_file_test(theme_css, G_FILE_TEST_EXISTS)) {
                // Install signal handler
                sigaction(SIGSEGV, &segv_action, &old_segv_action);

                // Try to load theme CSS up to 3 times
                for (int attempt = 0; attempt < 3 && !theme_parse_crashed; attempt++) {
                    if (sigsetjmp(theme_parse_jmpbuf, 1) == 0) {
                        // Load the GTK3 theme CSS
                        GtkCssProvider *css_provider = gtk_css_provider_new();
                        GFile *css_file = g_file_new_for_path(theme_css);
                        GError *error = NULL;

                        if (gtk_css_provider_load_from_file(css_provider, css_file, &error)) {
                            // Apply the CSS provider to the default screen
                            gtk_style_context_add_provider_for_screen(
                                gdk_screen_get_default(),
                                GTK_STYLE_PROVIDER(css_provider),
                                GTK_STYLE_PROVIDER_PRIORITY_THEME);
                        } else {
                            if (error) {
                                fprintf(stderr, "Warning: Failed to load theme CSS: %s\n", error->message);
                                g_error_free(error);
                            }
                        }

                        g_object_unref(css_file);
                        g_object_unref(css_provider);
                        break; // Success!
                    } else {
                        // Crashed, will retry
                        fprintf(stderr, "Warning: Theme CSS loading crashed (attempt %d/3), retrying...\n", attempt + 1);
                        theme_parse_crashed = 0;
                    }
                }

                // Restore original signal handler
                sigaction(SIGSEGV, &old_segv_action, NULL);

                if (theme_parse_crashed) {
                    fprintf(stderr, "Warning: Theme CSS loading failed after 3 attempts, continuing without theme\n");
                }
            }
            g_free(theme_css);
        }

        if (theme_name)
            g_free(theme_name);
    }

    gdk_init(0, NULL);
    //gdk_threads_init();

    // Check to see if we are in a UTF8 locale
	// TS : Changed 2008-01-08 as a more relaible way of testing for UTF-8
	MCutf8 = (strcmp(nl_langinfo(CODESET), "UTF-8") == 0)	;

	MCimagecache = new (nothrow) MCXImageCache ;

    if (MCdisplayname == NULL)
    {
        // gdk_get_display() returns the Wayland socket name on Wayland
        // sessions; we need an X11 display name, so read $DISPLAY directly.
        const gchar *t_x11_display = g_getenv("DISPLAY");
        MCdisplayname = (char *)(t_x11_display ? t_x11_display : gdk_get_display());
    }

	if ((dpy = gdk_display_open(MCdisplayname)) == NULL)
	{
        MCAutoStringRefAsSysString t_cmd;
        t_cmd.Lock(MCcmd);
        fprintf(stderr, "%s: Can't open display %s\n",
                *t_cmd, MCdisplayname);
		return False;
	}

    // Build the class name
    MCAutoStringRef t_class_name;
    MCAutoStringRefAsUTF8String t_class_name_utf8;
    bool t_community;
    t_community = MClicenseparameters.license_class == kMCLicenseClassCommunity;

    /* UNCHECKED */ MCStringCreateMutable(0, &t_class_name);
    // Use a stable, version-independent class name so StartupWMClass in the
    // .desktop file never needs updating between releases.
    /* UNCHECKED */ MCStringAppendFormat(*t_class_name, "%s", MCapplicationstring);
    /* UNCHECKED */ t_class_name_utf8.Lock(*t_class_name);

    // Used to load the icon and other desktop properties
    gdk_set_program_class(*t_class_name_utf8);

    // The GLib event loop calls this function to respond to GDK events.
    // Unfortunately, when GTK gets initied, it will try to steal this from us.
    gdk_event_handler_set(&gdk_event_fn, gpointer(this), &gdk_event_fn_lost);

    // ── HiDPI: monitor scale-factor change notifications ──
    //
    // GDK fires "notify::scale-factor" on each GdkMonitor when the user
    // changes the display scale in GNOME Settings (or GDK_SCALE changes).
    // We connect to existing monitors now and to any added later via the
    // display's "monitor-added" signal.
    //
    // On scale change: MCLinuxOnMonitorScaleChanged calls MCResSetPixelScale()
    // which calls MCResPlatformHandleScaleChange() to re-scale all open
    // stacks — mirroring the WM_DPICHANGED path on Windows.

    // Connect to all monitors currently known to the display.
    gint t_n = gdk_display_get_n_monitors(dpy);
    for (gint i = 0; i < t_n; i++)
    {
        GdkMonitor *t_mon = gdk_display_get_monitor(dpy, i);
        g_signal_connect(t_mon, "notify::scale-factor",
                         G_CALLBACK(MCLinuxOnMonitorScaleChanged), nullptr);
    }

    // Connect for monitors added in the future (e.g. plugging in a display).
    g_signal_connect(dpy, "monitor-added",
                     G_CALLBACK(MCLinuxOnMonitorAdded), nullptr);

    x11::Display* XDisplay;
    XDisplay = x11::gdk_x11_display_get_xdisplay(dpy);

    GdkScreen *t_screen;
    t_screen = gdk_display_get_default_screen(dpy);

    {
        MCAutoStringRef t_displayname;
        MCNewAutoNameRef t_displayname_asname;
        if (MCStringCreateWithSysString(gdk_display_get_name(dpy),
                                        &t_displayname) &&
            MCNameCreate(*t_displayname, &t_displayname_asname))
        {
            displayname.Reset(t_displayname_asname.Take());
        }
    }
    if (XDisplay != nullptr)
    {
        MCAutoStringRef t_vendor_string, t_vendor;
        MCNewAutoNameRef t_vendorname;
        const char *t_vendor_cstr = x11::XServerVendor(XDisplay);
        if (t_vendor_cstr != nullptr &&
            MCStringCreateWithSysString(t_vendor_cstr, &t_vendor) &&
            MCStringFormat(&t_vendor_string, "%@ %d", *t_vendor,
                           x11::XVendorRelease(XDisplay)) &&
            MCNameCreate(*t_vendor_string, &t_vendorname))
        {
            vendorname.Reset(t_vendorname.Take());
        }
    }

#ifdef SYNCMODE
    // TODO: equivalent in GDK?
	XSynchronize(dpy, True);
	XSync ( dpy, False ) ;
#ifdef STARTMSGS
	fprintf(stderr, "Xserver sync on\n");
#endif
#endif

	//XDND - Need to set up the xDnD protocol so that we have access to the XdndAware atom.
	init_xDnD();

    if (MCvisualid)
    {
        // An explicit visual ID was specified on the command line. Get it.
        vis = x11::gdk_x11_screen_lookup_visual(t_screen, MCvisualid);

        if (vis == NULL)
        {
            MCAutoStringRefAsSysString t_cmd;
            t_cmd.Lock(MCcmd);
            fprintf(stderr, "%s: Bad visual id %x\n", *t_cmd, MCvisualid);
            MCvisualid = 0;
        }
        else
        {
            // What type of visual are we dealing with?
            // -- tperry 15-11-2025: GTK3 - colormaps handled automatically, just extract visual info
            switch (gdk_visual_get_visual_type(vis))
            {
                case GDK_VISUAL_STATIC_GRAY:
                case GDK_VISUAL_STATIC_COLOR:
                {
                    // GTK3: No need to create colormap, just setup colors
                    setupcolors();
                    break;
                }

                case GDK_VISUAL_TRUE_COLOR:
                {
                    // GTK3: No need to create colormap, just extract visual information
                    gint t_redshift, t_greenshift, t_blueshift;
                    gint t_redbits, t_greenbits, t_bluebits;
                    gdk_visual_get_red_pixel_details(vis, NULL, &t_redshift, &t_redbits);
                    gdk_visual_get_green_pixel_details(vis, NULL, &t_greenshift, &t_greenbits);
                    gdk_visual_get_blue_pixel_details(vis, NULL, &t_blueshift, &t_bluebits);
                    redshift = t_redshift;
                    greenshift = t_greenshift;
                    blueshift = t_blueshift;
                    redbits = t_redbits;
                    greenbits = t_greenbits;
                    bluebits = t_bluebits;

                    break;
                }

                case GDK_VISUAL_DIRECT_COLOR:
                {
                    // -- tperry 15-11-2025: GTK3 - no colormap needed, just extract visual info
                    guint32 t_redmask, t_greenmask, t_bluemask;
                    gint t_redshift, t_greenshift, t_blueshift;
                    gint t_redbits, t_greenbits, t_bluebits;
                    gdk_visual_get_red_pixel_details(vis, &t_redmask, &t_redshift, &t_redbits);
                    gdk_visual_get_green_pixel_details(vis, &t_greenmask, &t_greenshift, &t_greenbits);
                    gdk_visual_get_blue_pixel_details(vis, &t_bluemask, &t_blueshift, &t_bluebits);
                    redshift = t_redshift;
                    greenshift = t_greenshift;
                    blueshift = t_blueshift;
                    redbits = t_redbits;
                    greenbits = t_greenbits;
                    bluebits = t_bluebits;

                    // GTK3: Color allocation handled automatically, no need to store colors
                    break;
                }

                case GDK_VISUAL_GRAYSCALE:
                {
                    // -- tperry 15-11-2025: GTK3 - no colormap needed, just setup colors
                    setupcolors();

                    for (int i = 0 ; i < ncolors ; i++)
                    {
                        colors[i].red = colors[i].green = colors[i].blue = i * MAXUINT2 / ncolors;
                    }
                    // GTK3: Color allocation handled automatically
                    break;
                }

                case GDK_VISUAL_PSEUDO_COLOR:
                {
                    // -- tperry 15-11-2025: GTK3 - no colormap needed
                    setupcolors();
                    MCuseprivatecmap = true;
                    break;
                }
            }
        }
    }

	if (MCvisualid == 0)
	{
        // -- tperry 15-11-2025: GTK3 - colormaps handled automatically, just get visuals
        // If the screen is composited, we can expect alpha values to work
        if (gdk_screen_is_composited(t_screen))
        {
            // Get an RGBA visual to use
#ifdef STARTMSGS
			fprintf(stderr, "Composite window manager detected. Trying to use RGBA visual.\n");
#endif
            vis = gdk_screen_get_rgba_visual(t_screen);
        }

        // If getting the RGBA visual fails, or we are not composited, then
        // use the default visual.
        if (vis == nullptr)
        {
            // Get the default visual (GTK3: no colormap needed)
            vis = gdk_screen_get_system_visual(t_screen);
        }

        // If getting the system visual fails then use the rgb visual.
        if (vis == nullptr)
        {
            // GTK3: gdk_screen_get_rgb_visual removed, use rgba visual
            vis = gdk_screen_get_rgba_visual(t_screen);
        }

		if (gdk_visual_get_visual_type(vis) == GDK_VISUAL_TRUE_COLOR)
		{
            gint t_redshift, t_greenshift, t_blueshift;
            gint t_redbits, t_greenbits, t_bluebits;
            gdk_visual_get_red_pixel_details(vis, NULL, &t_redshift, &t_redbits);
            gdk_visual_get_green_pixel_details(vis, NULL, &t_greenshift, &t_greenbits);
            gdk_visual_get_blue_pixel_details(vis, NULL, &t_blueshift, &t_bluebits);
            redshift = t_redshift;
            greenshift = t_greenshift;
            blueshift = t_blueshift;
            redbits = t_redbits;
            greenbits = t_greenbits;
            bluebits = t_bluebits;
		}
		else
		{
			setupcolors();
		}
	}

    // -- tperry 15-11-2025: GTK3 - setup window attributes for NULL window
    GdkWindowAttr gdkwa;
    gdkwa.event_mask = GDK_ALL_EVENTS_MASK & ~GDK_POINTER_MOTION_HINT_MASK;
    gdkwa.x = gdkwa.y = gdkwa.width = gdkwa.height = 8;
    gdkwa.visual = vis;
    // GTK3: colormap field removed from GdkWindowAttr
    gdkwa.wclass = GDK_INPUT_OUTPUT;
    gdkwa.window_type = GDK_WINDOW_TOPLEVEL;

    // -- tperry 15-11-2025: GTK3 - no need to create temporary window for GC
    // Cairo contexts are created on-demand when needed for drawing
    gc = nullptr;  // Will be created when needed

    // Create the NULL window
    NULLWindow = gdk_window_new(gdk_screen_get_root_window(t_screen),
                                &gdkwa,
                                GDK_WA_X|GDK_WA_Y|GDK_WA_VISUAL);

    // GTK3: GdkGC removed, Cairo contexts created on-demand

	black_pixel.red = black_pixel.green = black_pixel.blue = 0;
	white_pixel.red = white_pixel.green = white_pixel.blue = MAXUINT2;

	MCdpy = dpy;

    {
        // Detect Muffin (Cinnamon) — see comment on s_wm_is_muffin above.
        const char *t_wm = x11::gdk_x11_screen_get_window_manager_name(
            gdk_display_get_default_screen(MCdpy));
        s_wm_is_muffin = (t_wm != nullptr &&
                          (strstr(t_wm, "Muffin") != nullptr ||   // Cinnamon
                           strstr(t_wm, "Marco")  != nullptr));   // MATE/Metacity
        s_wm_is_marco  = (t_wm != nullptr &&
                           strstr(t_wm, "Marco")  != nullptr);    // MATE/Metacity only
    }

    /*GdkPixmap *cdata = gdk_pixmap_new(getroot(), 16, 16, 1);
    GdkPixmap *cmask = gdk_pixmap_new(getroot(), 16, 16, 1);
    gdk_gc_set_foreground(gc1, &t_color);
    gdk_draw_rectangle(cdata, gc1, TRUE, 0, 0, 16, 16);
    gdk_draw_rectangle(cmask, gc1, TRUE, 0, 0, 16, 16);
    t_color.pixel = 1;
    gdk_gc_set_foreground(gc1, &t_color);*/

    MCColor c;
    c.red = c.green = c.blue = 0x0;

    for (uint32_t i = 0; i < PI_NCURSORS; i++)
        MCcursors[i] = nil;

    //g_object_unref(cdata);
    //g_object_unref(cmask);

	MConecolor.red = MConecolor.green = MConecolor.blue = 0xFFFF;
	MCselectioncolor = MCpencolor = black_pixel;
	MCbrushcolor = white_pixel;
	MCaccentcolor = MChilitecolor;
	gray_pixel.red = gray_pixel.green = gray_pixel.blue = 0x8080;

	background_pixel.red = background_pixel.green = background_pixel.blue = 0xdcdc;
	if (MCcurtheme && MCcurtheme->getthemeid() == LF_NATIVEGTK)
		MCcurtheme->load();
	opened = True;
	selectiontext = NULL;
	int2 x, y;
	querymouse(x, y);

	if (MCmodifierstate & 0x02 << MCextendkey)
		MCextendkey = 5; // disable bogus numlock key binding on some SPARCs
	MCwbr.y = MCwbr.x = 0;
	MCwbr.width = getwidth();
	MCwbr.height = getheight();

	// GTK2 is now statically linked - always available
	m_has_native_theme = m_has_gtk = true;
	m_has_native_color_dialogs = true;
	m_has_native_file_dialogs = true;
	m_has_native_print_dialogs = true;

    // If we have GTK, we can make use of the GTK IME support
    if (m_has_gtk)
    {
        m_im_context = gtk_im_multicontext_new();

        // Set up the signals for the IM context
        g_signal_connect(m_im_context, "commit", G_CALLBACK(&on_commit), this);
        g_signal_connect(m_im_context, "delete-surrounding", G_CALLBACK(&on_delete_surrounding), this);
        g_signal_connect(m_im_context, "preedit-changed", G_CALLBACK(&on_preedit_changed), this);
        g_signal_connect(m_im_context, "preedit-end", G_CALLBACK(&on_preedit_end), this);
        g_signal_connect(m_im_context, "preedit-start", G_CALLBACK(&on_preedit_start), this);
        g_signal_connect(m_im_context, "retrieve-surrounding", G_CALLBACK(&on_retrieve_surrounding), this);
    }

#ifdef HAVE_LIBGNOME
	if ( initialise_weak_link_gnome_vfs() != 0 )
	{
		MCuselibgnome = initialise_weak_link_libgnome();
		gnome_vfs_init();
	}
#endif

    // There are also some atoms that we need to set up
    MCworkareaatom = gdk_atom_intern_static_string("_NET_WORKAREA");
    MCclientlistatom = gdk_atom_intern_static_string("_NET_CLIENT_LIST");
    MCstrutpartialatom = gdk_atom_intern_static_string("_NET_WM_STRUT_PARTIAL");
    MCdndselectionatom = gdk_atom_intern_static_string("XdndSelection");

	return True;
}

// Defined in xans.cpp
extern void gtk_file_tidy_up ( void );



// Returns an XAtom with the given name ;
Atom  MCScreenDC::make_atom ( char * p_atom_name )
{
	return gdk_atom_intern(p_atom_name, FALSE);
}

//XDND
extern void MCLinuxDragAndDropFinalize();

Boolean MCScreenDC::close(Boolean force)
{
	// TODO - We may need to do clipboard persistance here

	destroybackdrop();
    gdk_window_destroy(NULLWindow);

    // Process any pending GTK events before closing to prevent corruption
    while (gtk_events_pending())
        gtk_main_iteration_do(FALSE);

    // Clear pending events BEFORE closing display to avoid use-after-free
    while (pendingevents != NULL)
    {
        MCEventnode *tptr = (MCEventnode *)pendingevents->remove(pendingevents);
        if (tptr != NULL)
            delete tptr;
    }

    gdk_display_flush(dpy);
    // gc is a cairo_t*, not a GObject — release with cairo_destroy if non-null
    if (gc) { cairo_destroy(gc); gc = nullptr; }

	//XDND
	MCLinuxDragAndDropFinalize();

    gdk_display_close(dpy);

	delete (char *)selectiontext.getstring();
	opened = False;

	gtk_file_tidy_up () ;


	delete MCimagecache ;

	return True;
}

MCNameRef MCScreenDC::getdisplayname()
{
	return *displayname;
}


// -- tperry 15-11-2025: GTK3 - GdkVisual is opaque, use gdk_visual_get_depth()
uint2 MCScreenDC::getrealdepth(void)
{
	return gdk_visual_get_depth(vis);
}



uint2 MCScreenDC::getdepth(void)
{
	gint depth = gdk_visual_get_depth(vis);
	if (depth < 24)
		return 32;

	return depth;
}

void MCScreenDC::grabpointer(Window w)
{
    // GTK3: use gdk_seat_grab instead of deprecated gdk_pointer_grab (Wayland-safe)
    // Window is typedef'd as GdkWindow* in this codebase, so w is used directly.
    GdkSeat *t_seat = gdk_display_get_default_seat(dpy);
    gdk_seat_grab(t_seat, w,
                  GDK_SEAT_CAPABILITY_POINTER,
                  FALSE, NULL, NULL, NULL, NULL);
}

void MCScreenDC::ungrabpointer()
{
    // GTK3: use gdk_seat_ungrab instead of deprecated gdk_display_pointer_ungrab
    GdkSeat *t_seat = gdk_display_get_default_seat(dpy);
    gdk_seat_ungrab(t_seat);
}

// IM-2014-01-29: [[ HiDPI ]] Placeholder method for Linux HiDPI support
uint16_t MCScreenDC::platform_getwidth(void)
{
	return device_getwidth();
}

uint16_t MCScreenDC::device_getwidth(void)
{
	return gdk_screen_get_width(getscreen());
}

// IM-2014-01-29: [[ HiDPI ]] Placeholder method for Linux HiDPI support
uint16_t MCScreenDC::platform_getheight(void)
{
	return device_getheight();
}

uint16_t MCScreenDC::device_getheight(void)
{
	return gdk_screen_get_height(getscreen());
}

uint2 MCScreenDC::getwidthmm()
{
	return gdk_screen_get_width_mm(getscreen());
}

uint2 MCScreenDC::getheightmm()
{
	return gdk_screen_get_height_mm(getscreen());
}

// MW-2005-09-24: We shouldn't be accessing the display structure like this
//   so use the XMaxRequestSize() call instead
uint2 MCScreenDC::getmaxpoints()
{
	return MCU_min(x11::XMaxRequestSize(x11::gdk_x11_display_get_xdisplay(dpy)) - REQUEST_SIZE, MAX_POINTS);
}

uint2 MCScreenDC::getvclass()
{
	switch (gdk_visual_get_visual_type(vis))
    {
        case GDK_VISUAL_STATIC_GRAY:
            return StaticGray;

        case GDK_VISUAL_GRAYSCALE:
            return GrayScale;

        case GDK_VISUAL_STATIC_COLOR:
            return StaticColor;

        case GDK_VISUAL_PSEUDO_COLOR:
            return PseudoColor;

        case GDK_VISUAL_TRUE_COLOR:
            return TrueColor;

        case GDK_VISUAL_DIRECT_COLOR:
            return DirectColor;

        default:
            MCAssert(false);
            return 0;
    }
}

void MCScreenDC::openwindow(Window window, Boolean override)
{
	MCStack *target = MCdispatcher->findstackd(window);

	// WM_POPOVER is handled entirely in MCStack::platform_openwindow() before
	// this function is ever called (it short-circuits to avoid passing a null
	// window here), so no WM_POPOVER check is needed.

    // If the backdrop is active, pre-set _NET_WM_STATE_ABOVE and WM_TRANSIENT_FOR
    // as X11 properties BEFORE mapping.  The WM reads these at MapRequest time, so
    // the window starts life in the correct stacking layer.  assignbackdrop() will
    // also send the ClientMessage form of _NET_WM_STATE_ABOVE after mapping as
    // belt-and-suspenders, but the pre-map property is what matters for new windows.
    if (target != nullptr)
    {
        Window_mode t_mode = target->getmode();
        if (t_mode >= WM_TOP_LEVEL && t_mode <= WM_SHEET &&
            backdrop != DNULL && (backdrop_active || backdrop_hard))
        {
            x11::Display *t_xdpy = x11::gdk_x11_display_get_xdisplay(MCdpy);
            x11::Window   t_xwin = x11::gdk_x11_window_get_xid(window);
            x11::Atom t_state = x11::XInternAtom(t_xdpy, "_NET_WM_STATE", False);
            x11::Atom t_above = x11::XInternAtom(t_xdpy, "_NET_WM_STATE_ABOVE", False);
            x11::XChangeProperty(t_xdpy, t_xwin, t_state,
                                 (x11::Atom)4, 32, PropModeReplace,
                                 (unsigned char*)&t_above, 1);
            x11::XFlush(t_xdpy);
        }
    }

	{
		// XFCE workaround: Use show_unraised for palette windows to prevent focus stealing
		bool use_unraised = false;
		if (target && target->getrealmode() == WM_PALETTE)
		{
			const char *desktop = getenv("XDG_CURRENT_DESKTOP");
			if (desktop && (strstr(desktop, "XFCE") || strstr(desktop, "xfce")))
				use_unraised = true;
		}

		if (use_unraised)
			gdk_window_show_unraised(window);
		else
			gdk_window_show(window);
	}

    // Send ClientMessage form of _NET_WM_STATE_ABOVE and set WM_TRANSIENT_FOR
    // now that the window is mapped.
    if (target != nullptr)
        assignbackdrop(target->getmode(), window);

	MCstacks->enableformodal(window, False);
}

void MCScreenDC::closewindow(Window window)
{
	MCstacks->enableformodal(window, True);

    // If the parent stack for the current popover is being closed, dismiss
    // the popover first so it doesn't outlive its anchor.
    if (MCpopoverstack != nullptr && MCpopoverparentstack != nullptr &&
        MCpopoverparentstack->getwindowalways() == window)
    {
        MCStack *t_popover = MCpopoverstack;
        MCpopoverstack = nullptr;
        MCpopoverparentstack = nullptr;
        MCdispatcher->wclose(t_popover->getwindowalways());
    }

    // If the popover itself is being closed, tear down the GTK hierarchy.
    if (MCpopoverstack != nullptr && MCpopoverstack->getwindowalways() == window)
    {
        MCpopoverstack = nullptr;          // null first so on_popover_closed is a no-op
        MCpopoverparentstack = nullptr;
        // MCLinuxPopoverClose() nulls MCStack::window via s_popover_window_ptr
        // (a &window pointer stored in realize()) before calling gtk_widget_destroy,
        // so MCStack::destroywindow()'s `if (window == nil)` guard fires cleanly.
        MCLinuxPopoverClose();
        // Do NOT call gdk_window_hide() — the GdkWindow belongs to GTK.
        return;
    }

	gdk_window_hide(window);
}

void MCScreenDC::destroywindow(Window &window)
{
    // If the parent stack for the current popover is being destroyed, dismiss
    // the popover first so it doesn't outlive its anchor.
    if (MCpopoverstack != nullptr && MCpopoverparentstack != nullptr &&
        MCpopoverparentstack->getwindowalways() == window)
    {
        MCStack *t_popover = MCpopoverstack;
        MCpopoverstack = nullptr;
        MCpopoverparentstack = nullptr;
        MCdispatcher->wclose(t_popover->getwindowalways());
    }

    // If the popover itself is being destroyed, tear down the GTK hierarchy.
    if (MCpopoverstack != nullptr && MCpopoverstack->getwindowalways() == window)
    {
        MCpopoverstack = nullptr;
        MCpopoverparentstack = nullptr;
        window = DNULL;                    // GdkWindow is GTK-owned; null before destroy
        MCLinuxPopoverClose();
        return;
    }

	gdk_window_destroy(window);
	window = DNULL;
}

void MCScreenDC::raisewindow(Window window)
{
    if (window != nullptr)
        gdk_window_raise(window);
}

void MCScreenDC::iconifywindow(Window window)
{
	gdk_window_iconify(window);
}

void MCScreenDC::uniconifywindow(Window window)
{
	gdk_window_deiconify(window);
}

void MCScreenDC::setname(Window window, MCStringRef newname)
{
	MCAutoStringRefAsUTF8String t_newname_utf8;
	/* UNCHECKED */ t_newname_utf8 . Lock(newname);

	gdk_window_set_title(window, *t_newname_utf8);
}

void MCScreenDC::setcmap(MCStack *sptr)
{
	//gdk_drawable_set_colormap(sptr->getw(), getcmap());
}

void MCScreenDC::sync(Window w)
{
	gdk_display_sync(dpy);
}

void MCScreenDC::flush(Window w)
{
	gdk_display_flush(dpy);
}

void MCScreenDC::beep()
{
	gdk_beep();
}

void MCScreenDC::setinputfocus(Window window)
{
	gdk_window_focus(window, MCeventtime);
}

// -- tperry 15-11-2025: GTK3 - pixmaps are now cairo surfaces
void MCScreenDC::freepixmap(Pixmap &pixmap)
{
	if (pixmap != DNULL)
	{
		cairo_surface_destroy((cairo_surface_t*)pixmap);
		pixmap = DNULL;
	}
}

// -- tperry 15-11-2025: GTK3 - GdkPixmap removed, use cairo_surface_t instead
Pixmap MCScreenDC::createpixmap(uint2 width, uint2 height,
                                uint2 depth, Boolean purge)
{
	if (depth == 0 || depth == 32)
	{
		if (gdk_visual_get_depth(vis) == 24)
			depth = 24;
		else
			depth = 32;
	}

	// Create a Cairo image surface instead of GdkPixmap
	cairo_format_t format;
	if (depth == 1)
		format = CAIRO_FORMAT_A1;
	else if (depth == 8)
		format = CAIRO_FORMAT_A8;
	else if (depth == 24)
		format = CAIRO_FORMAT_RGB24;
	else
		format = CAIRO_FORMAT_ARGB32;

	cairo_surface_t *pm = cairo_image_surface_create(format, width, height);
	assert(pm != DNULL);

	return (Pixmap)pm;
}

// IM-2014-01-29: [[ HiDPI ]] Placeholder method for Linux HiDPI support
bool MCScreenDC::platform_getwindowgeometry(Window w, MCRectangle &r_rect)
{
	return device_getwindowgeometry(w, r_rect);
}

bool MCScreenDC::device_getwindowgeometry(Window w, MCRectangle &drect)
{
	Window root, child;
	gint x, y;
	gint width, height;

    // -- tperry 15-11-2025: GTK3 - gdk_window_get_geometry signature changed (no depth parameter)
    gdk_window_get_geometry(w, NULL, NULL, &width, &height);
    gdk_window_get_origin(w, &x, &y);

	MCU_set_rect(drect, x, y, width, height);
	return true;
}


// -- tperry 15-11-2025: GTK3 - pixmaps are now cairo surfaces
Boolean MCScreenDC::getpixmapgeometry(Pixmap p, uint2 &w, uint2 &h, uint2 &d)
{
    cairo_surface_t *surface = (cairo_surface_t*)p;

    w = cairo_image_surface_get_width(surface);
    h = cairo_image_surface_get_height(surface);

    // Get depth from Cairo format
    cairo_format_t format = cairo_image_surface_get_format(surface);
    switch (format)
    {
        case CAIRO_FORMAT_ARGB32:
            d = 32;
            break;
        case CAIRO_FORMAT_RGB24:
            d = 24;
            break;
        case CAIRO_FORMAT_A8:
            d = 8;
            break;
        case CAIRO_FORMAT_A1:
            d = 1;
            break;
        default:
            d = 32;
            break;
    }

	return True;
}

void MCScreenDC::setgraphicsexposures(Boolean on, MCStack *sptr)
{
	// -- tperry 15-11-2025: GTK3 - GdkGC removed, exposures handled differently
	// In GTK3/Cairo, exposure events are handled automatically
	// This function is now a no-op
}

// -- tperry 15-11-2025: GTK3 - GdkFunction removed, use cairo_operator_t instead
cairo_operator_t MCScreenDC::XOpToGdkOp(int op)
{
    // Map X11 operations to Cairo operators
    // Note: Not all X11 ops have direct Cairo equivalents
    switch (op)
    {
        case GXcopy:
            return CAIRO_OPERATOR_SOURCE;

        case GXxor:
            return CAIRO_OPERATOR_XOR;

        case GXclear:
            return CAIRO_OPERATOR_CLEAR;

        case GXand:
            // Cairo doesn't have AND, use DEST_IN as closest
            return CAIRO_OPERATOR_DEST_IN;

        case GXor:
            return CAIRO_OPERATOR_ADD;

        case GXnoop:
            return CAIRO_OPERATOR_DEST;

        // For operations without direct Cairo equivalents, use SOURCE
        case GXinvert:
        case GXandReverse:
        case GXandInverted:
        case GXequiv:
        case GXorReverse:
        case GXcopyInverted:
        case GXorInverted:
        case GXnand:
        case GXnor:
        case GXset:
        default:
            return CAIRO_OPERATOR_SOURCE;
    }
}

// -- tperry 15-11-2025: GTK3 - rewrite to use Cairo instead of GdkGC and gdk_draw_drawable
void MCScreenDC::copyarea(Drawable s, Drawable d, int2 depth,
                          int2 sx, int2 sy,  uint2 sw, uint2 sh, int2 dx,
                          int2 dy, uint4 rop)
{
	if (s == DNULL || d == DNULL)
		return;

    assert(rop <= GXset);

    // Create Cairo context for destination
    cairo_t *cr = gdk_cairo_create((GdkWindow*)d);

    // Set the operator based on rop
    if (rop != GXcopy)
        cairo_set_operator(cr, XOpToGdkOp(rop));
    else
        cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);

    // Set source from the source drawable
    gdk_cairo_set_source_window(cr, (GdkWindow*)s, dx - sx, dy - sy);

    // Paint the rectangle
    cairo_rectangle(cr, dx, dy, sw, sh);
    cairo_fill(cr);

    cairo_destroy(cr);
}

MCBitmap *MCScreenDC::createimage(uint16_t depth, uint16_t width, uint16_t height, bool set, uint8_t value)
{
    // NOTE:
    //  The specified depth is completely ignored and a 32-bit RGBA image is
    //  always created. GdkPixbuf doesn't work well with 1-bit images.

    // Create a new bitmap
    GdkPixbuf *t_image;
    t_image = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);

    if (set && t_image != nil)
        memset(gdk_pixbuf_get_pixels(t_image), value, gdk_pixbuf_get_byte_length(t_image));

    return (MCBitmap*)t_image;
}

void MCScreenDC::destroyimage(MCBitmap *image)
{
    if (image != NULL && G_IS_OBJECT(image))
        g_object_unref(image);
}

void MCScreenDC::putimage(Drawable d, MCBitmap *source, int2 sx, int2 sy,
                          int2 dx, int2 dy, uint2 w, uint2 h)
{
	if (d == DNULL)
		return;

    // If we use gdk_draw_pixbuf, the pixbuf gets blended with the existing
    // contents of the window - something that we definitely do not want. We
    // need to use Cairo directly to do the drawing to the window surface.
    cairo_t *t_cr = gdk_cairo_create((GdkWindow*)d);
    cairo_set_operator(t_cr, CAIRO_OPERATOR_SOURCE);
    cairo_rectangle(t_cr, dx, dy, w, h);
    cairo_clip(t_cr);
    gdk_cairo_set_source_pixbuf(t_cr, source, dx-sx, dy-sy);
    cairo_paint(t_cr);
    cairo_destroy(t_cr);
}

// -- tperry 15-11-2025: GTK3 - gdk_pixbuf_get_from_drawable replaced with gdk_pixbuf_get_from_window
MCBitmap *MCScreenDC::getimage(Drawable d, int2 x, int2 y, uint2 w, uint2 h)
{
	GdkPixbuf *t_image;
    t_image = gdk_pixbuf_get_from_window((GdkWindow*)d, x, y, w, h);
    return (MCBitmap*)t_image;
}

void MCScreenDC::flipimage(MCBitmap *image, int2 byte_order, int2 bit_order)
{
    // Not needed
}

// -- tperry 15-11-2025: GTK3 - GdkGC removed, setfunction is now a no-op
void MCScreenDC::setfunction(uint4 rop)
{
    // In GTK3, drawing functions use Cairo operators set per-operation
    // This function is now a no-op
}

// -- tperry 15-11-2025: GTK3 - gdk_x11_drawable_get_xid replaced with gdk_x11_window_get_xid
uintptr_t MCScreenDC::dtouint(Drawable d)
{
	// Return the XID
    return d != DNULL ? x11::gdk_x11_window_get_xid((GdkWindow*)d) : 0;
}

Boolean MCScreenDC::uinttowindow(uintptr_t id, Window &w)
{
    // Look up the XID in GDK's window table
    w = x11::gdk_x11_window_lookup_for_display(dpy, id);
	return True;
}

void MCScreenDC::getbeep(uint4 which, int4& r_value)
{
    // There doesn't seem to be a way to do this with GDK...
    x11::XKeyboardState values;
    x11::XGetKeyboardControl(x11::gdk_x11_display_get_xdisplay(dpy), &values);
	switch (which)
	{
	case P_BEEP_LOUDNESS:
		r_value = values.bell_percent;
		break;
	case P_BEEP_PITCH:
		r_value = values.bell_pitch;
		break;
	case P_BEEP_DURATION:
		r_value = values.bell_duration;
		break;
	}
}


void MCScreenDC::setbeep(uint4 which, int4 beep)
{
	// There doesn't seem to be a way to do this with GDK...
    x11::XKeyboardControl control;
	switch (which)
	{
	case P_BEEP_LOUDNESS:
		beep = MCU_min(beep, 100);
		control.bell_percent = beep;
        x11::XChangeKeyboardControl(x11::gdk_x11_display_get_xdisplay(dpy), KBBellPercent, &control);
		break;
	case P_BEEP_PITCH:
		control.bell_pitch = beep;
		x11::XChangeKeyboardControl(x11::gdk_x11_display_get_xdisplay(dpy), KBBellPitch, &control);
		break;
	case P_BEEP_DURATION:
		control.bell_duration = beep;
		x11::XChangeKeyboardControl(x11::gdk_x11_display_get_xdisplay(dpy), KBBellDuration, &control);

		break;
	}
}

MCNameRef MCScreenDC::getvendorname(void)
{
	return *vendorname;
}

uint2 MCScreenDC::getpad()
{
	// No longer seems to be necessary
    return 32;
}

Window MCScreenDC::getroot()
{
	return gdk_screen_get_root_window(gdk_display_get_default_screen(dpy));
}

MCImageBitmap *MCScreenDC::snapshot(MCRectangle &r, uint4 window, MCStringRef displayname, MCPoint *size)
{
	GdkDisplay* t_display;
    GdkScreen* t_screen;
    GdkWindow* t_root;

    // Are we connecting to a different display?
    if (displayname != nil)
    {
        // Create a new connection and get the corresponding screen and root
        MCAutoStringRefAsSysString t_displayname;
        t_displayname.Lock(displayname);

        if ((t_display = gdk_display_open(*t_displayname)) == NULL)
        {
            // Could not open the requested display
            return NULL;
        }

        t_screen = gdk_display_get_default_screen(t_display);
        t_root = gdk_screen_get_root_window(t_screen);
    }
    else
    {
        // Use the current connection
        t_display = dpy;
        t_screen = gdk_display_get_default_screen(t_display);
        t_root = gdk_screen_get_root_window(t_screen);
    }

    // Ensure that we are up-to-date with the window server
    gdk_display_sync(t_display);

    // Are we going to display a selection rectangle?
    if (window == 0 && r.x == -32768)
    {
        // Switch to a box drawing cursor and take control of the pointer
		// MDW bugfix_17257
        // GdkCursor *t_cursor = gdk_cursor_new(GDK_PLUS);
        GdkCursor *t_cursor = gdk_cursor_new_from_name(gdk_display_get_default(), "crosshair");
        GdkSeat *t_seat = gdk_display_get_default_seat(t_display);
        if (gdk_seat_grab(t_seat, t_root,
                          GDK_SEAT_CAPABILITY_POINTER,
                          FALSE, t_cursor, NULL, NULL, NULL) != GDK_GRAB_SUCCESS)
        {
            // Could not grab the pointer
            return NULL;
        }

        // -- tperry 15-11-2025: GTK3 - prepare for drawing selection rectangle with Cairo
        MCRectangle t_rect(kMCEmptyRectangle);
        // Create a Cairo context for XOR drawing
        cairo_t *t_cr = gdk_cairo_create(t_root);
        cairo_set_operator(t_cr, CAIRO_OPERATOR_XOR);
        cairo_set_source_rgb(t_cr, 1.0, 1.0, 1.0);  // White for XOR
        cairo_set_line_width(t_cr, 1.0);
        int16_t t_start_x = 0;
        int16_t t_start_y = 0;
        bool t_drawing = false;
        bool t_done = false;

        // Minature event loop for handling mouse and key events while selecting
        while (!t_done)
        {
            gdk_display_sync(t_display);

            // Place all events onto the pending event queue
            EnqueueGdkEvents();

            bool t_queue = false;
            GdkEvent *t_event = NULL;
            if (pendingevents != NULL)
            {
                // Get the next event from the queue
                t_event = gdk_event_copy(pendingevents->event);
                MCEventnode *tptr = (MCEventnode *)pendingevents->remove(pendingevents);
                delete tptr;
            }

            // If there are no events, actively wait for one
            if (t_event == NULL)
            {
                g_main_context_iteration(NULL, TRUE);
                continue;
            }

            // Various type casts of the event structure
            GdkEventKey *t_event_key = (GdkEventKey*)t_event;
            GdkEventMotion *t_event_motion = (GdkEventMotion*)t_event;
            GdkEventButton *t_event_button = (GdkEventButton*)t_event;

            // What is the event?
            switch (t_event->type)
            {
                case GDK_EXPOSE:
                    // Put the event back on the queue and do an expose
                    gdk_event_put(t_event);
                    MCscreen->expose();
                    break;

                case GDK_KEY_PRESS:
                    MCeventtime = gdk_event_get_time(t_event);
                    if (t_event_key->keyval == GDK_KEY_Escape)
                    {
                        if (t_drawing)
                        {
                            // -- tperry 15-11-2025: GTK3 - draw selection rectangle with Cairo
                            cairo_rectangle(t_cr, t_rect.x + 0.5, t_rect.y + 0.5, t_rect.width - 1, t_rect.height - 1);
                            cairo_stroke(t_cr);
                            x11::gdk_x11_ungrab_server();
                        }

                        // End the selection
                        t_done = true;
                    }
                    break;

                case GDK_MOTION_NOTIFY:
                    MCeventtime = gdk_event_get_time(t_event);
                    if (t_drawing)
                    {
                        // -- tperry 15-11-2025: GTK3 - update selection rectangle with Cairo
                        // Erase old rectangle (XOR mode)
                        cairo_rectangle(t_cr, t_rect.x + 0.5, t_rect.y + 0.5, t_rect.width - 1, t_rect.height - 1);
                        cairo_stroke(t_cr);
                        // Draw new rectangle
                        t_rect = MCU_compute_rect(t_start_x, t_start_y, t_event_motion->x, t_event_motion->y);
                        cairo_rectangle(t_cr, t_rect.x + 0.5, t_rect.y + 0.5, t_rect.width - 1, t_rect.height - 1);
                        cairo_stroke(t_cr);
                    }
                    break;

                case GDK_BUTTON_PRESS:
					// MDW bugfix_17257
					// x11::gdk_x11_grab_server();
                    MCeventtime = gdk_event_get_time(t_event);
                    t_start_x = t_event_button->x;
                    t_start_y = t_event_button->y;
                    t_rect = MCU_compute_rect(t_start_x, t_start_y, t_start_x, t_start_y);
                    // -- tperry 15-11-2025: GTK3 - draw initial selection rectangle with Cairo
                    cairo_rectangle(t_cr, t_rect.x + 0.5, t_rect.y + 0.5, t_rect.width - 1, t_rect.height - 1);
                    cairo_stroke(t_cr);
                    t_drawing = true;
                    break;

                case GDK_BUTTON_RELEASE:
                    MCeventtime = gdk_event_get_time(t_event);
					// MDW bugfix_17257
                    // setmods(t_event_button->state, 0, t_event_button->button, True);
                    // -- tperry 15-11-2025: GTK3 - erase final selection rectangle with Cairo
                    cairo_rectangle(t_cr, t_rect.x + 0.5, t_rect.y + 0.5, t_rect.width - 1, t_rect.height - 1);
                    cairo_stroke(t_cr);
                    r = MCU_compute_rect(t_start_x, t_start_y, t_event_button->x, t_event_button->y);
                    if (r.width < 4 && r.height < 4)
                        r.width = r.height = 0;
					// MDW bugfix_17257
                    // x11::gdk_x11_ungrab_server();
                    t_done = true;
                    break;

                case GDK_GRAB_BROKEN:
                    t_done = true;
                    break;

				default:
					/* Ignore this event */
					break;
            }

            // The event needs to be released
            gdk_event_free(t_event);
        }

        // -- tperry 15-11-2025: GTK3 - release grabs and Cairo resources
        gdk_seat_ungrab(gdk_display_get_default_seat(t_display));
        g_object_unref(t_cursor);  // GTK3: gdk_cursor_unref removed, use g_object_unref
        cairo_destroy(t_cr);  // Clean up Cairo context
        gdk_display_flush(t_display);
    }
    if (r.x == -32768)
    {
        r.x = r.y = 0;
    }
    if (window != 0 || r.width == 0 || r.height == 0)
    {
        int rx, ry, wx, wy;
        unsigned int kb;
        GdkWindow *t_child;
        if (window == 0)
        {
            // This is done using Xlib calls because there are no suitable
            // equivalents in GDK (it can only return windows beloning to this
            // application when mapping coordinates to a window).
            // -- tperry 15-11-2025: GTK3 - use gdk_x11_window_get_xid
            x11::Display* dpy;
            dpy = x11::gdk_x11_display_get_xdisplay(t_display);
            x11::Window root;
            root = x11::gdk_x11_window_get_xid(t_root);

            // Grab the server to prevent pointer movement during this operation
            x11::XGrabServer(dpy);

            // Get the window under the pointer
            x11::Window troot, child;
            x11::XQueryPointer(dpy, root, &troot, &child, &rx, &ry, &wx, &wy, &kb);

            // Warp the pointer to the coordinates specified by the user and get
            // the child window at that point
			int2 oldx = rx;
			int2 oldy = ry;
            x11::XWarpPointer(dpy, None, x11::gdk_x11_window_get_xid(getroot()), 0, 0, 0, 0, r.x, r.y);
            x11::XQueryPointer(dpy, root, &troot, &child, &rx, &ry, &wx, &wy, &kb);

            // If no child window, use the screen's root window instead
            if (child == None)
				child = troot;

            // If the control key is not held down, do the query again
            //
            // ??? Why is this here? I can see it being necessary in the case
            // that root and troot are not on the same screen but why does it
            // depend on whether the control key is held down...?
			if (!(MCmodifierstate & MS_CONTROL))
			{
                x11::Window oldchild = child;
                x11::XQueryPointer(dpy, child, &troot, &child, &rx, &ry, &wx, &wy, &kb);
				if (child == None)
					child = oldchild;
			}

            // Convert the X11 window to a GDK window
            t_child = x11::gdk_x11_window_foreign_new_for_display(t_display, child);

            // -- tperry 15-11-2025: GTK3 - use gdk_x11_window_get_xid
            // Restore the pointer location and ungrab the server
            x11::XWarpPointer(dpy, None, x11::gdk_x11_window_get_xid(getroot()), 0, 0, 0, 0, oldx, oldy);
            x11::XUngrabServer(dpy);
        }
        else
        {
            // Convert the X11 window to a GDK window
            t_child = x11::gdk_x11_window_foreign_new_for_display(t_display, window);
        }

        // Get the position and dimensions of the window
        gint x, y;
        unsigned int w, h;
        w = gdk_window_get_width(t_child);
        h = gdk_window_get_height(t_child);
        gdk_window_get_origin(t_child, &x, &y);

        // Are we wanting the whole window or just a specified rectangle?
        if (r.width == 0 || r.height == 0)
        {
            // Whole window
            MCU_set_rect(r, x, y, w, h);
            r = MCU_clip_rect(r, 0, 0, gdk_screen_get_width(t_screen), gdk_screen_get_height(t_screen));
        }
        else
        {
            // Rectangle within window
            r.x += x;
            r.y += y;
        }

        // Release the reference to the child window that we created
        g_object_unref(t_child);
    }

    // Clip to the device boundaries
    r = MCU_clip_rect(r, 0, 0, device_getwidth(), device_getheight());

    // Nothing more to do if the rectangle is degenerate
    if (r.width == 0 || r.height == 0)
        return NULL;

    // -- tperry 15-11-2025: GTK3 - use gdk_pixbuf_get_from_window instead
    // Get the snapshot from the root window.
    // FG-2014-08-05: [[ Bugfix 13032 ]]
    // GTK3: gdk_pixbuf_get_from_window always returns RGBA pixbuf
    GdkPixbuf *t_image;
    t_image = gdk_pixbuf_get_from_window(t_root, r.x, r.y, r.width, r.height);

    // Abort if the pixbuf could not be captured
    if (t_image == NULL)
        return NULL;

    // Close the connection to the display
    if (displayname != nil)
    {
        gdk_display_close(t_display);
    }

    // Turn the GDK pixbuf into an engine bitmap
    MCImageBitmap *t_bitmap = nil;
    /* UNCHECKED */ MCImageBitmapCreateWithGdkPixbuf(t_image, t_bitmap);
    g_object_unref(t_image);

    // Do any scaling that is required to satisfy the specified size
	if (size != nil &&
	    ((uint32_t) size -> x != t_bitmap -> width ||
	     (uint32_t) size -> y != t_bitmap -> height))
	{
		MCImageBitmap *t_new_bitmap;
		MCImageScaleBitmap(t_bitmap, size -> x, size -> y, INTERPOLATION_BILINEAR, t_new_bitmap);
		MCImageFreeBitmap(t_bitmap);
		t_bitmap = t_new_bitmap;
	}

	return t_bitmap;
}


void MCScreenDC::hidebackdrop(bool p_hide)
{
	if (!MChidebackdrop)
		return;

	if (!backdrop_active && !backdrop_hard)
		return;

	if ( backdrop == DNULL)
		return ;

	if ( p_hide )
	{
		disablebackdrop(false);
	}
	else
	{
		enablebackdrop(false);
	}
}


// Paint the current backdrop color/pattern onto a GdkWindow.
// p_w / p_h are the ACTUAL X11 window dimensions (not GDK's stale cache).
//
// For solid colours we use XFillRectangle with a freshly-created GC that has
// no clip mask.  gdk_cairo_create() inherits GDK's visible-region clip for the
// window, which excludes any area covered by override_redirect windows above us
// (e.g. the GNOME Shell top bar on the secondary monitor).  cairo_reset_clip()
// removes the Cairo-level clip stack but NOT the underlying X11 GC clip that
// GDK set, so those regions are never painted via Cairo.  XFillRectangle with
// GCClipMask=None draws directly to the window's compositor backing pixmap and
// is not subject to any visible-region clip.
static void paint_backdrop_gdk_window(GdkWindow *p_win,
                                       gint p_w, gint p_h,
                                       const MCColor &p_color,
                                       Pixmap p_pixmap)
{
    if (!p_win)
        return;

    GdkDisplay   *t_gdkdpy = gdk_window_get_display(p_win);
    x11::Display *t_xdpy   = x11::gdk_x11_display_get_xdisplay(t_gdkdpy);
    x11::Window   t_xwin   = x11::gdk_x11_window_get_xid(p_win);

    if (p_pixmap == DNULL)
    {
        // Solid colour path: XFillRectangle with GCClipMask = None.
        // XFillRectangle uses physical pixels; p_w/p_h are GDK logical pixels,
        // so multiply by the window's scale factor for HiDPI correctness.
        int t_scale = gdk_window_get_scale_factor(p_win);
        unsigned long t_pixel =
            0xFF000000UL |
            ((unsigned long)(p_color.red   >> 8) << 16) |
            ((unsigned long)(p_color.green >> 8) <<  8) |
            ((unsigned long)(p_color.blue  >> 8));

        x11::XGCValues t_gcv = {};
        t_gcv.foreground = t_pixel;
        t_gcv.clip_mask  = None;
        x11::GC t_gc = x11::XCreateGC(t_xdpy, (x11::Drawable)t_xwin,
                                        GCForeground | GCClipMask, &t_gcv);
        x11::XFillRectangle(t_xdpy, (x11::Drawable)t_xwin, t_gc,
                            0, 0,
                            (unsigned)(p_w * t_scale), (unsigned)(p_h * t_scale));
        x11::XFreeGC(t_xdpy, t_gc);
        x11::XFlush(t_xdpy);
        return;
    }

    // Pixmap path: use Cairo (pixmaps are already in X11 device space so
    // the visible-region clip issue does not affect correctness here).
    cairo_t *cr = gdk_cairo_create(p_win);
    cairo_reset_clip(cr);
    cairo_rectangle(cr, 0, 0, (double)p_w, (double)p_h);
    cairo_clip(cr);
    cairo_set_source_surface(cr, (cairo_surface_t *)p_pixmap, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
}

void MCScreenDC::createbackdrop_window(void)
{
    // Destroy any stale extra backdrops first.
    for (Window t_extra : s_extra_backdrops)
    {
        if (t_extra != DNULL)
        {
            gdk_window_hide(t_extra);
            gdk_window_destroy(t_extra);
        }
    }
    s_extra_backdrops.clear();

    GdkScreen *t_screen = gdk_display_get_default_screen(MCdpy);
    int t_nmon = gdk_screen_get_n_monitors(t_screen);
    if (t_nmon < 1) t_nmon = 1;

    // On Marco/Muffin with multiple monitors: create ONE window spanning the
    // bounding box of all monitor workareas instead of per-monitor windows.
    // Extra backdrop windows on Marco are stuck at the top of the WM stacking
    // order regardless of EWMH restack requests (ConfigureRequest(Below) and
    // _NET_RESTACK_WINDOW are both ignored).  A single spanning window avoids
    // extra backdrops entirely; Marco treats it like the primary backdrop
    // (META_LAYER_NORMAL, layer 4), which sits correctly below HXT stacks in
    // META_LAYER_TOP (layer 6) granted by keep_above.
    //
    // On Mutter, per-monitor windows are required: the compositor captures
    // textures per-monitor and constrains spanning windows to the monitor
    // containing their top-left corner.
    if (s_wm_is_marco && t_nmon > 1)
    {
        // Marco/Metacity only: create ONE window spanning the bounding box of
        // all monitor workareas.  Extra backdrop windows on Marco are stuck at
        // the top of the WM stacking order regardless of EWMH restack requests,
        // so we avoid them entirely.  Muffin (Cinnamon) is a Mutter fork and
        // handles per-monitor extra backdrops correctly — do NOT apply this path
        // there (s_wm_is_marco is false for Muffin).
        GdkRectangle t_g0 = {0, 0, 1920, 1080};
        gdk_screen_get_monitor_workarea(t_screen, 0, &t_g0);
        int t_bbox_x1 = t_g0.x, t_bbox_y1 = t_g0.y;
        int t_bbox_x2 = t_g0.x + t_g0.width;
        int t_bbox_y2 = t_g0.y + t_g0.height;
        for (int i = 1; i < t_nmon; i++)
        {
            GdkRectangle t_g = {0, 0, 1920, 1080};
            gdk_screen_get_monitor_workarea(t_screen, i, &t_g);
            if (t_g.x < t_bbox_x1) t_bbox_x1 = t_g.x;
            if (t_g.y < t_bbox_y1) t_bbox_y1 = t_g.y;
            if (t_g.x + t_g.width  > t_bbox_x2) t_bbox_x2 = t_g.x + t_g.width;
            if (t_g.y + t_g.height > t_bbox_y2) t_bbox_y2 = t_g.y + t_g.height;
        }

        GdkWindowAttr gdkwa;
        gdkwa.event_mask = GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
            | GDK_FOCUS_CHANGE_MASK | GDK_POINTER_MOTION_MASK
            | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_EXPOSURE_MASK;
        gdkwa.x       = t_bbox_x1;
        gdkwa.y       = t_bbox_y1;
        gdkwa.width   = t_bbox_x2 - t_bbox_x1;
        gdkwa.height  = t_bbox_y2 - t_bbox_y1;
        gdkwa.wclass  = GDK_INPUT_OUTPUT;
        gdkwa.visual  = getvisual();
        gdkwa.window_type = GDK_WINDOW_TOPLEVEL;

        Window t_win = gdk_window_new(getroot(), &gdkwa,
                                      GDK_WA_VISUAL | GDK_WA_X | GDK_WA_Y);
        backdrop = t_win;
        s_primary_backdrop = t_win;
        // s_extra_backdrops intentionally stays empty.
    }
    else
    {
        // Mutter (or single monitor): one window per physical monitor, sized to
        // that monitor's workarea.  GDK_WA_X|GDK_WA_Y pass x,y to XCreateWindow
        // so Mutter assigns the window to the correct monitor at MapRequest time.
        for (int i = 0; i < t_nmon; i++)
        {
            GdkRectangle t_geom = {0, 0, 1920, 1080};
            gdk_screen_get_monitor_workarea(t_screen, i, &t_geom);

            GdkWindowAttr gdkwa;
            gdkwa.event_mask = GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
                | GDK_FOCUS_CHANGE_MASK | GDK_POINTER_MOTION_MASK
                | GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_EXPOSURE_MASK;
            gdkwa.x       = t_geom.x;
            gdkwa.y       = t_geom.y;
            gdkwa.width   = t_geom.width;
            gdkwa.height  = t_geom.height;
            gdkwa.wclass  = GDK_INPUT_OUTPUT;
            gdkwa.visual  = getvisual();
            gdkwa.window_type = GDK_WINDOW_TOPLEVEL;

            Window t_win = gdk_window_new(getroot(), &gdkwa,
                                          GDK_WA_VISUAL | GDK_WA_X | GDK_WA_Y);

            if (i == 0)
            {
                backdrop = t_win;
                s_primary_backdrop = t_win;
            }
            else
                s_extra_backdrops.push_back(t_win);
        }
    }

    gdk_display_sync(MCdpy);

    // Install root-window _NET_ACTIVE_WINDOW filter (once) so we can detect
    // when the user switches to another application via ALT+TAB.
    hxt_install_active_window_filter(MCdpy);
}


void MCScreenDC::enablebackdrop(bool p_hard)
{
	bool t_error;
	t_error = false;

	if (p_hard && backdrop_hard)
		return;

	if (!p_hard && backdrop_active)
		return;

	if (p_hard)
		backdrop_hard = true;
	else
		backdrop_active = True;

	// Destroy and recreate the primary backdrop window so it starts with
	// fresh WM state, matching how Windows re-creates via initialisebackdrop().
	// The pre-existing window (from configurebackdrop) carries stale hints
	// that prevent it from showing correctly on the primary monitor.
	if (backdrop != DNULL)
	{
		gdk_window_hide(backdrop);
		gdk_window_destroy(backdrop);
		backdrop = DNULL;
        s_primary_backdrop = nullptr;
	}
	createbackdrop_window();

	t_error = (backdrop == DNULL);

	if (!t_error)
	{

        // Helper: configure and show one managed backdrop GdkWindow at (x,y,w,h).
        // Strategy: pre-position the window via XMoveResizeWindow while it is
        // still unmapped (direct X11, no WM ConfigureRequest).  When gdk_window_show
        // triggers the MapRequest, Mutter reads XGetGeometry → (x,y) and assigns
        // the window to the monitor containing (x,y).  Since it is already on the
        // correct monitor, any subsequent same-monitor corrective move is accepted.
        auto show_backdrop_win = [&](GdkWindow *p_win, bool p_is_extra, gint x, gint y, gint w, gint h)
        {
            x11::Display *t_xdpy = x11::gdk_x11_display_get_xdisplay(dpy);
            x11::Window   t_xwin = x11::gdk_x11_window_get_xid(p_win);

            // GDK coordinates are logical (scaled) pixels; X11 direct calls need
            // physical pixels.  Multiply by the GDK scale factor (1 on non-HiDPI,
            // 2 on 2× HiDPI, etc.) for all raw X11 geometry operations.
            int t_scale = gdk_window_get_scale_factor(p_win);

            // Pre-position while unmapped: direct X11, no WM interference.
            x11::XMoveResizeWindow(t_xdpy, t_xwin,
                                   x * t_scale, y * t_scale,
                                   (unsigned)(w * t_scale), (unsigned)(h * t_scale));
            x11::XFlush(t_xdpy);

            // Suppress decorations so the backdrop is a bare window.
            gdk_window_set_decorations(p_win, (GdkWMDecoration)0);
            gdk_window_set_functions(p_win, (GdkWMFunction)0);
            // No keyboard focus for the backdrop.
            gdk_window_set_accept_focus(p_win, FALSE);
            // Exclude from taskbar/pager so the WM doesn't apply normal placement.
            gdk_window_set_skip_taskbar_hint(p_win, TRUE);
            gdk_window_set_skip_pager_hint(p_win, TRUE);

            // Do NOT set _NET_WM_STATE_ABOVE: we keep everything in the NORMAL
            // layer so the WM can freely raise other apps above the backdrop via
            // ALT+TAB.  Z-order is managed reactively via _NET_ACTIVE_WINDOW.
            //
            // On Muffin, set _NET_WM_WINDOW_TYPE_NORMAL explicitly for all
            // backdrop windows to prevent Muffin from inferring a higher layer
            // from the no-decoration Motif hints.
            if (s_wm_is_muffin)
            {
                x11::Atom t_wm_type   = x11::XInternAtom(t_xdpy, "_NET_WM_WINDOW_TYPE", False);
                x11::Atom t_wm_normal = x11::XInternAtom(t_xdpy, "_NET_WM_WINDOW_TYPE_NORMAL", False);
                x11::XChangeProperty(t_xdpy, t_xwin, t_wm_type,
                                     (x11::Atom)4 /*XA_ATOM*/, 32,
                                     PropModeReplace,
                                     (unsigned char*)&t_wm_normal, 1);
                x11::XFlush(t_xdpy);
            }

            GdkRGBA t_rgba;
            t_rgba.red   = backdropcolor.red   / 65535.0;
            t_rgba.green = backdropcolor.green / 65535.0;
            t_rgba.blue  = backdropcolor.blue  / 65535.0;
            t_rgba.alpha = 1.0;
            gdk_window_set_background_rgba(p_win, &t_rgba);

            // Map and immediately raise to cover other windows.
            // No keep_above — we stay in the NORMAL layer so ALT+TAB works.
            gdk_window_show_unraised(p_win);
            gdk_window_raise(p_win);

            // Reinforce geometry via GDK after mapping — some WMs apply their
            // own placement when transitioning a window to the ABOVE layer.
            gdk_window_move_resize(p_win, x, y, w, h);
            gdk_display_sync(dpy);

            paint_backdrop_gdk_window(p_win, w, h, backdropcolor, m_backdrop_pixmap);
        };

        // Show backdrop window(s).  On Marco with multiple monitors a single
        // spanning window covers all monitors (s_extra_backdrops is empty);
        // show it at the bounding box of all workareas.  Otherwise show one
        // window per monitor at that monitor's workarea.
        {
            GdkScreen *t_screen = gdk_display_get_default_screen(dpy);
            int t_nmon = gdk_screen_get_n_monitors(t_screen);
            if (t_nmon < 1) t_nmon = 1;

            if (s_wm_is_marco && s_extra_backdrops.empty() && t_nmon > 1)
            {
                // Marco spanning: one window at the bounding box of all workareas.
                GdkRectangle t_g0 = {0, 0, 1920, 1080};
                gdk_screen_get_monitor_workarea(t_screen, 0, &t_g0);
                int t_bbox_x1 = t_g0.x, t_bbox_y1 = t_g0.y;
                int t_bbox_x2 = t_g0.x + t_g0.width;
                int t_bbox_y2 = t_g0.y + t_g0.height;
                for (int i = 1; i < t_nmon; i++)
                {
                    GdkRectangle t_g = {0, 0, 1920, 1080};
                    gdk_screen_get_monitor_workarea(t_screen, i, &t_g);
                    if (t_g.x < t_bbox_x1) t_bbox_x1 = t_g.x;
                    if (t_g.y < t_bbox_y1) t_bbox_y1 = t_g.y;
                    if (t_g.x + t_g.width  > t_bbox_x2) t_bbox_x2 = t_g.x + t_g.width;
                    if (t_g.y + t_g.height > t_bbox_y2) t_bbox_y2 = t_g.y + t_g.height;
                }
                show_backdrop_win(backdrop, false,
                                  t_bbox_x1, t_bbox_y1,
                                  t_bbox_x2 - t_bbox_x1,
                                  t_bbox_y2 - t_bbox_y1);
            }
            else
            {
                for (int i = 0; i < t_nmon; i++)
                {
                    GdkRectangle t_geom = {0, 0, 1920, 1080};
                    gdk_screen_get_monitor_workarea(t_screen, i, &t_geom);
                    Window t_win = (i == 0) ? backdrop
                                 : ((i - 1) < (int)s_extra_backdrops.size()
                                    ? s_extra_backdrops[i - 1] : DNULL);
                    if (t_win != DNULL)
                        show_backdrop_win(t_win, i > 0, t_geom.x, t_geom.y,
                                          t_geom.width, t_geom.height);
                }
            }
        }
        gdk_display_flush(dpy);

        // On Muffin/Marco: start a polling timer that reads
        // _NET_CLIENT_LIST_STACKING and re-raises any HXT stack that has been
        // buried below a backdrop window (e.g. by Muffin's click-to-raise).
        // Without keep_above, Muffin can raise the backdrop above stacks on
        // click — the timer corrects this.  Not needed on Mutter because
        // transient_for guarantees stacks stay above the backdrop.
        if (s_wm_is_muffin && s_stacking_timer_id == 0)
            s_stacking_timer_id = g_timeout_add(250, hxt_check_stacking, nullptr);
	}
	else
	{
		backdrop_active = False;
		//finalisebackdrop();
	}
    // MDW [17323] - refresh *after* the gdk calls
    MCstacks -> refresh();

    // Deferred repaint + position check (200ms): paints all backdrops after the
    // GLib main loop has had a chance to run.
    // to the secondary monitor, so backdrop[0] is no longer occluded on primary.
    // GNOME Shell's compositor skips capturing textures for fully-occluded windows,
    // so the initial XFillRectangle on backdrop[0] (done while backdrop[1] was above
    // it at 0,0) is never picked up by the compositor.  Re-painting here, after
    // backdrop[1] has moved away, ensures the compositor captures the correct pixels.
    if (s_primary_backdrop)
    {
        // Holds geometry targets (in PHYSICAL pixels) for up to 4 backdrop windows
        // so the 200ms timer can verify placement and send corrective messages.
        // XGetGeometry and _NET_MOVERESIZE_WINDOW both use physical pixels, so
        // we pre-multiply GDK logical coords by the window scale factor here.
        struct BackdropDiag {
            x11::Display *xdpy;
            x11::Window   xwin[4];
            int           tx[4], ty[4], tw[4], th[4]; // target geometry (physical px)
            int           count;
            unsigned long pixel;
        };
        unsigned long t_def_pixel =
            0xFF000000UL |
            ((unsigned long)(backdropcolor.red   >> 8) << 16) |
            ((unsigned long)(backdropcolor.green >> 8) <<  8) |
            ((unsigned long)(backdropcolor.blue  >> 8));
        auto *bd = new BackdropDiag{};
        bd->xdpy  = x11::gdk_x11_display_get_xdisplay(dpy);
        bd->pixel = t_def_pixel;
        bd->count = 0;
        {
            GdkScreen *t_sc = gdk_display_get_default_screen(dpy);
            int t_nmon_bd = gdk_screen_get_n_monitors(t_sc);
            if (t_nmon_bd < 1) t_nmon_bd = 1;
            int t_s0 = gdk_window_get_scale_factor(s_primary_backdrop);
            bd->xwin[0] = x11::gdk_x11_window_get_xid(s_primary_backdrop);
            bd->count   = 1;

            if (s_wm_is_marco && s_extra_backdrops.empty() && t_nmon_bd > 1)
            {
                // Marco spanning: target geometry is the bounding box of all workareas.
                GdkRectangle t_g0 = {0, 0, 1920, 1080};
                gdk_screen_get_monitor_workarea(t_sc, 0, &t_g0);
                int t_bbox_x1 = t_g0.x, t_bbox_y1 = t_g0.y;
                int t_bbox_x2 = t_g0.x + t_g0.width;
                int t_bbox_y2 = t_g0.y + t_g0.height;
                for (int i = 1; i < t_nmon_bd; i++)
                {
                    GdkRectangle t_g = {0, 0, 1920, 1080};
                    gdk_screen_get_monitor_workarea(t_sc, i, &t_g);
                    if (t_g.x < t_bbox_x1) t_bbox_x1 = t_g.x;
                    if (t_g.y < t_bbox_y1) t_bbox_y1 = t_g.y;
                    if (t_g.x + t_g.width  > t_bbox_x2) t_bbox_x2 = t_g.x + t_g.width;
                    if (t_g.y + t_g.height > t_bbox_y2) t_bbox_y2 = t_g.y + t_g.height;
                }
                bd->tx[0] = t_bbox_x1 * t_s0; bd->ty[0] = t_bbox_y1 * t_s0;
                bd->tw[0] = (t_bbox_x2 - t_bbox_x1) * t_s0;
                bd->th[0] = (t_bbox_y2 - t_bbox_y1) * t_s0;
            }
            else
            {
                // Per-monitor: primary targets monitor 0 workarea.
                GdkRectangle t_g0 = {0, 0, 1920, 1080};
                gdk_screen_get_monitor_workarea(t_sc, 0, &t_g0);
                bd->tx[0] = t_g0.x * t_s0; bd->ty[0] = t_g0.y * t_s0;
                bd->tw[0] = t_g0.width * t_s0; bd->th[0] = t_g0.height * t_s0;
                // Extra backdrops (monitors 1..N)
                for (int i = 0; i < (int)s_extra_backdrops.size() && bd->count < 4; i++)
                {
                    if (s_extra_backdrops[i])
                    {
                        GdkRectangle t_gi = {0, 0, 1920, 1080};
                        gdk_screen_get_monitor_workarea(t_sc, i + 1, &t_gi);
                        int t_si = gdk_window_get_scale_factor(s_extra_backdrops[i]);
                        int k = bd->count;
                        bd->xwin[k] = x11::gdk_x11_window_get_xid(s_extra_backdrops[i]);
                        bd->tx[k]   = t_gi.x * t_si; bd->ty[k] = t_gi.y * t_si;
                        bd->tw[k]   = t_gi.width * t_si; bd->th[k] = t_gi.height * t_si;
                        bd->count++;
                    }
                }
            }
        }
        g_timeout_add_full(G_PRIORITY_DEFAULT, 200,
            +[](gpointer p) -> gboolean {
                auto *d = static_cast<BackdropDiag*>(p);

                x11::Atom t_mra = x11::XInternAtom(d->xdpy,
                                                    "_NET_MOVERESIZE_WINDOW", False);
                for (int i = 0; i < d->count; i++)
                {
                    // Check actual position; send corrective move if Mutter shifted it.
                    x11::Window t_root_r; int t_gx = 0, t_gy = 0;
                    unsigned int t_gw = 0, t_gh = 0, t_gbw = 0, t_gd_v = 0;
                    x11::XGetGeometry(d->xdpy, (x11::Drawable)d->xwin[i],
                                      &t_root_r, &t_gx, &t_gy,
                                      &t_gw, &t_gh, &t_gbw, &t_gd_v);

                    if (t_gx != d->tx[i] || t_gy != d->ty[i] ||
                        (int)t_gw != d->tw[i] || (int)t_gh != d->th[i])
                    {
                        x11::XEvent t_ev = {};
                        t_ev.xclient.type         = ClientMessage;
                        t_ev.xclient.display      = d->xdpy;
                        t_ev.xclient.window       = d->xwin[i];
                        t_ev.xclient.message_type = t_mra;
                        t_ev.xclient.format       = 32;
                        t_ev.xclient.data.l[0]   = (1<<8)|(1<<9)|(1<<10)|(1<<11)|(2<<12);
                        t_ev.xclient.data.l[1]   = (long)d->tx[i];
                        t_ev.xclient.data.l[2]   = (long)d->ty[i];
                        t_ev.xclient.data.l[3]   = (long)d->tw[i];
                        t_ev.xclient.data.l[4]   = (long)d->th[i];
                        x11::XSendEvent(d->xdpy, x11::XDefaultRootWindow(d->xdpy),
                                        False,
                                        SubstructureRedirectMask | SubstructureNotifyMask,
                                        &t_ev);
                    }

                    // Repaint this backdrop.
                    x11::XGCValues t_gcv = {};
                    t_gcv.foreground = d->pixel;
                    t_gcv.clip_mask  = None;
                    x11::GC t_gc = x11::XCreateGC(d->xdpy, (x11::Drawable)d->xwin[i],
                                                   GCForeground | GCClipMask, &t_gcv);
                    x11::XFillRectangle(d->xdpy, (x11::Drawable)d->xwin[i], t_gc,
                                        0, 0, (unsigned)d->tw[i], (unsigned)d->th[i]);
                    x11::XFreeGC(d->xdpy, t_gc);
                }
                x11::XFlush(d->xdpy);

                // Schedule a follow-up repaint at +300ms so the paint happens AFTER
                // any corrective _NET_MOVERESIZE_WINDOW has been processed by Mutter.
                struct RepaintData {
                    x11::Display *xdpy;
                    x11::Window   xwin[4];
                    int           tw[4], th[4];
                    int           count;
                    unsigned long pixel;
                };
                auto *rd = new RepaintData{};
                rd->xdpy  = d->xdpy;
                rd->count = d->count;
                rd->pixel = d->pixel;
                for (int i = 0; i < d->count; i++) {
                    rd->xwin[i] = d->xwin[i];
                    rd->tw[i]   = d->tw[i];
                    rd->th[i]   = d->th[i];
                }
                g_timeout_add_full(G_PRIORITY_DEFAULT, 300,
                    +[](gpointer p2) -> gboolean {
                        auto *rd2 = static_cast<RepaintData*>(p2);
                        for (int i = 0; i < rd2->count; i++) {
                            x11::XGCValues t_gcv2 = {};
                            t_gcv2.foreground = rd2->pixel;
                            t_gcv2.clip_mask  = None;
                            x11::GC t_gc2 = x11::XCreateGC(
                                rd2->xdpy, (x11::Drawable)rd2->xwin[i],
                                GCForeground | GCClipMask, &t_gcv2);
                            x11::XFillRectangle(rd2->xdpy, (x11::Drawable)rd2->xwin[i],
                                                t_gc2, 0, 0,
                                                (unsigned)rd2->tw[i], (unsigned)rd2->th[i]);
                            x11::XFreeGC(rd2->xdpy, t_gc2);
                        }
                        x11::XFlush(rd2->xdpy);
                        return G_SOURCE_REMOVE;
                    },
                    rd,
                    +[](gpointer p2) { delete static_cast<RepaintData*>(p2); });

                return G_SOURCE_REMOVE;
            },
            bd,
            +[](gpointer p) { delete static_cast<BackdropDiag*>(p); });
    }
}

void MCScreenDC::disablebackdrop(bool p_hard)
{
	if (!backdrop_hard && p_hard)
		return;

	if (!backdrop_active && !p_hard)
		return;

	if (p_hard)
		backdrop_hard = false;
	else
		backdrop_active = False;

	if (!backdrop_active && !backdrop_hard)
	{
        // Stop the stacking-order poll timer; no extra backdrops to watch.
        if (s_stacking_timer_id)
        {
            g_source_remove(s_stacking_timer_id);
            s_stacking_timer_id = 0;
        }
		if (backdrop != DNULL)
			gdk_window_hide(backdrop);
        for (Window t_extra : s_extra_backdrops)
            if (t_extra != DNULL)
                gdk_window_hide(t_extra);
		MCstacks -> refresh();
	}

}

bool MCPatternToX11Pixmap(MCPatternRef p_pattern, Pixmap &r_pixmap);
void MCScreenDC::configurebackdrop(const MCColor& p_colour, MCPatternRef p_pattern, MCImage *p_badge)
{
	// -- tperry 15-11-2025: GTK3 - use DNULL for Pixmap comparisons (Pixmap is unsigned long)
	// IM-2014-04-15: [[ Bug 11603 ]] Convert pattern to Pixmap for use with XSetWindowAttributes structure
	freepixmap(m_backdrop_pixmap);
    m_backdrop_pixmap = DNULL;

	if (p_pattern != nil)
		/* UNCHECKED */ MCPatternToX11Pixmap(p_pattern, m_backdrop_pixmap);

	if ( backdrop == DNULL )
		createbackdrop_window();

    if (m_backdrop_pixmap == DNULL)
    {
        backdropcolor = p_colour;
    }
	else
		MCColorSetPixel(backdropcolor, 0);

    if (backdrop == DNULL)
        return;

	// Repaint all per-monitor backdrop windows at their respective sizes.
    {
        GdkScreen *t_screen = gdk_display_get_default_screen(dpy);
        int t_nmon = gdk_screen_get_n_monitors(t_screen);
        if (t_nmon < 1) t_nmon = 1;
        for (int i = 0; i < t_nmon; i++)
        {
            GdkRectangle t_geom = {0, 0, 1920, 1080};
            gdk_screen_get_monitor_geometry(t_screen, i, &t_geom);
            Window t_win = (i == 0) ? backdrop
                         : ((i - 1) < (int)s_extra_backdrops.size()
                            ? s_extra_backdrops[i - 1] : DNULL);
            if (t_win != DNULL)
                paint_backdrop_gdk_window(t_win, t_geom.width, t_geom.height,
                                          backdropcolor, m_backdrop_pixmap);
        }
    }

	MCstacks -> refresh();
}


void MCScreenDC::assignbackdrop(Window_mode p_mode, Window p_window)
{
	if (p_mode >= WM_TOP_LEVEL && p_mode <= WM_SHEET && backdrop != DNULL)
    {
        if (backdrop_active || backdrop_hard)
        {
            // Register in the backdrop stack list (hold a GObject ref so the
            // pointer stays valid until we explicitly remove it).
            auto it = std::find(s_backdrop_stacks.begin(), s_backdrop_stacks.end(),
                                p_window);
            if (it == s_backdrop_stacks.end())
            {
                g_object_ref(p_window);
                s_backdrop_stacks.push_back(p_window);
            }

            // On Mutter: transient_for promotes the stack into the ABOVE layer
            // (transient children inherit their parent's layer).
            // On Marco/Muffin: skip transient_for — it triggers WM group-pull
            // behaviour that fights the spanning-window approach.
            //
            // WM_PALETTE (autocomplete popup etc.) must NOT be made transient
            // for the backdrop: since the ALT+TAB fix the backdrop lives in the
            // NORMAL layer, so transient_for would demote a palette window out
            // of the ABOVE layer (where its own _NET_WM_STATE_ABOVE placed it),
            // causing it to appear behind script editors that stay in ABOVE.
            if (!s_wm_is_muffin && p_mode != WM_PALETTE)
                gdk_window_set_transient_for(p_window, backdrop);
            else
                gdk_property_delete(p_window,
                                    gdk_atom_intern_static_string("WM_TRANSIENT_FOR"));
            // Raise the stack above the backdrop (no keep_above — NORMAL layer).
            gdk_window_raise(p_window);
        }
        else
        {
            // Unregister from backdrop stack list.
            auto it = std::find(s_backdrop_stacks.begin(), s_backdrop_stacks.end(),
                                p_window);
            if (it != s_backdrop_stacks.end())
            {
                g_object_unref(*it);
                s_backdrop_stacks.erase(it);
            }
            gdk_property_delete(p_window, gdk_atom_intern_static_string("WM_TRANSIENT_FOR"));
        }
    }
}


/*void MCScreenDC::createbackdrop(MCStringRef color)
void MCScreenDC::createbackdrop(MCStringRef color)
{
	if (m_backdrop_pixmap == DNULL &&
            parsecolor(color, backdropcolor))
        alloccolor(backdropcolor);
	else
		backdropcolor.pixel = 0;

    GdkWindowAttr t_wa;
    t_wa.x = t_wa.y = 0;
    t_wa.width = device_getwidth();
    t_wa.height = device_getheight();
    t_wa.wclass = GDK_INPUT_OUTPUT;
    t_wa.visual = vis;
    t_wa.window_type = GDK_WINDOW_TOPLEVEL;
    t_wa.override_redirect = TRUE;
    t_wa.event_mask = GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK|GDK_ENTER_NOTIFY_MASK
        |GDK_LEAVE_NOTIFY_MASK|GDK_POINTER_MOTION_MASK|GDK_EXPOSURE_MASK;

    backdrop = gdk_window_new(NULL, &t_wa, GDK_WA_X|GDK_WA_Y|GDK_WA_VISUAL);

	if (m_backdrop_pixmap != DNULL)
	{
		gdk_window_set_back_pixmap(backdrop, m_backdrop_pixmap, FALSE);
	}
	else
	{
		GdkColor t_colour;
        t_colour.red = backdropcolor.red;
        t_colour.green = backdropcolor.green;
        t_colour.blue = backdropcolor.blue;
        gdk_rgb_find_color(gdk_drawable_get_colormap(backdrop), &t_colour);
        gdk_window_set_background(backdrop, &t_colour);
	}

    gdk_window_show_unraised(backdrop);
}*/


void MCScreenDC::destroybackdrop()
{
	if (backdrop != DNULL)
	{
		gdk_window_hide(backdrop);
        gdk_window_destroy(backdrop);
		backdrop = DNULL;
        s_primary_backdrop = nullptr;
	}
    for (GdkWindow *t_extra : s_extra_backdrops)
    {
        if (t_extra) { gdk_window_hide(t_extra); gdk_window_destroy(t_extra); }
    }
    s_extra_backdrops.clear();

	freepixmap(m_backdrop_pixmap);
}


Bool MCScreenDC::is_composite_wm ( int screen_id )
{
    return gdk_display_supports_composite(dpy);
}

////////////////////////////////////////////////////////////////////////////////

// Linux implementation of system appearance detection.
// MCPlatformGetSystemProperty is macOS-only; on Linux we detect dark mode
// via the GTK theme name — if it contains "dark" (case-insensitive) we
// report a dark appearance, otherwise light.
void MCScreenDC::getsystemappearance(MCSystemAppearance &r_appearance)
{
	r_appearance = kMCSystemAppearanceLight;

	GtkSettings *t_settings = gtk_settings_get_default();
	if (t_settings == NULL)
		return;

	gchar *t_theme_name = NULL;
	g_object_get(t_settings, "gtk-theme-name", &t_theme_name, NULL);
	if (t_theme_name != NULL)
	{
		gchar *t_lower = g_ascii_strdown(t_theme_name, -1);
		if (t_lower != NULL)
		{
			if (g_strstr_len(t_lower, -1, "dark") != NULL)
				r_appearance = kMCSystemAppearanceDark;
			g_free(t_lower);
		}
		g_free(t_theme_name);
	}
}

////////////////////////////////////////////////////////////////////////////////
