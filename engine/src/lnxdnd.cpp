#include "lnxprefix.h"

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

    // Pointer device for window-under-cursor queries during motion
    GdkDevice *t_pointer = gdk_seat_get_pointer(gdk_display_get_default_seat(dpy));

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

                    // Use gdk_device_get_window_at_position instead of
                    // gdk_drag_find_window_for_screen: the latter creates a
                    // GdkWindowCache via XCompositeGetOverlayWindow which
                    // can disturb Mutter's compositor state and cause a GPU hang.
                    gint t_win_x = 0, t_win_y = 0;
                    GdkWindow *t_dest_window = gdk_device_get_window_at_position(t_pointer, &t_win_x, &t_win_y);
                    GdkDragProtocol t_protocol = (t_dest_window != NULL) ? GDK_DRAG_PROTO_XDND : GDK_DRAG_PROTO_NONE;

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
                }

                break;
            }

            case GDK_BUTTON_RELEASE:
            {
                // Drop the item that was being dragged.
                if (t_action != DRAG_ACTION_NONE)
                {
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
                // Which action did the destination request?
                GdkDragAction t_gdk_action;
                t_gdk_action = gdk_drag_context_get_selected_action(t_context);

                if (t_gdk_action == GDK_ACTION_LINK)
                    t_action = DRAG_ACTION_LINK;
                if (t_gdk_action == GDK_ACTION_MOVE)
                    t_action = DRAG_ACTION_MOVE;
                if (t_gdk_action == GDK_ACTION_COPY)
                    t_action = DRAG_ACTION_COPY;

                MCLinuxDragAndDropSetCursorForAction(w, t_action, p_image);

                break;
            }

            case GDK_DROP_START:
                // Release the pointer grab before processing the drop so that
                // the drop target (and Mutter) can receive input normally.
                G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                gdk_display_pointer_ungrab(dpy, t_event->dnd.time);
                G_GNUC_END_IGNORE_DEPRECATIONS
                DnDClientEvent(t_event);
                // Don't set t_dnd_done here; wait for GDK_DROP_FINISHED which
                // arrives after the destination sends XdndFinished.
                break;

            case GDK_DROP_FINISHED:
            {
                // Did the drop succeed?
                bool t_success;
                t_success = gdk_drag_drop_succeeded(t_context);

                if (!t_success)
                    t_action = DRAG_ACTION_NONE;

                t_dnd_done = true;
                break;
            }

            case GDK_GRAB_BROKEN:
            {
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
