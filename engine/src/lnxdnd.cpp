#include "lnxprefix.h"

#include <stdio.h>

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

    // The drag-and-drop loop
    bool t_dnd_done = false;
    while (!t_dnd_done)
    {
        if (t_modal_loop.broken)
            break;

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
                // Throttle gdk_drag_motion() to ≤30 Hz to prevent overloading
                // the compositor (XWayland + Mutter + Intel i915) with
                // XConfigureWindow calls that trigger GPU compositing repaints.
                // 33 ms ≈ 30 fps.
                static guint32 s_last_motion_ms = 0;
                bool t_skip_position = (t_event->motion.time - s_last_motion_ms < 33);

                if (!t_skip_position)
                {
                    s_last_motion_ms = t_event->motion.time;

                    // Use X11 XQueryPointer + XdndAware tree walk to find the
                    // drop target. gdk_device_get_window_at_position only finds
                    // GDK-registered (our own) windows — it returns NULL for
                    // windows belonging to other applications, so XdndPosition
                    // would never reach Text Editor, Firefox, etc.
                    // gdk_drag_find_window_for_screen is avoided because it uses
                    // XCompositeGetOverlayWindow (via GdkWindowCache), which
                    // disturbs Mutter's compositor state and causes GPU hangs.
                    // All GDK-X11 functions and Xlib types are in x11:: namespace
                    // (lnxprefix.h wraps <gdk/gdkx.h> in "namespace x11 {}").
                    x11::Display *t_xdisplay = x11::gdk_x11_display_get_xdisplay(dpy);
                    x11::Window   t_xroot    = x11::gdk_x11_window_get_xid(gdk_get_default_root_window());
                    x11::Window   t_xtarget  = MCLinuxFindXdndTarget(t_xdisplay, t_xroot);

                    // Wrap the X11 target as a GdkWindow for gdk_drag_motion.
                    // If the window is already registered in GDK (one of ours),
                    // use the existing object. Otherwise create a foreign wrapper
                    // that we own and must unref after the call.
                    GdkWindow *t_dest_window = NULL;
                    GdkDragProtocol t_protocol = GDK_DRAG_PROTO_NONE;
                    bool t_dest_foreign = false;

                    if (t_xtarget != None)
                    {
                        GdkWindow *t_known = x11::gdk_x11_window_lookup_for_display(dpy, t_xtarget);
                        if (t_known != NULL)
                        {
                            t_dest_window = t_known;   // intra-app, already ref'd
                        }
                        else
                        {
                            t_dest_window = x11::gdk_x11_window_foreign_new_for_display(dpy, t_xtarget);
                            t_dest_foreign = true;     // cross-app, we own this ref
                        }
                        t_protocol = GDK_DRAG_PROTO_XDND;
                    }

                    fprintf(stderr, "DND motion: dest_xid=%lu foreign=%d proto=%d root=(%.0f,%.0f)\n",
                            (unsigned long)(x11::Window)t_xtarget, (int)t_dest_foreign, (int)t_protocol,
                            t_event->motion.x_root, t_event->motion.y_root);
                    fflush(stderr);

                    if (t_dest_window == NULL)
                    {
                        t_action = DRAG_ACTION_NONE;
                        MCLinuxDragAndDropSetCursorForAction(w, DRAG_ACTION_NONE, p_image);
                    }

                    gdk_drag_motion(t_context, t_dest_window, t_protocol,
                                    t_event->motion.x_root, t_event->motion.y_root,
                                    GdkDragAction(t_suggested_action),
                                    GdkDragAction(t_possible_actions),
                                    t_event->motion.time);

                    if (t_dest_foreign && t_dest_window != NULL)
                        g_object_unref(t_dest_window);
                }

                break;
            }

            case GDK_BUTTON_RELEASE:
            {
                fprintf(stderr, "DND button-release: t_action=%d (0=none,1=copy,2=move,4=link)\n",
                        (int)t_action);
                fflush(stderr);
                // Drop the item that was being dragged.
                if (t_action != DRAG_ACTION_NONE)
                {
                    fprintf(stderr, "DND button-release: calling gdk_drag_drop\n");
                    fflush(stderr);
                    // Claim XdndSelection NOW — after the button-release event
                    // has been enqueued in the X server. Mutter will process
                    // ButtonRelease before XFixesSelectionNotify (X11 ordering
                    // guarantee), so when its XFixes handler fires, the button
                    // is no longer pressed → no exclusive DnD grab is installed.
                    gdk_selection_owner_set_for_display(dpy, w, t_selection,
                                                        t_event->button.time, TRUE);
                    gdk_drag_drop(t_context, t_event->button.time);
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
                            fprintf(stderr, "  [%u] %s\n", (unsigned)i, t_aname ? t_aname : "(unknown)");
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

                    fprintf(stderr, "DND selection-request: data %s\n",
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
