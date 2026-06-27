#include "lnxprefix.h"

#include <stdio.h>
#include <vector>

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "dispatch.h"
#include "image.h"
#include "globals.h"
#include "resolution.h"
#include "redraw.h"
#include "util.h"

#include "lnxdc.h"


static bool g_dnd_init = false;
static GdkCursor *g_dnd_cursor_drag_init = NULL;
static GdkCursor *g_dnd_cursor_drop_copy = NULL;
static GdkCursor *g_dnd_cursor_drop_move = NULL;
static GdkCursor *g_dnd_cursor_drop_link = NULL;
static GdkCursor *g_dnd_cursor_drop_fail = NULL;


// Do all the setup of the xDnD protocol
static void MCLinuxDragAndDropInitialize(GdkDisplay* p_display)
{
	if (!g_dnd_init)
	{
        // Create cursors for indicating drop acceptability
        g_dnd_cursor_drag_init = gdk_cursor_new_from_name(p_display, "grabbing");
        g_dnd_cursor_drop_copy = gdk_cursor_new_from_name(p_display, "copy");
        g_dnd_cursor_drop_move = gdk_cursor_new_from_name(p_display, "move");
        g_dnd_cursor_drop_link = gdk_cursor_new_from_name(p_display, "link");
        g_dnd_cursor_drop_fail = gdk_cursor_new_from_name(p_display, "no-drop");

        // Initialisation done
        g_dnd_init = true;
	}
}

// Nothing ever calls this but somebody might, one day...
void MCLinuxDragAndDropFinalize()
{
    // GTK3: gdk_cursor_unref removed, use g_object_unref
    if (g_dnd_cursor_drag_init)
        g_object_unref(g_dnd_cursor_drag_init);
    if (g_dnd_cursor_drop_copy)
        g_object_unref(g_dnd_cursor_drop_copy);
    if (g_dnd_cursor_drop_move)
        g_object_unref(g_dnd_cursor_drop_move);
    if (g_dnd_cursor_drop_link)
        g_object_unref(g_dnd_cursor_drop_link);
    if (g_dnd_cursor_drop_fail)
        g_object_unref(g_dnd_cursor_drop_fail);
    g_dnd_init = false;
}

void MCLinuxDragAndDropSetCursorDragStart(GdkWindow *w, MCImage *p_image)
{
    // Images are not yet supported
    gdk_window_set_cursor(w, g_dnd_cursor_drag_init);
}

void MCLinuxDragAndDropSetCursorForAction(GdkWindow *w, MCDragAction p_action, MCImage *p_image)
{
    // Images are not supported at the moment (though GDK does provide some very
    // basic support for doing so via the find_window_for_screen function)
    if (p_action == DRAG_ACTION_COPY)
    {
        gdk_window_set_cursor(w, g_dnd_cursor_drop_copy);
    }
    else if (true || p_action == DRAG_ACTION_MOVE)
    {
        gdk_window_set_cursor(w, g_dnd_cursor_drop_move);
    }
    else if (p_action == DRAG_ACTION_LINK)
    {
        gdk_window_set_cursor(w, g_dnd_cursor_drop_link);
    }
    else
    {
        gdk_window_set_cursor(w, g_dnd_cursor_drop_fail);
    }
}


// Find the XdndAware X11 window under the pointer without using
// GdkWindowCache / XCompositeGetOverlayWindow (which causes GPU hangs).
//
// Strategy:
//   1. XQueryPointer to walk the X11 window tree from root to the deepest
//      child under the pointer.
//   2. Walk UP from that child looking for a window with the XdndAware
//      property set — that is the correct XdndDrop target.
//
// All X11 types and functions are in the x11:: namespace because lnxprefix.h
// wraps <gdk/gdkx.h> (and thereby all of Xlib) in "namespace x11 {}".
// X11 constant macros (None, False, AnyPropertyType, Success) are #defines
// and remain globally visible.
//
// Returns None if no XdndAware window is found (e.g. cursor over desktop).
static x11::Window MCLinuxFindXdndTarget(x11::Display *p_display,
                                         x11::Window   p_root)
{
    static x11::Atom s_xdnd_aware = None;
    if (s_xdnd_aware == None)
        s_xdnd_aware = x11::XInternAtom(p_display, "XdndAware", False);

    x11::Window t_child = p_root;

    // Step 1: descend to the deepest child under the pointer
    for (;;)
    {
        x11::Window t_root_ret, t_next = None;
        int t_rx, t_ry, t_wx, t_wy;
        unsigned int t_mask;
        if (!x11::XQueryPointer(p_display, t_child,
                                &t_root_ret, &t_next,
                                &t_rx, &t_ry, &t_wx, &t_wy, &t_mask)
                || t_next == None)
            break;
        t_child = t_next;
    }

    // Step 2: walk up looking for XdndAware
    x11::Window t_w = t_child;
    while (t_w != None && t_w != p_root)
    {
        x11::Atom t_type;
        int t_fmt;
        unsigned long t_items, t_after;
        unsigned char *t_data = NULL;

        // AnyPropertyType (0) avoids needing XA_ATOM across the namespace boundary;
        // we only care whether the property exists, not its type value.
        int t_rc = x11::XGetWindowProperty(p_display, t_w, s_xdnd_aware,
                                           0, 1, False,
                                           AnyPropertyType,
                                           &t_type, &t_fmt,
                                           &t_items, &t_after, &t_data);
        if (t_rc == Success && t_data != NULL)
        {
            x11::XFree(t_data);
            return t_w;   // found it
        }
        if (t_data != NULL)
            x11::XFree(t_data);

        // Move to parent
        x11::Window t_parent, t_root2;
        x11::Window *t_children = NULL;
        unsigned int t_nch;
        x11::XQueryTree(p_display, t_w,
                        &t_root2, &t_parent,
                        &t_children, &t_nch);
        if (t_children != NULL)
            x11::XFree(t_children);
        t_w = t_parent;
    }

    return None;
}

// ── Manual Xdnd source protocol ───────────────────────────────────────────────
//
// GDK3's gdk_drag_motion() does not send XdndPosition to foreign (non-GDK)
// destination windows.  We implement the XDND v5 source protocol directly
// using XSendEvent for cross-app drops, and keep the GDK path only for
// intra-app (own-window) drops.
//
// All X11 types/functions are in x11:: (lnxprefix.h wraps gdkx.h in that ns).
// X11 constant macros (None, False, ClientMessage, NoEventMask, etc.) are
// #defines and are globally visible without a namespace prefix.

struct MCLinuxXdndAtoms
{
    x11::Atom xdnd_enter;
    x11::Atom xdnd_position;
    x11::Atom xdnd_leave;
    x11::Atom xdnd_drop;
    x11::Atom xdnd_status;
    x11::Atom xdnd_finished;
    x11::Atom xdnd_type_list;
    x11::Atom xdnd_action_copy;
    x11::Atom xdnd_action_move;
    x11::Atom xdnd_action_link;
};

static MCLinuxXdndAtoms s_xdnd;
static bool             s_xdnd_inited = false;

static void MCLinuxXdndInitAtoms(x11::Display *p_xdpy)
{
    if (s_xdnd_inited) return;
    s_xdnd.xdnd_enter       = x11::XInternAtom(p_xdpy, "XdndEnter",      False);
    s_xdnd.xdnd_position    = x11::XInternAtom(p_xdpy, "XdndPosition",   False);
    s_xdnd.xdnd_leave       = x11::XInternAtom(p_xdpy, "XdndLeave",      False);
    s_xdnd.xdnd_drop        = x11::XInternAtom(p_xdpy, "XdndDrop",       False);
    s_xdnd.xdnd_status      = x11::XInternAtom(p_xdpy, "XdndStatus",     False);
    s_xdnd.xdnd_finished    = x11::XInternAtom(p_xdpy, "XdndFinished",   False);
    s_xdnd.xdnd_type_list   = x11::XInternAtom(p_xdpy, "XdndTypeList",   False);
    s_xdnd.xdnd_action_copy = x11::XInternAtom(p_xdpy, "XdndActionCopy", False);
    s_xdnd.xdnd_action_move = x11::XInternAtom(p_xdpy, "XdndActionMove", False);
    s_xdnd.xdnd_action_link = x11::XInternAtom(p_xdpy, "XdndActionLink", False);
    s_xdnd_inited = true;
}

static void MCLinuxXdndSendMessage(x11::Display *p_xdpy, x11::Window p_dest,
                                   x11::Atom p_type,
                                   long d0, long d1, long d2, long d3, long d4)
{
    x11::XClientMessageEvent t_msg = {};
    t_msg.type         = ClientMessage;
    t_msg.display      = p_xdpy;
    t_msg.window       = p_dest;
    t_msg.message_type = p_type;
    t_msg.format       = 32;
    t_msg.data.l[0]    = d0;
    t_msg.data.l[1]    = d1;
    t_msg.data.l[2]    = d2;
    t_msg.data.l[3]    = d3;
    t_msg.data.l[4]    = d4;
    x11::XSendEvent(p_xdpy, p_dest, False, NoEventMask,
                    reinterpret_cast<x11::XEvent*>(&t_msg));
}

// Send XdndEnter.  When there are more than 3 types we set the
// "more-than-3-types" flag and write them into XdndTypeList on the source.
// p_types / p_count are gulong[] (same layout as x11::Atom[]).
static void MCLinuxXdndSendEnter(x11::Display     *p_xdpy,
                                  x11::Window       p_src,
                                  x11::Window       p_dest,
                                  const gulong     *p_types,
                                  uindex_t          p_count)
{
    // XdndTypeList property on source (needed if >3 types)
    x11::XChangeProperty(p_xdpy, p_src,
                         s_xdnd.xdnd_type_list,
                         (x11::Atom)4 /* XA_ATOM */, 32,
                         PropModeReplace,
                         reinterpret_cast<const unsigned char*>(p_types),
                         (int)p_count);

    long t_flags = (5L << 24) | (p_count > 3 ? 1L : 0L); // version=5, list flag
    long t_t0 = (p_count >= 1) ? (long)p_types[0] : 0L;
    long t_t1 = (p_count >= 2) ? (long)p_types[1] : 0L;
    long t_t2 = (p_count >= 3) ? (long)p_types[2] : 0L;

    MCLinuxXdndSendMessage(p_xdpy, p_dest, s_xdnd.xdnd_enter,
                           (long)p_src, t_flags, t_t0, t_t1, t_t2);
}

static void MCLinuxXdndSendPosition(x11::Display *p_xdpy,
                                     x11::Window   p_src,
                                     x11::Window   p_dest,
                                     int p_root_x, int p_root_y,
                                     unsigned long p_time,
                                     x11::Atom     p_action)
{
    long t_xy = ((long)(p_root_x & 0xffff) << 16) | (long)(p_root_y & 0xffff);
    MCLinuxXdndSendMessage(p_xdpy, p_dest, s_xdnd.xdnd_position,
                           (long)p_src, 0L, t_xy, (long)p_time, (long)p_action);
}

static void MCLinuxXdndSendLeave(x11::Display *p_xdpy,
                                  x11::Window   p_src,
                                  x11::Window   p_dest)
{
    MCLinuxXdndSendMessage(p_xdpy, p_dest, s_xdnd.xdnd_leave,
                           (long)p_src, 0L, 0L, 0L, 0L);
}

static void MCLinuxXdndSendDrop(x11::Display  *p_xdpy,
                                 x11::Window    p_src,
                                 x11::Window    p_dest,
                                 unsigned long  p_time)
{
    MCLinuxXdndSendMessage(p_xdpy, p_dest, s_xdnd.xdnd_drop,
                           (long)p_src, 0L, (long)p_time, 0L, 0L);
}

// ── Raw X11 event filter for Xdnd source responses ───────────────────────────
//
// GTK3 removed GdkEventClient, so XdndStatus / XdndFinished ClientMessages
// arriving on the source window are never delivered as GdkEvents.
// We install a GDK window filter that intercepts the raw XEvent before GDK
// can discard it, stores the result in a small struct, and removes the event
// from GDK's queue.

struct MCLinuxXdndFilterData
{
    const MCLinuxXdndAtoms *atoms;
    // XdndStatus response
    bool         got_status;
    bool         status_accept;
    x11::Atom    status_action;
    // XdndFinished response
    bool         got_finished;
};

static GdkFilterReturn MCLinuxXdndFilter(GdkXEvent *p_xevent,
                                          GdkEvent  */*p_event*/,
                                          gpointer   p_data)
{
    // Diagnostic: count every call to verify gdk_window_add_filter(NULL,...) works.
    // If we never see "DND filter #1" in the log, the display-level filter is not
    // being called at all (GTK3 build issue or filter not installed).
    {
        static unsigned long s_filter_count = 0;
        s_filter_count++;
        if (s_filter_count <= 5 || s_filter_count % 500 == 0)
        {
            x11::XEvent *t_dbg = static_cast<x11::XEvent*>(p_xevent);
            fprintf(stderr, "DND filter #%lu: X event type=%d\n",
                    s_filter_count, t_dbg->type);
            fflush(stderr);
        }
    }

    x11::XEvent *t_xev = static_cast<x11::XEvent*>(p_xevent);
    if (t_xev->type != ClientMessage)
        return GDK_FILTER_CONTINUE;

    x11::XClientMessageEvent *t_cm = &t_xev->xclient;
    MCLinuxXdndFilterData    *t_fd = static_cast<MCLinuxXdndFilterData*>(p_data);

    // Diagnostic: log all ClientMessages so we can confirm XdndStatus is arriving
    fprintf(stderr, "DND filter: ClientMessage msgtype=%lu window=%lu\n",
            (unsigned long)t_cm->message_type,
            (unsigned long)t_cm->window);
    fflush(stderr);

    if (t_cm->message_type == t_fd->atoms->xdnd_status)
    {
        bool t_want_more = (t_cm->data.l[1] & 2L) != 0;
        t_fd->status_accept = (t_cm->data.l[1] & 1L) != 0;
        t_fd->status_action = (x11::Atom)(unsigned long)t_cm->data.l[4];
        t_fd->got_status    = true;
        fprintf(stderr,
                "DND filter: XdndStatus caught! accept=%d want_more=%d action=%lu\n",
                (int)t_fd->status_accept, (int)t_want_more,
                (unsigned long)t_fd->status_action);
        fflush(stderr);
        return GDK_FILTER_REMOVE;   // consume; don't let GDK discard it
    }
    if (t_cm->message_type == t_fd->atoms->xdnd_finished)
    {
        bool t_accept = (t_cm->data.l[1] & 1L) != 0;
        fprintf(stderr,
                "DND filter: XdndFinished caught! window=%lu accept=%d\n",
                (unsigned long)t_cm->window, (int)t_accept);
        fflush(stderr);
        t_fd->got_finished = true;
        return GDK_FILTER_REMOVE;
    }

    return GDK_FILTER_CONTINUE;
}

struct dnd_modal_loop_context
{
    GdkDragContext* drag_context;
    GdkDisplay* display;
};

static void break_dnd_modal_loop(void* context)
{
    dnd_modal_loop_context* t_context = (dnd_modal_loop_context*)context;
    gdk_drag_abort(t_context->drag_context, GDK_CURRENT_TIME);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gdk_display_pointer_ungrab(t_context->display, GDK_CURRENT_TIME);
    G_GNUC_END_IGNORE_DEPRECATIONS
}

// SN-2014-07-11: [[ Bug 12769 ]] Update the signature - the non-implemented UIDC dodragdrop was called otherwise
MCDragAction MCScreenDC::dodragdrop(Window w, MCDragActionSet p_allowed_actions, MCImage *p_image, const MCPoint* p_image_offset)
{
    // Ensure that the DnD mechanisms are ready for use
    MCLinuxDragAndDropInitialize(dpy);

    // Preserve the modifier state
    uint16_t t_old_modstate = MCmodifierstate;

    // Translate the allowed actions into a set of GDK actions
    gint t_possible_actions = 0;
    gint t_suggested_action = 0;
    if (p_allowed_actions & DRAG_ACTION_COPY)
        t_possible_actions |= GDK_ACTION_COPY;
    if (p_allowed_actions & DRAG_ACTION_MOVE)
        t_possible_actions |= GDK_ACTION_MOVE;
    if (p_allowed_actions & DRAG_ACTION_LINK)
        t_possible_actions |= GDK_ACTION_LINK;

    // Which is the "best" action that we support?
    if (t_possible_actions & GDK_ACTION_LINK)
        t_suggested_action = GDK_ACTION_LINK;
    else if (t_possible_actions & GDK_ACTION_MOVE)
        t_suggested_action = GDK_ACTION_MOVE;
    else if (t_possible_actions & GDK_ACTION_COPY)
        t_suggested_action = GDK_ACTION_COPY;

    // Get the list of supported targets
    MCLinuxRawClipboard* t_dragboard = static_cast<MCLinuxRawClipboard*> (MCdragboard->GetRawClipboard());
    MCAutoDataRef t_targets(t_dragboard->CopyTargets());
    if (*t_targets == NULL)
        return DRAG_ACTION_NONE;

    // Turn it into a GList
    GList* t_target_list = NULL;
    for (uindex_t i = 0; i < MCDataGetLength(*t_targets)/sizeof(gulong); i++)
    {
        gulong t_atom = reinterpret_cast<const gulong*>(MCDataGetBytePtr(*t_targets))[i];
        t_target_list = g_list_append(t_target_list, gpointer(t_atom));
    }
    if (t_target_list == NULL)
        return DRAG_ACTION_NONE;

    // Create a drag-and-drop context for this operation.
    // gdk_drag_begin is deprecated since GTK 3.10 but still present in 3.24.
    // It internally calls gdk_drag_begin_for_device with the default pointer.
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    GdkDragContext *t_context = gdk_drag_begin(w, t_target_list);
    G_GNUC_END_IGNORE_DEPRECATIONS
    g_list_free(t_target_list);

    // Diagnostic: check whether gdk_drag_begin claimed XdndSelection internally.
    // GTK3's X11 DnD source (gdk_x11_drag_context_drag_begin_for_device) is
    // supposed to call _gdk_x11_display_set_selection_owner for XdndSelection.
    // If it does, we already own it and XdndEnter to foreign windows should be
    // accepted by conforming receivers without any extra claim.
    {
        x11::Display *t_xdpy_check = x11::gdk_x11_display_get_xdisplay(dpy);
        x11::Window  t_src_check   = x11::gdk_x11_window_get_xid(w);
        x11::Atom t_xdnd_sel_xatom = x11::XInternAtom(t_xdpy_check, "XdndSelection", False);
        x11::Window t_xdnd_sel_owner = x11::XGetSelectionOwner(t_xdpy_check, t_xdnd_sel_xatom);
        fprintf(stderr, "DND: after gdk_drag_begin — XdndSelection owner=%lu, our xid=%lu (%s)\n",
                (unsigned long)t_xdnd_sel_owner,
                (unsigned long)t_src_check,
                t_xdnd_sel_owner == t_src_check ? "WE OWN IT" : "not owned by us");
        fflush(stderr);
    }

    // Take ownership of the mouse so that nothing interferes with the drag.
    // gdk_pointer_grab is deprecated since GTK 3.0 but still present in 3.24;
    // it wraps gdk_seat_grab internally.
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gdk_pointer_grab(w, FALSE,
                     GdkEventMask(GDK_POINTER_MOTION_MASK|GDK_BUTTON_PRESS_MASK|GDK_BUTTON_RELEASE_MASK),
                     NULL, NULL, MCeventtime);
    G_GNUC_END_IGNORE_DEPRECATIONS

    // We need to know what action was selected so we know whether to delete
    // the data afterwards (as done for move actions)
    MCDragAction t_action = DRAG_ACTION_NONE;

    // Context for breaking out of the modal loop, if required
    dnd_modal_loop_context t_loop_context;
    modal_loop t_modal_loop;
    t_loop_context.drag_context = t_context;
    t_loop_context.display = dpy;
    t_modal_loop.break_function = break_dnd_modal_loop;
    t_modal_loop.context = &t_loop_context;
    modalLoopStart(t_modal_loop);

    // Set the cursor
    MCLinuxDragAndDropSetCursorDragStart(w, p_image);

    // Prepare the drag-and-drop selection atom but do NOT claim ownership yet.
    //
    // Root cause of the system-wide input freeze on XWayland + Mutter 48:
    //   Mutter watches XdndSelection via XFixes (XFixesSelectSelectionInput).
    //   When XdndSelection is claimed AND a button is pressed over an XWayland
    //   surface, meta_xwayland_dnd_handle_xfixes_selection_notify calls
    //   meta_wayland_data_device_start_drag, which installs an exclusive Clutter
    //   event handler (xdnd_event_interface). That handler swallows all button-
    //   press events (CLUTTER_EVENT_STOP) and, if it is never torn down, freezes
    //   click input system-wide.
    //
    // Fix: delay claiming XdndSelection until GDK_BUTTON_RELEASE (just before
    // gdk_drag_drop). X11 event ordering guarantees that Mutter's XFixesSelection-
    // Notify handler runs AFTER Mutter has already processed the ButtonRelease.
    // At that point n_buttons_pressed == 0 in Clutter, find_dnd_candidate_device
    // returns FALSE, and meta_wayland_data_device_start_drag is never called.
    t_dragboard->SetClipboardWindow(w);
    GdkAtom t_selection = t_dragboard->GetSelectionAtom();

    // X11 state for the cross-app Xdnd protocol
    x11::Display *t_xdisplay  = x11::gdk_x11_display_get_xdisplay(dpy);
    x11::Window   t_src_xid   = x11::gdk_x11_window_get_xid(w);
    x11::Window   t_xdnd_foreign_dest = None;  // foreign XID we last entered
    GdkWindow    *t_foreign_gdk = NULL;         // GdkWindow wrapper for foreign dest
    bool          t_xdnd_foreign_entered = false; // true after first gdk_drag_motion to current foreign dest
    // When the cursor leaves a foreign window on the same event as (or just before)
    // button-release, GDK would normally send XdndLeave before we can drop.  We
    // defer that one gdk_drag_motion call and keep the last foreign dest here so
    // the button-release handler can still attempt XdndDrop.
    x11::Window   t_deferred_drop_dest = None; // foreign XID saved when leaving, cleared on next motion
    // Set to true once XdndDrop has been sent cross-app; suppresses subsequent
    // motion and button-release events that arrive before XdndFinished.
    bool          t_sent_foreign_drop  = false;
    gint64        t_foreign_drop_time  = 0;    // g_get_monotonic_time() at drop send
    // Last position successfully sent to the foreign dest as raw XdndPosition.
    // Used to send a reminder XdndPosition before XdndDrop in the deferred case.
    int           t_last_foreign_x     = 0;
    int           t_last_foreign_y     = 0;
    unsigned long t_last_foreign_time  = 0;
    // Re-enter tracking: after a delay inside a foreign window with action=0,
    // we send XdndLeave then a fresh XdndEnter+XdndPosition once to break
    // GDK's rect suppression and let Mutter respond with the real action.
    // Time-based: Mutter's first XdndStatus(action=0, want_more=0) fires
    // immediately (before the async Wayland round-trip to the native target
    // completes). We wait 200ms for the round-trip to finish, then re-enter
    // once so Mutter can answer with the actual action.
    gint64        t_foreign_enter_time   = 0;  // g_get_monotonic_time() on first entry
    bool          t_foreign_reenter_done  = false; // only re-enter once per window
    MCLinuxXdndInitAtoms(t_xdisplay);

    // Resolve the IPC/relay window that owns XdndSelection — this is the
    // source XID that Mutter (and all XDND targets) will identify as the drag
    // source when they read XdndTypeList and XdndActionList.
    // Must be done before setting properties so we target the right window.
    x11::Atom   t_xdnd_sel_atom = x11::XInternAtom(t_xdisplay, "XdndSelection", False);
    x11::Window t_xdnd_ipc_xid  = x11::XGetSelectionOwner(t_xdisplay, t_xdnd_sel_atom);
    if (t_xdnd_ipc_xid == None)
        t_xdnd_ipc_xid = t_src_xid;   // fallback: no IPC window, use visible
    fprintf(stderr, "DND: IPC/relay xid=%lu (XdndSelection owner, used as Xdnd src)\n",
            (unsigned long)t_xdnd_ipc_xid);
    fflush(stderr);

    // Set XdndTypeList and XdndActionList on the IPC window.
    // Per XDND protocol, the target reads these properties from whichever
    // window owns XdndSelection (the IPC window, not the visible window).
    // Setting them only on the visible window means the target reads an empty
    // property and rejects the drag regardless of the action we suggest.
    //
    // Additionally, Mutter's X11→Wayland bridge passes X11 atom names verbatim
    // as Wayland MIME type strings.  Wayland-native apps (e.g. GNOME Text
    // Editor) register "text/plain;charset=utf-8" as their DnD target — not
    // "UTF8_STRING".  We augment the type list with the standard text/plain
    // aliases so Mutter can offer them to the Wayland app.  SelectionRequests
    // for these aliases are satisfied by falling back to UTF8_STRING data.
    {
        const gulong *t_base_atoms =
            reinterpret_cast<const gulong*>(MCDataGetBytePtr(*t_targets));
        uindex_t t_base_count = MCDataGetLength(*t_targets) / sizeof(gulong);

        // Dump every atom name to confirm what the dragboard contains.
        fprintf(stderr, "DND: dragboard types (%zu):\n", (size_t)t_base_count);
        for (uindex_t i = 0; i < t_base_count; i++)
        {
            gchar *t_n = gdk_atom_name((GdkAtom)(guintptr)t_base_atoms[i]);
            fprintf(stderr, "DND:  [%zu] atom=%lu  %s\n",
                    (size_t)i, (unsigned long)t_base_atoms[i], t_n ? t_n : "?");
            g_free(t_n);
        }
        fflush(stderr);

        // Probe for any text-related type so we know whether to add text/plain aliases.
        static const char *s_text_probes[] = {
            "UTF8_STRING", "UTF-8",
            "text/plain;charset=utf-8", "text/plain;charset=UTF-8",
            "text/plain", "STRING",
        };
        bool t_has_utf8 = false;
        for (auto probe : s_text_probes)
        {
            x11::Atom t_probe_atom = x11::XInternAtom(t_xdisplay, probe, True /* only_if_exists */);
            if (t_probe_atom == None) continue;
            for (uindex_t i = 0; i < t_base_count; i++)
            {
                if ((x11::Atom)t_base_atoms[i] == t_probe_atom)
                {
                    t_has_utf8 = true;
                    fprintf(stderr, "DND: found text probe '%s' at [%zu]\n", probe, (size_t)i);
                    fflush(stderr);
                    break;
                }
            }
            if (t_has_utf8) break;
        }

        // Build augmented type list: base types + text/plain aliases (if we
        // have UTF8 data and the alias isn't already in the list).
        static const char *s_plain_aliases[] = {
            "text/plain;charset=utf-8",
            "text/plain;charset=UTF-8",
            "text/plain",
        };
        const int k_alias_count = (int)(sizeof(s_plain_aliases)/sizeof(s_plain_aliases[0]));

        // Reserve space for worst case: base + all aliases
        std::vector<x11::Atom> t_aug_atoms(t_base_atoms, t_base_atoms + t_base_count);
        if (t_has_utf8)
        {
            for (int ai = 0; ai < k_alias_count; ai++)
            {
                x11::Atom t_a = x11::XInternAtom(t_xdisplay, s_plain_aliases[ai], False);
                // Only add if not already advertised
                bool t_dup = false;
                for (uindex_t i = 0; i < t_base_count; i++)
                    if ((x11::Atom)t_base_atoms[i] == t_a) { t_dup = true; break; }
                if (!t_dup)
                    t_aug_atoms.push_back(t_a);
            }
        }

        x11::Atom t_action_list_atom =
            x11::XInternAtom(t_xdisplay, "XdndActionList", False);
        x11::Atom t_action_atoms[3];
        int t_action_count = 0;
        if (t_possible_actions & GDK_ACTION_COPY)
            t_action_atoms[t_action_count++] = s_xdnd.xdnd_action_copy;
        if (t_possible_actions & GDK_ACTION_MOVE)
            t_action_atoms[t_action_count++] = s_xdnd.xdnd_action_move;
        if (t_possible_actions & GDK_ACTION_LINK)
            t_action_atoms[t_action_count++] = s_xdnd.xdnd_action_link;

        // Primary: augmented list on IPC window (what the target actually reads)
        x11::XChangeProperty(t_xdisplay, t_xdnd_ipc_xid,
                             s_xdnd.xdnd_type_list,
                             (x11::Atom)4 /* XA_ATOM */, 32,
                             PropModeReplace,
                             reinterpret_cast<const unsigned char*>(t_aug_atoms.data()),
                             (int)t_aug_atoms.size());
        x11::XChangeProperty(t_xdisplay, t_xdnd_ipc_xid,
                             t_action_list_atom,
                             (x11::Atom)4 /* XA_ATOM */, 32,
                             PropModeReplace,
                             reinterpret_cast<const unsigned char*>(t_action_atoms),
                             t_action_count);

        // Also set on visible window (base list only) for intra-app drops
        x11::XChangeProperty(t_xdisplay, t_src_xid,
                             s_xdnd.xdnd_type_list,
                             (x11::Atom)4 /* XA_ATOM */, 32,
                             PropModeReplace,
                             reinterpret_cast<const unsigned char*>(t_base_atoms),
                             (int)t_base_count);
        x11::XChangeProperty(t_xdisplay, t_src_xid,
                             t_action_list_atom,
                             (x11::Atom)4 /* XA_ATOM */, 32,
                             PropModeReplace,
                             reinterpret_cast<const unsigned char*>(t_action_atoms),
                             t_action_count);

        x11::XFlush(t_xdisplay);
        fprintf(stderr,
                "DND: XdndTypeList on ipc=%lu (%zu types, has_utf8=%d) visible=%lu (%zu types)\n",
                (unsigned long)t_xdnd_ipc_xid, t_aug_atoms.size(), (int)t_has_utf8,
                (unsigned long)t_src_xid, (size_t)t_base_count);
        fflush(stderr);
    }

    // Install a display-level GDK filter (window=NULL) to capture XdndFinished
    // ClientMessages.  GTK3 removed GdkEventClient so these never appear as
    // GdkEvents.  NULL ensures our filter runs before per-window filters.
    MCLinuxXdndFilterData t_xdnd_filter = {};
    t_xdnd_filter.atoms = &s_xdnd;
    gdk_window_add_filter(NULL, MCLinuxXdndFilter, &t_xdnd_filter);

    // Pre-compute the preferred Xdnd action atom
    x11::Atom t_xdnd_action = s_xdnd.xdnd_action_copy;
    if (t_suggested_action == GDK_ACTION_MOVE)
        t_xdnd_action = s_xdnd.xdnd_action_move;
    else if (t_suggested_action == GDK_ACTION_LINK)
        t_xdnd_action = s_xdnd.xdnd_action_link;

    // The drag-and-drop loop
    bool t_dnd_done = false;
    while (!t_dnd_done)
    {
        if (t_modal_loop.broken)
            break;

        // Poll the X event queue for ALL ClientMessages (any window).
        // XCheckTypedEvent catches XdndStatus / XdndFinished regardless of
        // which window Mutter addressed them to. Non-Xdnd ClientMessages
        // are put back via XPutBackEvent so GDK can still see them.
        // Only drain when we have an active foreign destination to avoid
        // consuming unrelated ClientMessages during intra-app drags.
        // After XdndDrop is sent, Mutter must reply with XdndFinished.
        // Bail out if it hasn't arrived after 2 seconds (prevents infinite wait).
        if (t_sent_foreign_drop &&
            (g_get_monotonic_time() - t_foreign_drop_time) > 2000000LL)
        {
            fprintf(stderr,
                    "DND: XdndFinished timeout after 2s — ending modal loop\n");
            fflush(stderr);
            t_dnd_done = true;
        }

        if (t_xdnd_foreign_dest != None || t_sent_foreign_drop)
        {
            x11::XEvent t_xpoll;
            while (x11::XCheckTypedEvent(t_xdisplay, ClientMessage, &t_xpoll))
            {
                x11::XClientMessageEvent *t_cm = &t_xpoll.xclient;
                fprintf(stderr,
                        "DND Xpoll: ClientMessage msgtype=%lu window=%lu"
                        " (xdnd_status=%lu xdnd_finished=%lu)\n",
                        (unsigned long)t_cm->message_type,
                        (unsigned long)t_cm->window,
                        (unsigned long)s_xdnd.xdnd_status,
                        (unsigned long)s_xdnd.xdnd_finished);
                fflush(stderr);
                if (t_cm->message_type == s_xdnd.xdnd_status)
                {
                    bool t_want_more = (t_cm->data.l[1] & 2L) != 0;
                    t_xdnd_filter.status_accept =
                        (t_cm->data.l[1] & 1L) != 0;
                    t_xdnd_filter.status_action =
                        (x11::Atom)(unsigned long)t_cm->data.l[4];
                    t_xdnd_filter.got_status = true;
                    fprintf(stderr,
                            "DND Xpoll: XdndStatus accept=%d want_more=%d action=%lu\n",
                            (int)t_xdnd_filter.status_accept,
                            (int)t_want_more,
                            (unsigned long)t_xdnd_filter.status_action);
                    fflush(stderr);
                }
                else if (t_cm->message_type == s_xdnd.xdnd_finished)
                {
                    bool t_accept = (t_cm->data.l[1] & 1L) != 0;
                    fprintf(stderr,
                            "DND Xpoll: XdndFinished accept=%d\n",
                            (int)t_accept);
                    fflush(stderr);
                    t_xdnd_filter.got_finished = true;
                }
                else
                {
                    // Not an Xdnd message — identify atom for diagnostics then
                    // put back for GDK to handle.
                    char *t_atom_name =
                        x11::XGetAtomName(t_xdisplay, t_cm->message_type);
                    fprintf(stderr,
                            "DND Xpoll: unknown ClientMessage atom=%lu name='%s'\n",
                            (unsigned long)t_cm->message_type,
                            t_atom_name ? t_atom_name : "(null)");
                    fflush(stderr);
                    if (t_atom_name)
                        x11::XFree(t_atom_name);
                    x11::XPutBackEvent(t_xdisplay, &t_xpoll);
                    break;  // stop draining to avoid spin on this event
                }
            }
        }

        // Check for Xdnd responses captured by the raw-X11 window filter.
        // The filter runs inside g_main_context_iteration / EnqueueGdkEvents
        // and sets these flags; we poll them here each loop iteration.
        if (t_xdnd_filter.got_status)
        {
            t_xdnd_filter.got_status = false;
            if (t_xdnd_filter.status_accept)
            {
                if (t_xdnd_filter.status_action == s_xdnd.xdnd_action_move)
                    t_action = DRAG_ACTION_MOVE;
                else if (t_xdnd_filter.status_action == s_xdnd.xdnd_action_link)
                    t_action = DRAG_ACTION_LINK;
                else
                    t_action = DRAG_ACTION_COPY;
            }
            else
            {
                t_action = DRAG_ACTION_NONE;
            }
            MCLinuxDragAndDropSetCursorForAction(w, t_action, p_image);
            fprintf(stderr,
                    "DND XdndStatus: accept=%d action=%lu → t_action=%d\n",
                    (int)t_xdnd_filter.status_accept,
                    (unsigned long)t_xdnd_filter.status_action,
                    (int)t_action);
            fflush(stderr);
        }
        if (t_xdnd_filter.got_finished)
        {
            fprintf(stderr, "DND XdndFinished received\n");
            fflush(stderr);
            t_xdnd_filter.got_finished = false;
            t_dnd_done = true;
        }

        // Run the GLib event loop to exhaustion
        EnqueueGdkEvents();

        GdkEvent *t_event;
        if (pendingevents != NULL)
        {
            // Get the next event from the queue
            t_event = gdk_event_copy(pendingevents->event);
            MCEventnode *tptr = (MCEventnode *)pendingevents->remove(pendingevents);
            delete tptr;
        }
        else
        {
            // In theory, all events should have already been queued as pending
            // through the GLib main loop. However, that only applies to those
            // that the server has already sent - this function call prompts the
            // server to send any events queued on its end.
            t_event = gdk_event_get();
        }

        // If there is still no event, actively wait for one
        if (t_event == NULL)
        {
            g_main_context_iteration(NULL, TRUE);
            continue;
        }

        switch (t_event->type)
        {
            case GDK_KEY_PRESS:
            case GDK_KEY_RELEASE:
            {
                // Update the modifier state with the asynchronous state
                MCmodifierstate = MCscreen->querymods();
                break;
            }

            case GDK_MOTION_NOTIFY:
            {
                // After XdndDrop has been sent, ignore all further motion events;
                // we're waiting for XdndFinished and any motion is just noise.
                if (t_sent_foreign_drop)
                    break;

                // When the cursor leaves a foreign window we save the dest as
                // t_deferred_drop_dest (set below) so button-release can still
                // drop there if no further motion arrives.  We skip the
                // gdk_drag_motion call for that one "leave" event to avoid
                // sending XdndLeave prematurely (which would cancel Mutter's
                // Wayland accept state).  Cleared to false at the top so it
                // only suppresses exactly one gdk_drag_motion per leave.
                bool t_skip_gdk_motion = false;

                // Detect the XdndAware target via X11 tree-walk (bypasses GDK's
                // window cache which only knows our own GDK windows).
                x11::Window t_xroot   = x11::gdk_x11_window_get_xid(
                                            gdk_get_default_root_window());
                x11::Window t_xtarget = MCLinuxFindXdndTarget(t_xdisplay, t_xroot);

                bool t_is_foreign = false;
                GdkWindow *t_dest_gdk = NULL;

                if (t_xtarget != None)
                {
                    GdkWindow *t_known =
                        x11::gdk_x11_window_lookup_for_display(dpy, t_xtarget);
                    // Previous calls to gdk_x11_window_foreign_new_for_display
                    // register the XID as GDK_WINDOW_FOREIGN.  Distinguish our
                    // real windows (TOPLEVEL/CHILD) from foreign ones.
                    if (t_known != NULL &&
                        gdk_window_get_window_type(t_known) != GDK_WINDOW_FOREIGN)
                        t_dest_gdk = t_known;
                    else
                        t_is_foreign = true;
                }

                // ── Foreign-window wrapper management (unthrottled) ──
                //
                // We use gdk_drag_motion for BOTH intra-app and foreign targets.
                // For foreign windows, GDK sends XdndEnter (when dest changes) and
                // XdndPosition using the drag context's IPC window as source — the
                // same window that owns XdndSelection. GDK also installs its own
                // filter on the IPC window for XdndStatus / XdndFinished, routing
                // them back as GDK_DRAG_STATUS / GDK_DROP_FINISHED events.
                //
                // Manual XSendEvent sends to Mutter's DnD tracking window produce
                // zero response; Mutter's bridge apparently requires that the drag
                // context and window wrappers be registered via GDK's own internals.
                if (t_is_foreign)
                {
                    if (t_xdnd_foreign_dest != t_xtarget)
                    {
                        // Leaving the old foreign window — GDK sends XdndLeave
                        // implicitly when gdk_drag_motion is called with a new dest
                        if (t_foreign_gdk != NULL)
                        {
                            g_object_unref(t_foreign_gdk);
                            t_foreign_gdk = NULL;
                        }
                        // Create a GdkWindow wrapper for the new foreign dest.
                        // This registers the XID as GDK_WINDOW_FOREIGN in GDK's
                        // internal table; our foreign-detection check (type !=
                        // GDK_WINDOW_FOREIGN) ensures subsequent lookups still
                        // classify it as foreign, not intra-app.
                        t_foreign_gdk =
                            x11::gdk_x11_window_foreign_new_for_display(dpy, t_xtarget);
                        t_xdnd_foreign_dest = t_xtarget;
                        t_xdnd_foreign_entered = false;  // first motion will use gdk_drag_motion
                        t_foreign_enter_time   = 0;      // set on first gdk_drag_motion
                        t_foreign_reenter_done = false;

                        // Send raw XdndEnter from the IPC/selection window.
                        // GDK3 sends XdndEnter with context->source_window (the
                        // visible widget window, t_src_xid) as data.l[0].  Mutter's
                        // meta_xwayland_dnd_handle_client_message checks
                        //   (Window) event->data.l[0] == dnd->owner
                        // where dnd->owner = XGetSelectionOwner(XdndSelection) =
                        // t_xdnd_ipc_xid.  The mismatch causes Mutter to silently
                        // discard GDK's XdndEnter, XdndPosition, and XdndDrop, so
                        // text never reaches the Wayland-native target.
                        //
                        // Fix: bypass GDK and send from t_xdnd_ipc_xid so Mutter
                        // accepts the message.  XdndTypeList is already written on
                        // t_xdnd_ipc_xid during setup, so we set the type-list flag
                        // (bit 0) and let the target read the property directly.
                        if (t_xdnd_ipc_xid != None)
                        {
                            MCLinuxXdndSendMessage(
                                t_xdisplay, t_xtarget,
                                s_xdnd.xdnd_enter,
                                (long)t_xdnd_ipc_xid,
                                (5L << 24) | 1L,  // XDND v5, type-list flag
                                0L, 0L, 0L);
                            x11::XFlush(t_xdisplay);
                            fprintf(stderr,
                                    "DND: sent raw XdndEnter from ipc=%lu to foreign=%lu\n",
                                    (unsigned long)t_xdnd_ipc_xid,
                                    (unsigned long)t_xtarget);
                            fflush(stderr);
                        }

                        fprintf(stderr,
                                "DND: entered foreign xid=%lu wrapper=%s\n",
                                (unsigned long)t_xtarget,
                                t_foreign_gdk ? "ok" : "FAILED");
                        fflush(stderr);
                    }
                    // Use the foreign GdkWindow wrapper as the gdk_drag_motion dest
                    if (t_foreign_gdk != NULL)
                        t_dest_gdk = t_foreign_gdk;
                }
                else if (t_xdnd_foreign_dest != None)
                {
                    // Pointer left the foreign window; release wrapper.
                    // Save dest as t_deferred_drop_dest: if button-release arrives
                    // before the NEXT motion event (common when the cursor crosses
                    // the boundary while releasing), we can still send XdndDrop
                    // without the intervening XdndLeave that gdk_drag_motion would
                    // send.  t_deferred_drop_dest is cleared on the next motion.
                    if (t_xdnd_foreign_entered)
                    {
                        t_deferred_drop_dest = t_xdnd_foreign_dest;
                        t_skip_gdk_motion    = true;  // don't send XdndLeave this event
                    }
                    fprintf(stderr, "DND: left foreign xid=%lu (deferred=%lu skip=%d)\n",
                            (unsigned long)t_xdnd_foreign_dest,
                            (unsigned long)t_deferred_drop_dest,
                            (int)t_skip_gdk_motion);
                    fflush(stderr);
                    if (t_foreign_gdk != NULL)
                    {
                        g_object_unref(t_foreign_gdk);
                        t_foreign_gdk = NULL;
                    }
                    t_xdnd_foreign_dest = None;
                    t_xdnd_foreign_entered = false;
                    t_action = DRAG_ACTION_NONE;
                }

                // ── Position updates (throttled to ≤30 Hz / 33 ms) ──
                static guint32 s_last_motion_ms = 0;
                bool t_skip_position =
                    (t_event->motion.time - s_last_motion_ms < 33);

                if (!t_skip_position)
                {
                    s_last_motion_ms = t_event->motion.time;

                    fprintf(stderr,
                            "DND motion: dest_xid=%lu foreign=%d root=(%.0f,%.0f)\n",
                            (unsigned long)t_xtarget, (int)t_is_foreign,
                            t_event->motion.x_root, t_event->motion.y_root);
                    fflush(stderr);

                    if (t_dest_gdk == NULL)
                    {
                        t_action = DRAG_ACTION_NONE;
                        MCLinuxDragAndDropSetCursorForAction(w, DRAG_ACTION_NONE,
                                                             p_image);
                    }
                    // When over a foreign window, show COPY cursor immediately without
                    // waiting for Mutter's XdndStatus.  Mutter's async X11→Wayland
                    // handshake means the first XdndStatus always has action=0; the
                    // real action is only confirmed at drop time.  Sending repeated
                    // XdndLeave+XdndEnter (the previous re-enter approach) resets the
                    // Wayland handshake on every mouse-move, so it never completes.
                    // Instead we send ONE XdndEnter+XdndPosition (on first entry),
                    // let the handshake finish undisturbed, and attempt the drop on
                    // button-release regardless of t_action (see GDK_BUTTON_RELEASE).
                    else if (t_is_foreign && t_dest_gdk != NULL)
                    {
                        MCLinuxDragAndDropSetCursorForAction(w, DRAG_ACTION_COPY, p_image);
                    }

                    if (t_skip_gdk_motion)
                    {
                        // "Left foreign" event: defer XdndLeave until next motion
                        // so button-release can still use t_deferred_drop_dest.
                        fprintf(stderr,
                                "DND: skipping gdk_drag_motion (XdndLeave deferred)\n");
                        fflush(stderr);
                    }
                    else
                    {
                        // For foreign targets, prefer COPY (most apps accept it for
                        // text drops). Fall back to MOVE, then the caller's
                        // t_suggested_action.  Keep the full t_possible_actions mask
                        // so modifier-key overrides can work later.
                        GdkDragAction t_eff_suggested;
                        if (t_is_foreign)
                        {
                            if (t_possible_actions & GDK_ACTION_COPY)
                                t_eff_suggested = GDK_ACTION_COPY;
                            else if (t_possible_actions & GDK_ACTION_MOVE)
                                t_eff_suggested = GDK_ACTION_MOVE;
                            else
                                t_eff_suggested = GdkDragAction(t_suggested_action);
                        }
                        else
                        {
                            t_eff_suggested = GdkDragAction(t_suggested_action);
                        }

                        fprintf(stderr,
                                "DND gdk_drag_motion: foreign=%d entered=%d possible=0x%x"
                                " eff_suggested=0x%x dest=%s\n",
                                (int)t_is_foreign, (int)t_xdnd_foreign_entered,
                                (unsigned)t_possible_actions,
                                (unsigned)t_eff_suggested, t_dest_gdk ? "ok" : "NULL");
                        fflush(stderr);
                        // Re-enter approach (DISABLED):
                        // Previously we sent XdndLeave + XdndEnter after 200ms to
                        // break "rect suppression" when Mutter's first XdndStatus
                        // had action=0.  This was the wrong diagnosis: want_more is
                        // always 1 from Mutter (no rect suppression), and the real
                        // problem was that GDK3 sends all XdndEnter/Position/Drop
                        // with t_src_xid (widget window) as data.l[0], but Mutter
                        // checks data.l[0] == dnd->owner (= t_xdnd_ipc_xid, the
                        // XdndSelection owner).  Mutter silently discarded every GDK
                        // message.  The re-enter loop sent XdndLeave+XdndEnter also
                        // via GDK, so they used the same wrong source and were equally
                        // ignored, while confusing our enter-tracking state.
                        // Fixed properly by sending raw XdndEnter/Position/Drop from
                        // t_xdnd_ipc_xid — see raw sends above and below.
#if 0
                        if (t_is_foreign &&
                            t_xdnd_foreign_entered &&
                            !t_foreign_reenter_done &&
                            t_action == DRAG_ACTION_NONE)
                        {
                            gint64 t_now = g_get_monotonic_time();
                            if (t_foreign_enter_time == 0)
                                t_foreign_enter_time = t_now;
                            if ((t_now - t_foreign_enter_time) >= 200000LL)
                            {
                                t_foreign_reenter_done = true;
                                t_xdnd_foreign_entered = false;
                                t_deferred_drop_dest   = None;
                                gdk_drag_motion(t_context, NULL, GDK_DRAG_PROTO_NONE,
                                                t_event->motion.x_root,
                                                t_event->motion.y_root,
                                                GdkDragAction(t_suggested_action),
                                                GdkDragAction(t_possible_actions),
                                                t_event->motion.time);
                            }
                        }
#endif

                        // This gdk_drag_motion call will send XdndLeave to any
                        // previous dest (including the deferred foreign dest).
                        // Invalidate the deferred dest now so button-release
                        // doesn't try to drop there after the leave is sent.
                        if (t_deferred_drop_dest != None && !t_is_foreign)
                            t_deferred_drop_dest = None;

                        gdk_drag_motion(t_context, t_dest_gdk,
                                        t_dest_gdk ? GDK_DRAG_PROTO_XDND
                                                   : GDK_DRAG_PROTO_NONE,
                                        t_event->motion.x_root,
                                        t_event->motion.y_root,
                                        t_eff_suggested,
                                        GdkDragAction(t_possible_actions),
                                        t_event->motion.time);

                        if (t_is_foreign && t_dest_gdk != NULL)
                        {
                            if (!t_xdnd_foreign_entered && t_foreign_enter_time == 0)
                                t_foreign_enter_time = g_get_monotonic_time();
                            t_xdnd_foreign_entered = true;
                            t_last_foreign_x    = t_event->motion.x_root;
                            t_last_foreign_y    = t_event->motion.y_root;
                            t_last_foreign_time = t_event->motion.time;

                            // Send raw XdndPosition from the IPC/selection window.
                            // GDK3 sends XdndPosition with t_src_xid (widget window)
                            // as data.l[0], which Mutter rejects (see XdndEnter
                            // comment above for full explanation).  We bypass GDK's
                            // send and use t_xdnd_ipc_xid instead.  GDK's call above
                            // (gdk_drag_motion) still runs to keep GDK's internal
                            // context state consistent; its XdndPosition is ignored by
                            // Mutter but Mutter's XdndStatus (in reply to ours) is
                            // received by GDK's IPC-window filter and delivered as
                            // GDK_DRAG_STATUS, updating t_action normally.
                            if (t_xdnd_ipc_xid != None)
                            {
                                MCLinuxXdndSendPosition(
                                    t_xdisplay, t_xdnd_ipc_xid,
                                    t_xdnd_foreign_dest,
                                    t_event->motion.x_root,
                                    t_event->motion.y_root,
                                    t_event->motion.time,
                                    s_xdnd.xdnd_action_copy);
                                x11::XFlush(t_xdisplay);
                            }
                        }
                    }  // end else (!t_skip_gdk_motion)
                }

                break;
            }

            case GDK_BUTTON_RELEASE:
            {
                // After XdndDrop is sent cross-app, a phantom button-release can
                // arrive (buffered in the X queue or synthesised after pointer
                // ungrab). Ignore it — the modal loop exits via XdndFinished or
                // the 2-second timeout, not a second button-release.
                if (t_sent_foreign_drop)
                    break;

                fprintf(stderr, "DND button-release: t_action=%d (0=none,1=copy,2=move,4=link)\n",
                        (int)t_action);
                fflush(stderr);

                // For cross-app drops, attempt XdndDrop even when t_action==0.
                // Mutter's first XdndStatus always has action=0 because the
                // X11→Wayland handshake with the Wayland-native target is async;
                // by the time the user releases the button the handshake has had
                // time to complete, so Mutter can accept the actual drop.
                //
                // t_deferred_drop_dest handles the case where the cursor crossed
                // the foreign-window boundary on the same event as (or just before)
                // the button-release: we saved the last foreign XID without sending
                // XdndLeave, so Mutter's Wayland state is still valid for the drop.
                x11::Window t_foreign_drop_xid =
                    (t_xdnd_foreign_dest != None) ? t_xdnd_foreign_dest
                                                  : t_deferred_drop_dest;
                bool t_attempt_foreign_drop =
                    (t_foreign_drop_xid != None &&
                     (t_xdnd_foreign_entered || t_deferred_drop_dest != None));

                if (t_action != DRAG_ACTION_NONE || t_attempt_foreign_drop)
                {
                    // Claim XdndSelection NOW — after the button-release event.
                    // Mutter processes ButtonRelease before XFixesSelectionNotify
                    // (X11 ordering guarantee), so no exclusive DnD grab fires.
                    if (t_foreign_drop_xid != None)
                    {
                        // ── Cross-app drop ──
                        // We send raw XdndDrop from t_xdnd_ipc_xid (see below), then
                        // also call gdk_drag_drop() for GDK state housekeeping.
                        //
                        // Root cause: GDK3 sends all Xdnd messages (Enter/Position/
                        // Drop) with context->source_window = t_src_xid (the visible
                        // widget window) as data.l[0].  Mutter's bridge checks
                        //   (Window) event->data.l[0] == dnd->owner
                        // where dnd->owner = XGetSelectionOwner(XdndSelection) =
                        // t_xdnd_ipc_xid.  Mismatch → Mutter silently discards every
                        // GDK message → text never reaches the Wayland-native target.
                        //
                        // gdk_drag_drop() is still called to mark the context as
                        // drop-initiated; GDK's IPC-window filter only translates
                        // XdndFinished → GDK_DROP_FINISHED when that flag is set.
                        // GDK's own XdndDrop (wrong source) is ignored by Mutter.
                        fprintf(stderr,
                                "DND button-release: gdk_drag_drop (foreign) → xid=%lu t_action=%d deferred=%d\n",
                                (unsigned long)t_foreign_drop_xid,
                                (int)t_action,
                                (int)(t_deferred_drop_dest != None));
                        fflush(stderr);

                        // Pre-drop XdndPosition (deferred case): if the cursor left
                        // the foreign window on the same event as button-release we
                        // saved the dest as t_deferred_drop_dest and skipped
                        // XdndLeave.  Mutter's Wayland state is still valid, but
                        // send a reminder XdndPosition from t_xdnd_ipc_xid to give
                        // Mutter a fresh position before the drop.
                        if (t_deferred_drop_dest != None && t_last_foreign_x != 0
                            && t_xdnd_ipc_xid != None)
                        {
                            MCLinuxXdndSendPosition(
                                t_xdisplay, t_xdnd_ipc_xid,
                                t_foreign_drop_xid,
                                t_last_foreign_x, t_last_foreign_y,
                                t_last_foreign_time,
                                s_xdnd.xdnd_action_copy);
                            x11::XFlush(t_xdisplay);
                            fprintf(stderr,
                                    "DND button-release: sent pre-drop XdndPosition from ipc=%lu at (%d,%d)\n",
                                    (unsigned long)t_xdnd_ipc_xid,
                                    t_last_foreign_x, t_last_foreign_y);
                            fflush(stderr);
                        }

                        // Send raw XdndDrop from the IPC/selection window.
                        // GDK3's gdk_drag_drop() sends XdndDrop with t_src_xid
                        // (widget window) as data.l[0], which Mutter rejects because
                        // data.l[0] != dnd->owner (= t_xdnd_ipc_xid).  Mutter never
                        // processes the drop so text never reaches the target.
                        //
                        // Fix: send raw XdndDrop from t_xdnd_ipc_xid first, then
                        // call gdk_drag_drop() to put GDK's context into drop-
                        // initiated state.  GDK's drop is ignored by Mutter (wrong
                        // source) but the state change is needed: when Mutter replies
                        // with XdndFinished → t_xdnd_ipc_xid, GDK's IPC-window
                        // filter translates it to GDK_DROP_FINISHED only if the
                        // context was marked drop-initiated by gdk_drag_drop().
                        if (t_xdnd_ipc_xid != None)
                        {
                            MCLinuxXdndSendDrop(
                                t_xdisplay, t_xdnd_ipc_xid,
                                t_foreign_drop_xid,
                                t_event->button.time);
                            x11::XFlush(t_xdisplay);
                            fprintf(stderr,
                                    "DND button-release: sent raw XdndDrop from ipc=%lu to foreign=%lu\n",
                                    (unsigned long)t_xdnd_ipc_xid,
                                    (unsigned long)t_foreign_drop_xid);
                            fflush(stderr);
                        }

                        G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                        gdk_display_pointer_ungrab(dpy, t_event->button.time);
                        G_GNUC_END_IGNORE_DEPRECATIONS
                        gdk_drag_drop(t_context, t_event->button.time);
                        t_sent_foreign_drop = true;
                        t_foreign_drop_time = g_get_monotonic_time();
                        // Wait for GDK_DROP_FINISHED (GDK receives XdndFinished
                        // and sets t_dnd_done) or 2-second timeout.  Suppress
                        // phantom motion/button-release via t_sent_foreign_drop.
                    }
                    else if (t_action != DRAG_ACTION_NONE)
                    {
                        // ── Intra-app drop ──
                        // Claim XdndSelection now — safe because the button just
                        // released and Mutter sees n_buttons_pressed=0 when
                        // XFixesSelectionNotify fires (X11 event ordering guarantee).
                        gdk_selection_owner_set_for_display(dpy, w, t_selection,
                                                            t_event->button.time, TRUE);
                        fprintf(stderr, "DND button-release: gdk_drag_drop intra-app\n");
                        fflush(stderr);
                        gdk_drag_drop(t_context, t_event->button.time);
                    }
                    else
                        t_dnd_done = true;
                }
                else
                    t_dnd_done = true;

                break;
            }

            case GDK_SELECTION_REQUEST:
            {
                {
                    gchar *t_target_name = gdk_atom_name(t_event->selection.target);
                    fprintf(stderr, "DND selection-request: target=%s\n",
                            t_target_name ? t_target_name : "(null)");
                    fflush(stderr);
                    g_free(t_target_name);
                }

                // We are using the dragboard
                MCLinuxRawClipboard* t_clipboard = static_cast<MCLinuxRawClipboard*> (MCdragboard->GetRawClipboard());

                // GTK3: selection.requestor is already a GdkWindow* (was an XID in GTK2).
                // Do NOT g_object_unref it here — gdk_event_free() handles that.
                GdkWindow *t_requestor = t_event->selection.requestor;

                // There is a backwards-compatibility issue with the way the
                // ICCCM deals with selections: older clients can request a
                // selection but not supply a property name. In that case,
                // the property set should be equal to the target name.
                GdkAtom t_property;
                if (t_event->selection.property != GDK_NONE)
                    t_property = t_event->selection.property;
                else
                    t_property = t_event->selection.target;

                // What type should the selection be converted to?
                static GdkAtom s_targets = gdk_atom_intern_static_string("TARGETS");
                if (t_event->selection.target == s_targets)
                {
                    // Get the list of types we can convert to
                    MCAutoDataRef t_targets(t_clipboard->CopyTargets());

                    if (*t_targets != NULL)
                    {
                        uindex_t t_target_atom_count = MCDataGetLength(*t_targets)/sizeof(gulong);
                        const gulong *t_atom_ptr = (const gulong*)MCDataGetBytePtr(*t_targets);
                        fprintf(stderr, "DND TARGETS: offering %u formats:\n", (unsigned)t_target_atom_count);
                        for (uindex_t i = 0; i < t_target_atom_count; i++)
                        {
                            gchar *t_aname = gdk_atom_name((GdkAtom)t_atom_ptr[i]);
                            fprintf(stderr, "DND:  [%u] %s\n", (unsigned)i, t_aname ? t_aname : "(unknown)");
                            g_free(t_aname);
                        }
                        fflush(stderr);

                        gdk_property_change(t_requestor, t_property,
                                            GDK_SELECTION_TYPE_ATOM,
                                            32,
                                            GDK_PROP_MODE_REPLACE,
                                            (const guchar*)MCDataGetBytePtr(*t_targets),
                                            t_target_atom_count);

                        gdk_selection_send_notify(t_event->selection.requestor,
                                                  t_event->selection.selection,
                                                  t_event->selection.target,
                                                  t_property,
                                                  t_event->selection.time);
                    }
                    else
                    {
                        gdk_selection_send_notify(t_event->selection.requestor,
                                                  t_event->selection.selection,
                                                  t_event->selection.target,
                                                  GDK_NONE,
                                                  t_event->selection.time);
                    }
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

                    // Fallback: text/plain* aliases → serve UTF8_STRING data.
                    // Mutter's X11→Wayland bridge passes atom names verbatim as
                    // MIME types, so Wayland apps request text/plain;charset=utf-8
                    // instead of UTF8_STRING.  Serve the same bytes either way.
                    if (*t_data == NULL && t_item != NULL && *t_atom_string != NULL)
                    {
                        static const char *s_plain_aliases[] = {
                            "text/plain;charset=utf-8",
                            "text/plain;charset=UTF-8",
                            "text/plain",
                        };
                        bool t_is_plain_alias = false;
                        for (auto alias : s_plain_aliases)
                        {
                            if (MCStringIsEqualToCString(*t_atom_string, alias,
                                                          kMCStringOptionCompareCaseless))
                            {
                                t_is_plain_alias = true;
                                break;
                            }
                        }
                        if (t_is_plain_alias)
                        {
                            MCAutoStringRef t_utf8_str;
                            MCStringCreateWithCString("UTF8_STRING", &t_utf8_str);
                            const MCRawClipboardItemRep *t_utf8_rep =
                                t_item->FetchRepresentationByType(*t_utf8_str);
                            if (t_utf8_rep != NULL)
                                t_data.Give(t_utf8_rep->CopyData());
                        }
                    }

                    fprintf(stderr, "DND selection-request: target=%s data %s\n",
                            (*t_atom_string != NULL)
                                ? MCStringGetCString(*t_atom_string) : "?",
                            (*t_data != NULL) ? "FOUND — sending" : "NOT FOUND — sending GDK_NONE");
                    fflush(stderr);

                    if (*t_data != NULL)
                    {
                        gdk_property_change(t_requestor, t_property,
                                            t_event->selection.target,
                                            8,
                                            GDK_PROP_MODE_REPLACE,
                                            (const guchar*)MCDataGetBytePtr(*t_data),
                                            MCDataGetLength(*t_data));

                        gdk_selection_send_notify(t_event->selection.requestor,
                                                  t_event->selection.selection,
                                                  t_event->selection.target,
                                                  t_property,
                                                  t_event->selection.time);
                    }
                    else
                    {
                        gdk_selection_send_notify(t_event->selection.requestor,
                                                  t_event->selection.selection,
                                                  t_event->selection.target,
                                                  GDK_NONE,
                                                  t_event->selection.time);
                    }
                }

                break;
            }

            case GDK_DRAG_ENTER:
                DnDClientEvent(t_event);
                break;

            case GDK_DRAG_LEAVE:
                DnDClientEvent(t_event);
                break;

            case GDK_DRAG_MOTION:
                DnDClientEvent(t_event);
                break;

            case GDK_DRAG_STATUS:
            {
                fprintf(stderr, "DND drag-status received\n");
                fflush(stderr);
                // Which action did the destination request?
                GdkDragAction t_gdk_action;
                t_gdk_action = gdk_drag_context_get_selected_action(t_context);

                // Reset first: if destination returns action=0 (rejected), clear
                // t_action so that a button-release won't call gdk_drag_drop on
                // a target that has already refused us (which causes a ~5s wait
                // for an XdndFinished that will never arrive promptly).
                if (t_gdk_action == 0)
                    t_action = DRAG_ACTION_NONE;
                else if (t_gdk_action == GDK_ACTION_LINK)
                    t_action = DRAG_ACTION_LINK;
                else if (t_gdk_action == GDK_ACTION_MOVE)
                    t_action = DRAG_ACTION_MOVE;
                else if (t_gdk_action == GDK_ACTION_COPY)
                    t_action = DRAG_ACTION_COPY;

                fprintf(stderr, "DND drag-status: gdk_action=%d → t_action=%d\n",
                        (int)t_gdk_action, (int)t_action);
                fflush(stderr);

                // For foreign (Wayland-native) targets, Mutter always returns
                // action=0 initially because the X11→Wayland handshake is async.
                // Show COPY cursor optimistically so the user doesn't think the
                // drop is rejected and move the mouse away before releasing.
                if (t_xdnd_foreign_dest != None || t_deferred_drop_dest != None)
                    MCLinuxDragAndDropSetCursorForAction(w, DRAG_ACTION_COPY, p_image);
                else
                    MCLinuxDragAndDropSetCursorForAction(w, t_action, p_image);

                break;
            }

            case GDK_DROP_START:
            {
                fprintf(stderr, "DND drop-start received (intra-app drop)\n");
                fflush(stderr);
                // Release the pointer grab before processing the drop so that
                // the drop target (and Mutter) can receive input normally.
                G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                gdk_display_pointer_ungrab(dpy, t_event->dnd.time);
                G_GNUC_END_IGNORE_DEPRECATIONS
                DnDClientEvent(t_event);
                // Exit the modal loop immediately. GDK_DROP_FINISHED would be
                // the cleaner exit point (after the destination sends XdndFinished)
                // but in GTK3 without gdk_drag_context_manage_dnd, the source-
                // side event filter only handles GDK_DROP_FINISHED — it is not
                // reliably delivered for intra-app drops. Staying in the loop
                // blocks g_main_context_iteration forever, freezing the app.
                t_dnd_done = true;
                break;
            }

            case GDK_DROP_FINISHED:
            {
                // Received when the destination sends XdndFinished (cross-app
                // DnD or future GTK3 paths that do deliver this event).
                bool t_success;
                t_success = gdk_drag_drop_succeeded(t_context);
                fprintf(stderr, "DND drop-finished received: success=%d\n", (int)t_success);
                fflush(stderr);

                if (!t_success)
                    t_action = DRAG_ACTION_NONE;

                t_dnd_done = true;
                break;
            }

            case GDK_GRAB_BROKEN:
            {
                fprintf(stderr, "DND grab-broken received — drag cancelled\n");
                fflush(stderr);
                // Drag operation was a failure
                t_action = DRAG_ACTION_NONE;
                t_dnd_done = true;
                break;
            }

		default:
			/* Ignore this event */
			break;
        }

        gdk_event_free(t_event);

        // Unlock the screen, perform redraw and other cleanup tasks
        MCU_resetprops(True);
        MCRedrawUpdateScreen();
        siguser();
    }

    modalLoopEnd();

    // Remove the Xdnd display-level filter now that the drag is complete
    gdk_window_remove_filter(NULL, MCLinuxXdndFilter, &t_xdnd_filter);

    // Release the foreign GdkWindow wrapper if still held.
    // gdk_drag_abort (called by break_dnd_modal_loop) sends XdndLeave to the
    // GDK drag context's current dest_window, so we don't need to send it
    // manually here.
    if (t_foreign_gdk != NULL)
    {
        g_object_unref(t_foreign_gdk);
        t_foreign_gdk = NULL;
    }
    t_xdnd_foreign_dest = None;

    // Release the drag context and any remaining pointer grab
    g_object_unref(t_context);
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gdk_display_pointer_ungrab(dpy, GDK_CURRENT_TIME);
    G_GNUC_END_IGNORE_DEPRECATIONS
    t_dragboard->SetClipboardWindow(NULL);

    // Restore the cursor
    gdk_window_set_cursor(w, NULL);

    // Restore the original modifier key state
    MCmodifierstate = t_old_modstate;

    return t_action;
}
