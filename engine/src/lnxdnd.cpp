#include "lnxprefix.h"

#include <stdio.h>
#include <dlfcn.h>

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
}

// SN-2014-07-11: [[ Bug 12769 ]] Update the signature - the non-implemented UIDC dodragdrop was called otherwise
MCDragAction MCScreenDC::dodragdrop(Window w, MCDragActionSet p_allowed_actions, MCImage *p_image, const MCPoint* p_image_offset)
{
    //fprintf(stderr, "DND: dodragdrop\n");
    // Ensure that the DnD mechanisms are ready for use
    MCLinuxDragAndDropInitialize(dpy);

    // PRE-DnD diagnostic: confirm no foreign window is covering us before DnD starts.
    // If root_child differs from our XID here, the problem pre-dates this DnD.
    {
        x11::Display *t_xdpy_pre = x11::gdk_x11_display_get_xdisplay(dpy);
        x11::Window t_root_ret_pre = 0, t_child_ret_pre = 0;
        int t_rx_pre = 0, t_ry_pre = 0, t_wx_pre = 0, t_wy_pre = 0;
        unsigned int t_mask_pre = 0;
        x11::XQueryPointer(t_xdpy_pre, x11::XDefaultRootWindow(t_xdpy_pre),
                           &t_root_ret_pre, &t_child_ret_pre,
                           &t_rx_pre, &t_ry_pre, &t_wx_pre, &t_wy_pre, &t_mask_pre);
        fprintf(stderr, "DND diag PRE: XQueryPointer root_child=0x%lx xy=(%d,%d) mask=0x%x\n",
                (unsigned long)t_child_ret_pre, t_rx_pre, t_ry_pre, (unsigned)t_mask_pre);

        // Baseline focus: if focus is here before DnD, it must be restored here after.
        x11::Window t_pre_focus = 0; int t_pre_revert = 0;
        x11::XGetInputFocus(t_xdpy_pre, &t_pre_focus, &t_pre_revert);
        fprintf(stderr, "DND diag PRE: XGetInputFocus focus=0x%lx revert=%d\n",
                (unsigned long)t_pre_focus, t_pre_revert);
        fflush(stderr);
    }

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
    
    // GTK3: gdk_drag_begin() deprecated since 3.10 — it has no device
    // association, so GDK cannot deliver GDK_DROP_FINISHED back to the
    // source. Without that, the modal loop below never exits, the seat
    // grab is never released, and the whole desktop locks up.
    // Use gdk_drag_begin_with_coordinates() (GTK 3.20+, same requirement
    // as gdk_seat_grab already used below).
    GdkDevice *t_pointer = gdk_seat_get_pointer(gdk_display_get_default_seat(dpy));
    GdkDragContext *t_context = gdk_drag_begin_for_device(w, t_pointer, t_target_list);
    g_list_free(t_target_list);
    // Note: gdk_drag_begin_for_device relies on the implicit X11 pointer grab
    // created by the initiating button-press event.
    //
    // GTK 3.24.49 source analysis (gdkdnd-x11.c, fully read):
    //   - _gdk_x11_window_drag_begin: no grab call of any kind.
    //   - gdk_x11_drag_context_drag_motion: no grab call (drag_context_grab is
    //     dead code — it checks ipc_window which is never set in our path).
    //   - drag_context_grab / gdk_seat_grab: NEVER called. grab_seat stays NULL.
    // The ONLY active grab is the X11 implicit pointer grab from the button press.
    // It is released automatically by the X server when the button is released,
    // but we also call XUngrabPointer explicitly in cleanup (belt-and-suspenders
    // for XWayland/Mutter which may defer the release until a server round-trip).

    // NOTE: gdk_drag_begin_for_device internally creates a 100×100 RGBA
    // GDK_WINDOW_TEMP drag-indicator window (create_drag_window in gdkdnd-x11.c).
    // It starts UNMAPPED. gdk_drag_motion() calls move_drag_window() on every
    // motion event, sending XConfigureWindow to XWayland at mouse rate (≥60 Hz).
    // Under XWayland + Mutter + Intel i915, each XConfigureWindow triggers a
    // Wayland surface-position update which may schedule a compositor repaint.
    // We throttle gdk_drag_motion() calls below to ≤30 Hz to avoid overloading
    // the i915 GPU command ring buffer.
    //
    // DO NOT call gdk_window_destroy() on the drag window here: gdk_window_destroy
    // calls g_object_unref() which drops the refcount to 0 and FREES the GdkWindow
    // object. context_x11->drag_window then becomes a dangling pointer, causing
    // use-after-free when move_drag_window() later accesses it (BadWindow + crash).
    
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
    
    // Take ownership of the drag-and-drop selection
    t_dragboard->SetClipboardWindow(w);
    GdkAtom t_selection = t_dragboard->GetSelectionAtom();
    gdk_selection_owner_set_for_display(dpy, w, t_selection, GDK_CURRENT_TIME, TRUE);
    
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
            fprintf(stderr, "DND modal: blocking in g_main_context_iteration\n");
            fflush(stderr);
            g_main_context_iteration(NULL, TRUE);
            fprintf(stderr, "DND modal: g_main_context_iteration returned\n");
            fflush(stderr);
            continue;
        }

        fprintf(stderr, "DND modal: event type=%d\n", (int)t_event->type);
        fflush(stderr);

        switch (t_event->type)
        {
            case GDK_KEY_PRESS:
            case GDK_KEY_RELEASE:
            {
                //fprintf(stderr, "DND: key event\n");
                // Update the modifier state with the asynchronous state
                MCmodifierstate = MCscreen->querymods();
                break;
            }
                
            case GDK_MOTION_NOTIFY:
            {
                // Throttle gdk_drag_motion() to ≤30 Hz.
                //
                // gdk_drag_motion() internally calls move_drag_window() on every
                // invocation, which sends XConfigureWindow to the X server. Under
                // XWayland + Mutter + Intel i915, each XConfigureWindow triggers a
                // Wayland surface-position update that schedules a compositor repaint.
                // At mouse-event rate (≥60 Hz, often 120 Hz) this generates more GPU
                // compositing work per second than the i915 command ring can absorb,
                // causing a kernel TDR timeout → hard system freeze.
                //
                // 33 ms ≈ 30 fps. We use event timestamps (ms) to throttle. When
                // a motion event is skipped we still track the latest position so that
                // the final XdndPosition message (at drop time) is accurate.
                static guint32 s_last_motion_ms = 0;
                bool t_skip_position = (t_event->motion.time - s_last_motion_ms < 33);

                GdkWindow *t_dest_window = nullptr;
                gint t_win_x = 0, t_win_y = 0;

                if (!t_skip_position)
                {
                    s_last_motion_ms = t_event->motion.time;

                    // Use gdk_device_get_window_at_position (not gdk_drag_find_window_for_screen).
                    // The latter creates a GdkWindowCache via XCompositeGetOverlayWindow which
                    // disturbs Mutter's compositor state → GPU hang on ThinkPad/Intel i915.
                    t_dest_window = gdk_device_get_window_at_position(t_pointer, &t_win_x, &t_win_y);
                    GdkDragProtocol t_protocol = (t_dest_window != NULL) ? GDK_DRAG_PROTO_XDND : GDK_DRAG_PROTO_NONE;

                    // Clear the action if we didn't find a target
                    if (t_dest_window == NULL)
                    {
                        t_action = DRAG_ACTION_NONE;
                        MCLinuxDragAndDropSetCursorForAction(w, DRAG_ACTION_NONE, p_image);
                    }

                    // Send a drag motion event (calls move_drag_window internally)
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
                fprintf(stderr, "DND modal: button release, t_action=%d\n", (int)t_action);
                fflush(stderr);
                if (t_action != DRAG_ACTION_NONE)
                {
                    gdk_drag_drop(t_context, t_event->button.time);
                    gdk_display_flush(dpy);
                    fprintf(stderr, "DND modal: gdk_drag_drop sent + flushed\n");
                    fflush(stderr);
                }
                else
                    t_dnd_done = true;

                break;
            }
                
            case GDK_SELECTION_REQUEST:
            {
                //fprintf(stderr, "DND: selection request\n");
                // We are using the dragboard
                MCLinuxRawClipboard* t_clipboard = static_cast<MCLinuxRawClipboard*> (MCdragboard->GetRawClipboard());
                
                // GTK3: selection.requestor is already a GdkWindow* (was GdkNativeWindow/XID in GTK2)
                GdkWindow *t_requestor = t_event->selection.requestor;
                
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
                if (t_event->selection.target == s_targets)
                {
                    // Get the list of types we can convert to
                    MCAutoDataRef t_targets(t_clipboard->CopyTargets());
                    
                    if (*t_targets != NULL)
                    {
                        // Set a property on the requestor containing the
                        // list of targets we can convert to.
                        uindex_t t_target_atom_count = MCDataGetLength(*t_targets)/sizeof(gulong);
                        gdk_property_change(t_requestor, t_property,
                                            GDK_SELECTION_TYPE_ATOM,
                                            32,
                                            GDK_PROP_MODE_REPLACE,
                                            (const guchar*)MCDataGetBytePtr(*t_targets),
                                            t_target_atom_count);
                        
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
                break;
            }
                
            case GDK_DRAG_ENTER:
                // This is a D&D client event
                DnDClientEvent(t_event);
                break;
                
            case GDK_DRAG_LEAVE:
                // This is a D&D client event
                DnDClientEvent(t_event);
                break;
                
            case GDK_DRAG_MOTION:
                // This is a D&D client event
                DnDClientEvent(t_event);
                break;
                
            case GDK_DRAG_STATUS:
            {
                //fprintf(stderr, "DND: drag status\n");
                // Which action did the destination request?
                GdkDragAction t_gdk_action;
                t_gdk_action = gdk_drag_context_get_selected_action(t_context);

                // Convert to the engine's drag actions
                if (t_gdk_action == GDK_ACTION_LINK)
                    t_action = DRAG_ACTION_LINK;
                if (t_gdk_action == GDK_ACTION_MOVE)
                    t_action = DRAG_ACTION_MOVE;
                if (t_gdk_action == GDK_ACTION_COPY)
                    t_action = DRAG_ACTION_COPY;
                
                // Update the cursor
                MCLinuxDragAndDropSetCursorForAction(w, t_action, p_image);
                
                break;
            }
                
            case GDK_DROP_START:
            {
                fprintf(stderr, "DND modal: GDK_DROP_START\n");
                fflush(stderr);

                // Process the drop: calls gdk_drop_finish → sends XdndFinished.
                // NOTE: do NOT call gdk_drag_abort here — that sends XdndLeave
                // AFTER XdndDrop, which is a protocol violation and generates a
                // spurious GDK_DRAG_LEAVE that can confuse GDK/XWayland state.
                DnDClientEvent(t_event);

                // Explicitly release XdndSelection. GDK normally releases it
                // when it processes GDK_DROP_FINISHED on the source side, but
                // we exit the modal loop here on GDK_DROP_START and never
                // process GDK_DROP_FINISHED. XWayland watches XdndSelection
                // ownership to drive wl_data_source.dnd_finished; if we still
                // own it, XWayland never sends that event, Mutter never
                // releases its seat grab, and the entire desktop input freezes.
                {
                    x11::Display *t_xdpy = x11::gdk_x11_display_get_xdisplay(dpy);
                    x11::Atom t_xdnd_sel = x11::XInternAtom(t_xdpy, "XdndSelection", 0);
                    if (t_xdnd_sel)
                    {
                        x11::Window t_owner = x11::XGetSelectionOwner(t_xdpy, t_xdnd_sel);
                        fprintf(stderr, "DND modal: XdndSelection owner=0x%lx before release\n",
                                (unsigned long)t_owner);
                        fflush(stderr);
                        x11::XSetSelectionOwner(t_xdpy, t_xdnd_sel, 0L, t_event->dnd.time);
                        x11::XFlush(t_xdpy);
                        fprintf(stderr, "DND modal: XdndSelection released\n");
                        fflush(stderr);
                    }
                }

                gdk_display_flush(dpy);
                // XSync round-trip — blocks until XdndFinished is processed.
                gdk_display_sync(dpy);
                {
                    int t_drop_pump = 0;
                    while (g_main_context_iteration(g_main_context_default(), FALSE))
                        t_drop_pump++;
                    fprintf(stderr, "DND modal: GDK_DROP_START post-sync pump: %d events\n", t_drop_pump);
                    fflush(stderr);
                }
                t_dnd_done = true;
                fprintf(stderr, "DND modal: GDK_DROP_START done, t_dnd_done=true\n");
                fflush(stderr);
                break;
            }

            case GDK_DROP_FINISHED:
            {
                fprintf(stderr, "DND modal: GDK_DROP_FINISHED — grab released by GDK\n");
                fflush(stderr);
                // GDK has now processed XdndFinished and released its internal
                // X pointer grab. Safe to exit and unref the context.
                bool t_success;
                t_success = gdk_drag_drop_succeeded(t_context);

                // If we failed, there was no action
                if (!t_success)
                    t_action = DRAG_ACTION_NONE;

                // All done
                t_dnd_done = true;
                break;
            }
                
            case GDK_GRAB_BROKEN:
            {
                //fprintf(stderr, "DND: drop broken\n");
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
        fprintf(stderr, "DND modal: post-event: resetprops\n");
        fflush(stderr);
        // Unlock the screen, perform redraw and other cleanup tasks
        MCU_resetprops(True);
        fprintf(stderr, "DND modal: post-event: redraw\n");
        fflush(stderr);
        MCRedrawUpdateScreen();
        fprintf(stderr, "DND modal: post-event: siguser\n");
        fflush(stderr);
        siguser();
        fprintf(stderr, "DND modal: post-event: done\n");
        fflush(stderr);
    }

    fprintf(stderr, "DND modal: modalLoopEnd\n");
    fflush(stderr);
    modalLoopEnd();

    // Step 1: Signal GDK that the DnD has completed successfully.
    //
    // GTK 3.24.49 source analysis confirms that in our code path:
    //   - drag_context_grab / gdk_seat_grab: NEVER called (ipc_window is NULL
    //     because we don't call gdk_drag_context_manage_dnd).
    //   - gdk_drag_begin_for_device → _gdk_x11_window_drag_begin: NO grab.
    //   - gdk_drag_motion → gdk_x11_drag_context_drag_motion: NO grab.
    //   - gdk_drag_drop → gdk_x11_drag_context_drag_drop: NO ungrab either
    //     (drag_context_ungrab is a no-op because grab_seat is NULL).
    //
    // gdk_drag_drop_done():
    //   - Sets context->drop_done = TRUE (idempotent sentinel).
    //   - On the success path, calls gdk_window_hide(x11_context->drag_window)
    //     followed by move_drag_window(context, -100, -100). move_drag_window
    //     calls gdk_window_show — so the drag window is RE-MAPPED off-screen
    //     at (-100,-100) after gdk_drag_drop_done returns. This leaves it as
    //     a live Wayland surface with X11/Wayland focus, consuming all keyboard
    //     and pointer events until explicitly hidden.
    //   - After this call, gdk_drag_context_handle_source_event() will
    //     process GDK_DROP_FINISHED as a no-op (drop_done guard fires).

    // Grab the drag window reference BEFORE gdk_drag_drop_done so we can
    // explicitly hide it again after gdk_drag_drop_done re-maps it.
    GdkWindow *t_drag_win = gdk_drag_context_get_drag_window(t_context);
    fprintf(stderr, "DND cleanup: drag_win=%p XID=0x%lx visible=%d\n",
            (void*)t_drag_win,
            t_drag_win ? (unsigned long)x11::gdk_x11_window_get_xid(t_drag_win) : 0UL,
            t_drag_win ? (int)gdk_window_is_visible(t_drag_win) : -1);
    fflush(stderr);

    fprintf(stderr, "DND cleanup: gdk_drag_drop_done\n");
    fflush(stderr);
    gdk_drag_drop_done(t_context, TRUE);

    // gdk_drag_drop_done just re-mapped the drag window at (-100,-100).
    // Force-hide it so XWayland unmaps the Wayland surface and Mutter's
    // drag-session seat grab is properly released.
    if (t_drag_win)
    {
        gdk_window_hide(t_drag_win);
        fprintf(stderr, "DND cleanup: drag_win force-hidden, visible=%d\n",
                (int)gdk_window_is_visible(t_drag_win));
        fflush(stderr);
    }

    // Step 2: Unconditionally release any lingering pointer grabs.
    //
    // The only grab that can exist at this point is the implicit X11 pointer
    // grab created by the initiating button-press event, which the X server
    // normally releases automatically when all buttons are released.
    //
    // Under XWayland/Mutter the release of that implicit grab may be deferred
    // until the compositor processes the next X server round-trip.  We call
    // all three ungrab APIs as belt-and-suspenders to cover:
    //   (a) GDK seat-level grabs  (b) GDK XI2 device grabs
    //   (c) X11 core-protocol grabs (the most likely survivor).

    // 2a. GDK seat-level ungrab — covers gdk_seat_grab (not used in our path
    //     but harmless).
    gdk_seat_ungrab(gdk_device_get_seat(t_pointer));

    // 2b. GDK device-level ungrab — also clears GDK's internal grab-tracking
    //     tables (display->device_grabs / _gdk_display_set_has_pointer_grab)
    //     and calls XIUngrabDevice.
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gdk_device_ungrab(t_pointer, GDK_CURRENT_TIME);
    G_GNUC_END_IGNORE_DEPRECATIONS

    // 2c. Direct Xlib core-protocol ungrab — releases any active XGrabPointer
    //     or passive-grab-promoted active grab, including deferred XWayland
    //     releases that XIUngrabDevice misses. Also ungrab the keyboard: if
    //     GDK or XWayland grabbed the keyboard for the drag (e.g. via
    //     XGrabKeyboard or the XWayland keyboard-grab protocol), this forces
    //     it to release. XUngrabKeyboard is a no-op when no grab is active.
    {
        x11::Display *t_xdpy = x11::gdk_x11_display_get_xdisplay(dpy);
        x11::XUngrabPointer(t_xdpy, 0L /* CurrentTime */);
        x11::XUngrabKeyboard(t_xdpy, 0L /* CurrentTime */);

        // Synthetic button-1 release via XTEST. Under XWayland, XTEST events
        // are forwarded through XWayland's fake libinput device and reach Mutter
        // as real Wayland input events. If Mutter still holds a Wayland-level
        // implicit pointer grab (waiting for the physical button release it never
        // received because the DnD swallowed the XdndDrop / XdndFinished
        // sequence without a real button-up event reaching the compositor), this
        // synthetic release should clear it and unblock input routing.
        //
        // We load XTestFakeButtonEvent via dlopen/dlsym to avoid adding -lXtst
        // to the link flags (standalone.target.mk is GYP-generated and -ldl is
        // already present in LIBS).
        {
            typedef int (*XTestFakeButtonEvent_fn)(x11::Display*, unsigned int, int, unsigned long);
            static XTestFakeButtonEvent_fn s_fn = NULL;
            static bool s_init = false;
            if (!s_init)
            {
                void *t_lib = dlopen("libXtst.so.6", RTLD_LAZY | RTLD_NOLOAD);
                if (!t_lib)
                    t_lib = dlopen("libXtst.so", RTLD_LAZY);
                if (t_lib)
                    s_fn = (XTestFakeButtonEvent_fn)dlsym(t_lib, "XTestFakeButtonEvent");
                s_init = true;
            }
            if (s_fn)
            {
                s_fn(t_xdpy, 1 /* button 1 */, 0 /* False = release */, 0L /* CurrentTime */);
                fprintf(stderr, "DND cleanup: XTestFakeButtonEvent button-1 release sent\n");
            }
            else
                fprintf(stderr, "DND cleanup: XTestFakeButtonEvent not available (libXtst missing)\n");
            fflush(stderr);
        }

        x11::XFlush(t_xdpy);
    }

    fprintf(stderr, "DND cleanup: all ungrabs called\n");
    fflush(stderr);

    // Step 3: Blocking sync — wait for the X server to process the ungrab
    // requests and return any pending error replies before we continue.
    // The DnD protocol is already complete at this point, so gdk_display_sync
    // (XSync round-trip) is safe and cannot cause the GPU stall that was the
    // reason we switched to gdk_display_flush in the main wait() loop.
    gdk_display_sync(dpy);
    fprintf(stderr, "DND cleanup: gdk_display_sync done\n");
    fflush(stderr);

    // Step 3b: Pump the GLib/GDK event loop while the drag context is still
    // live. The GDK_DROP_START handler already did a sync+pump inside the
    // modal loop, but any events that arrived between then and now (e.g., a
    // second XdndFinished attempt, or Mutter's Wayland DnD teardown events)
    // still need processing. Doing this before g_object_unref(t_context)
    // keeps GDK's state machine coherent.
    {
        int t_preunref_pump = 0;
        while (g_main_context_iteration(g_main_context_default(), FALSE))
            t_preunref_pump++;
        fprintf(stderr, "DND cleanup: pre-unref pump: %d events\n", t_preunref_pump);
        fflush(stderr);
    }

    // Diagnostic: query actual X11 pointer grab state after all ungrabs.
    // child_ret == None means the pointer is on a background window (no app window
    // under it), which would explain why no events are delivered.
    // mask & Button1Mask (bit 8) set means button 1 is still physically held.
    {
        x11::Display *t_xdpy = x11::gdk_x11_display_get_xdisplay(dpy);
        x11::Window t_root_ret = 0, t_child_ret = 0;
        int t_rx = 0, t_ry = 0, t_wx = 0, t_wy = 0;
        unsigned int t_mask = 0;
        // XQueryPointer on root reports: child=topmost window under pointer,
        // mask=modifier+button state, root=root window actually containing pointer.
        // If an X11 grab is active, child reflects the grab tree, not actual position.
        x11::XQueryPointer(t_xdpy,
                           x11::XDefaultRootWindow(t_xdpy),
                           &t_root_ret, &t_child_ret,
                           &t_rx, &t_ry, &t_wx, &t_wy, &t_mask);
        fprintf(stderr, "DND diag: XQueryPointer root_child=0x%lx root_xy=(%d,%d) mask=0x%x (btn1=%d btn2=%d btn3=%d)\n",
                (unsigned long)t_child_ret, t_rx, t_ry, (unsigned)t_mask,
                !!(t_mask & (1u<<8)), !!(t_mask & (1u<<9)), !!(t_mask & (1u<<10)));
        fflush(stderr);

        // If root_child differs from our window, identify the foreign window.
        // class: 1=InputOutput (visible, has pixels), 2=InputOnly (invisible, events only).
        // map_state: 0=Unmapped, 1=Unviewable (parent unmapped), 2=IsViewable (visible).
        // override_redirect: 1=WM cannot manage it (popup/tooltip/DnD overlay).
        x11::Window t_our_xid = x11::gdk_x11_window_get_xid(w);
        if (t_child_ret != 0 && t_child_ret != t_our_xid)
        {
            x11::XWindowAttributes t_attrs = {};
            if (x11::XGetWindowAttributes(t_xdpy, t_child_ret, &t_attrs))
            {
                fprintf(stderr, "DND diag: foreign window 0x%lx: class=%d map_state=%d override_redirect=%d"
                        " geom=(%d,%d %dx%d) event_mask=0x%lx all_event_masks=0x%lx\n",
                        (unsigned long)t_child_ret,
                        (int)t_attrs.c_class,
                        (int)t_attrs.map_state,
                        (int)t_attrs.override_redirect,
                        t_attrs.x, t_attrs.y, t_attrs.width, t_attrs.height,
                        (unsigned long)t_attrs.your_event_mask,
                        (unsigned long)t_attrs.all_event_masks);
                fflush(stderr);
            }
            else
            {
                fprintf(stderr, "DND diag: foreign window 0x%lx: XGetWindowAttributes failed\n",
                        (unsigned long)t_child_ret);
                fflush(stderr);
            }

            // Find the parent of the foreign window.
            // If parent == root: true sibling (stacking issue).
            // If parent == our XID: it's a child of ours (unexpected).
            // If parent == some other window: reparented (WM frame hierarchy).
            {
                x11::Window t_qt_root = 0, t_qt_parent = 0;
                x11::Window *t_qt_children = NULL;
                unsigned int t_qt_nchildren = 0;
                if (x11::XQueryTree(t_xdpy, t_child_ret,
                                    &t_qt_root, &t_qt_parent,
                                    &t_qt_children, &t_qt_nchildren))
                {
                    fprintf(stderr, "DND diag: foreign window 0x%lx parent=0x%lx"
                            " (our_xid=0x%lx x11_root=0x%lx)\n",
                            (unsigned long)t_child_ret,
                            (unsigned long)t_qt_parent,
                            (unsigned long)t_our_xid,
                            (unsigned long)t_qt_root);
                    if (t_qt_children)
                        x11::XFree(t_qt_children);
                }
                fflush(stderr);
            }
        }

        // Check where keyboard focus is right now.
        // PointerRoot(1) = focus follows pointer.
        // None(0) = no focus window.
        // Any other XID = that window has focus.
        {
            x11::Window t_focus_win = 0;
            int t_focus_revert = 0;
            x11::XGetInputFocus(t_xdpy, &t_focus_win, &t_focus_revert);
            fprintf(stderr, "DND diag: XGetInputFocus focus=0x%lx revert=%d (our_xid=0x%lx)\n",
                    (unsigned long)t_focus_win, t_focus_revert,
                    (unsigned long)t_our_xid);
            fflush(stderr);

            // Identify the focus window when it differs from our main window.
            // class: 1=InputOutput, 2=InputOnly.
            // map_state: 0=Unmapped, 1=Unviewable, 2=IsViewable.
            // A tiny InputOnly window is typically GDK's focus_window child.
            if (t_focus_win != 0 && t_focus_win != t_our_xid)
            {
                x11::XWindowAttributes t_fa = {};
                if (x11::XGetWindowAttributes(t_xdpy, t_focus_win, &t_fa))
                {
                    fprintf(stderr,
                            "DND diag: focus window 0x%lx: class=%d map_state=%d"
                            " size=%dx%d pos=(%d,%d) override=%d\n",
                            (unsigned long)t_focus_win,
                            (int)t_fa.c_class,
                            (int)t_fa.map_state,
                            t_fa.width, t_fa.height,
                            t_fa.x, t_fa.y,
                            (int)t_fa.override_redirect);

                    // Is it a child of our window?
                    x11::Window t_fw_root = 0, t_fw_parent = 0;
                    x11::Window *t_fw_children = NULL;
                    unsigned int t_fw_nchildren = 0;
                    x11::XQueryTree(t_xdpy, t_focus_win,
                                    &t_fw_root, &t_fw_parent,
                                    &t_fw_children, &t_fw_nchildren);
                    if (t_fw_children) x11::XFree(t_fw_children);
                    fprintf(stderr,
                            "DND diag: focus window parent=0x%lx"
                            " (our_xid=0x%lx, %s)\n",
                            (unsigned long)t_fw_parent,
                            (unsigned long)t_our_xid,
                            t_fw_parent == t_our_xid ? "CHILD OF OURS"
                            : t_fw_parent == 0 ? "root child"
                            : "other parent");
                }
                else
                {
                    fprintf(stderr, "DND diag: focus window 0x%lx: XGetWindowAttributes failed\n",
                            (unsigned long)t_focus_win);
                }
                fflush(stderr);
            }
        }

        // Find the parent of OUR window to answer: sibling or frame?
        // If our parent == root (t_root_ret): true siblings, stacking issue.
        // If our parent == t_child_ret: we're inside the "foreign" frame window.
        {
            x11::Window t_op_root = 0, t_op_parent = 0;
            x11::Window *t_op_children = NULL;
            unsigned int t_op_nchildren = 0;
            if (x11::XQueryTree(t_xdpy, t_our_xid, &t_op_root, &t_op_parent,
                                &t_op_children, &t_op_nchildren))
            {
                fprintf(stderr, "DND diag: our window 0x%lx parent=0x%lx root=0x%lx\n",
                        (unsigned long)t_our_xid, (unsigned long)t_op_parent,
                        (unsigned long)t_op_root);
                if (t_op_children) x11::XFree(t_op_children);
            }
            fflush(stderr);
        }

        // WM_CLASS of the foreign window — identifies who created it.
        // "hyperxtalk\0HyperXTalk\0" → our own frame/shell window (WM reparented us)
        // "gnome-shell\0..."         → compositor overlay
        // <not set>                  → internal/anonymous window
        if (t_child_ret != 0 && t_child_ret != t_our_xid)
        {
            x11::Atom t_wm_class_atom =
                x11::XInternAtom(t_xdpy, "WM_CLASS", 0 /* False — create if absent */);
            if (t_wm_class_atom)
            {
                x11::Atom t_ret_type = 0;
                int t_ret_fmt = 0;
                unsigned long t_ret_nitems = 0, t_ret_remaining = 0;
                unsigned char *t_ret_data = NULL;
                int t_prop_rc = x11::XGetWindowProperty(
                    t_xdpy, t_child_ret, t_wm_class_atom,
                    0L, 256L, 0 /* False/no-delete */,
                    0L /* AnyPropertyType */,
                    &t_ret_type, &t_ret_fmt,
                    &t_ret_nitems, &t_ret_remaining, &t_ret_data);
                if (t_prop_rc == 0 /* Success */ && t_ret_data && t_ret_nitems > 0)
                {
                    // WM_CLASS = "instance\0class\0"
                    const char *t_instance = (const char *)t_ret_data;
                    const char *t_class    = t_instance + strlen(t_instance) + 1;
                    fprintf(stderr, "DND diag: foreign WM_CLASS='%s' class='%s'\n",
                            t_instance, t_class);
                    x11::XFree(t_ret_data);
                }
                else
                {
                    fprintf(stderr, "DND diag: foreign WM_CLASS=<none rc=%d>\n", t_prop_rc);
                }
                fflush(stderr);
            }
        }

        // Check if XdndProxy is set on our window. If XWayland set XdndProxy,
        // GDK routes XdndDrop to XWayland's proxy instead of our window directly.
        // gdk_drag_abort sends XdndLeave to dest_xid, which equals the proxy XID
        // when XdndProxy is active — causing XWayland to call xwl_dnd_leave().
        {
            x11::Atom t_xdnd_proxy_atom = x11::XInternAtom(t_xdpy, "XdndProxy", True /* only-if-exists */);
            if (t_xdnd_proxy_atom != 0L /* None */)
            {
                x11::Atom t_ret_type2 = 0; int t_ret_fmt2 = 0;
                unsigned long t_ret_items2 = 0, t_ret_remaining2 = 0;
                unsigned char *t_ret_data2 = NULL;
                x11::XGetWindowProperty(t_xdpy, t_our_xid, t_xdnd_proxy_atom,
                                        0L, 1L, 0 /* False */,
                                        33L /* XA_WINDOW */,
                                        &t_ret_type2, &t_ret_fmt2,
                                        &t_ret_items2, &t_ret_remaining2, &t_ret_data2);
                if (t_ret_data2 && t_ret_items2 > 0)
                {
                    x11::Window t_proxy_xid = *(x11::Window *)t_ret_data2;
                    fprintf(stderr, "DND diag: XdndProxy on our window = 0x%lx (XWayland proxy)\n",
                            (unsigned long)t_proxy_xid);
                    x11::XFree(t_ret_data2);
                }
                else
                    fprintf(stderr, "DND diag: XdndProxy NOT set on our window\n");
            }
            else
                fprintf(stderr, "DND diag: XdndProxy atom does not exist\n");
            fflush(stderr);
        }
    }

    // Log the source window (w) so we can compare with XQueryPointer root_child
    // and gdk_device_get_window_at_position to see if they all agree on the same window.
    fprintf(stderr, "DND diag: source window w=%p XID=0x%lx\n",
            (void*)w, (unsigned long)x11::gdk_x11_window_get_xid(w));
    fflush(stderr);

    // Force GDK to re-query the pointer position. This updates GDK's internal
    // "pointer window" tracking, which may have gone stale while the drag window
    // was on top of the main window. Without this, GDK may continue routing
    // subsequent pointer events to the (now unmapped) drag window and silently
    // discarding them.
    {
        gint t_gdk_wx = 0, t_gdk_wy = 0;
        GdkWindow *t_ptr_window = gdk_device_get_window_at_position(t_pointer, &t_gdk_wx, &t_gdk_wy);
        fprintf(stderr, "DND diag: gdk_device_get_window_at_position = %p XID=0x%lx (%d,%d)\n",
                (void*)t_ptr_window,
                t_ptr_window ? (unsigned long)x11::gdk_x11_window_get_xid(t_ptr_window) : 0UL,
                t_gdk_wx, t_gdk_wy);
        fflush(stderr);
    }

    // Step 4: Release the context reference immediately.
    //
    // We called gdk_drag_drop_done() in Step 1, which set drop_done = TRUE.
    // If GDK_DROP_FINISHED arrives later and gdk_drag_context_handle_source_event
    // processes it, gdk_drag_drop_done is idempotent and the double-call is
    // harmless.  Dropping our ref now removes the context from GDK's internal
    // 'contexts' list, so subsequent events are never routed through it.
    //
    // NOTE: gdk_x11_drag_context_finalize does NOT destroy x11_context->drag_window
    // (only the base-class context->drag_window field, which is NULL for X11).
    // The drag-indicator window therefore survives context finalization.
    fprintf(stderr, "DND cleanup: g_object_unref context\n");
    fflush(stderr);
    g_object_unref(t_context);

    // Step 4b: Explicitly destroy the drag indicator window.
    //
    // gdk_window_hide() above sent XUnmapWindow, which tells XWayland to
    // unmap the Wayland surface — but the surface REMAINS in Mutter's
    // compositor layer stack (just with no buffer / invisible).  Mutter may
    // still route pointer events to this surface (it was the last surface
    // moving under the cursor), causing all clicks after DnD to be silently
    // discarded.
    //
    // gdk_window_destroy() → XDestroyWindow → XWayland calls wl_surface.destroy
    // → Mutter removes the surface from the compositor entirely → pointer events
    // are re-routed to our main window.
    //
    // This is safe here because:
    //   (a) g_object_unref(t_context) was just called — the context no longer
    //       holds a reference to the drag window or calls move_drag_window().
    //   (b) The DnD protocol is complete (XdndFinished already sent/received).
    //   (c) The comment at the top of the function warns against calling
    //       gdk_window_destroy DURING the drag (dangling pointer in context);
    //       that restriction does not apply here.
    if (t_drag_win)
    {
        fprintf(stderr, "DND cleanup: destroying drag_win XID=0x%lx\n",
                t_drag_win ? (unsigned long)x11::gdk_x11_window_get_xid(t_drag_win) : 0UL);
        fflush(stderr);
        gdk_window_destroy(t_drag_win);
        t_drag_win = NULL;
        // Sync so Mutter processes wl_surface.destroy before we restore focus.
        gdk_display_sync(dpy);
        fprintf(stderr, "DND cleanup: drag_win destroyed\n");
        fflush(stderr);
    }

    fprintf(stderr, "DND cleanup: done\n");
    fflush(stderr);

    t_dragboard->SetClipboardWindow(NULL);
    gdk_window_set_cursor(w, NULL);

    // Restore our window's Z-order and input focus after DnD.
    //
    // Root cause (confirmed by diagnostics):
    //   - A foreign X11 window (root child, sibling to ours) sits above our window
    //     after DnD. XQueryPointer returns it as root_child.
    //   - XRaiseWindow on our window is silently ignored by Mutter (focus-stealing
    //     prevention: Mutter won't honor client-initiated raises unless they come
    //     from explicit user interaction at the Wayland level).
    //   - XSetInputFocus / _NET_ACTIVE_WINDOW DO move X11 keyboard focus to our
    //     window, but Mutter's Wayland seat focus stays on the foreign window, so
    //     neither typing nor clicking works.
    //
    // Strategy: pump the GLib/GDK event loop first. Mutter may have queued
    // cleanup events (e.g., XdndFinished acknowledgment, selection release, or
    // Wayland dnd_leave) that — once processed — will cause it to lower the
    // foreign window on its own. Only then do the raise+focus calls.
    {
        // Pump pending GLib/GDK events. The drag context's XdndFinished and
        // any selection events are processed here, which may trigger Mutter to
        // tear down its internal DnD state and lower the proxy window.
        int t_pump_count = 0;
        while (g_main_context_iteration(g_main_context_default(), FALSE))
            t_pump_count++;
        fprintf(stderr, "DND fix: pumped %d g_main_context iterations\n", t_pump_count);
        fflush(stderr);

        guint32 t_server_time = x11::gdk_x11_get_server_time(w);
        fprintf(stderr, "DND fix: server_time=%u\n", (unsigned)t_server_time);
        fflush(stderr);

        // gdk_window_focus sends _NET_ACTIVE_WINDOW (tells Mutter which window
        // we want focused) and calls XSetInputFocus to GDK's focus_window child
        // (the 1x1 InputOnly subwindow that GDK uses for focus tracking).
        // Do NOT call XSetInputFocus directly on our main XID — that moves focus
        // away from GDK's focus_window child, breaking GDK's keyboard dispatch.
        gdk_window_focus(w, t_server_time);

        // XWarpPointer: move the pointer 1 pixel and back within our window.
        // This generates synthetic MotionNotify events which flow through XWayland
        // to Mutter, forcing Mutter to re-evaluate which Wayland surface receives
        // pointer events. If Mutter's Wayland DnD session left input routing
        // stuck (pointing at the ended drag session rather than our window surface),
        // this poke forces it to route events to the correct surface.
        {
            x11::Display *t_xdpy_fix = x11::gdk_x11_display_get_xdisplay(dpy);
            x11::Window t_fix_xid = x11::gdk_x11_window_get_xid(w);
            // Warp to (100,100) relative to our window then back to (100,101).
            // Use safe coords — content windows are at least 200×200.
            x11::XWarpPointer(t_xdpy_fix, 0L, t_fix_xid, 0, 0, 0, 0, 100, 100);
            x11::XWarpPointer(t_xdpy_fix, 0L, t_fix_xid, 0, 0, 0, 0, 100, 101);
            x11::XFlush(t_xdpy_fix);
            fprintf(stderr, "DND fix: XWarpPointer poke done\n");
            fflush(stderr);
        }
    }

    // Flush GDK side too, then sync to ensure all requests were processed.
    gdk_display_flush(dpy);

    // Post-fix: confirm stacking and focus were actually restored.
    {
        x11::Display *t_xdpy_post = x11::gdk_x11_display_get_xdisplay(dpy);
        x11::XSync(t_xdpy_post, False);

        x11::Window t_post_root = 0, t_post_child = 0;
        int t_post_rx = 0, t_post_ry = 0, t_post_wx = 0, t_post_wy = 0;
        unsigned int t_post_mask = 0;
        x11::XQueryPointer(t_xdpy_post, x11::XDefaultRootWindow(t_xdpy_post),
                           &t_post_root, &t_post_child,
                           &t_post_rx, &t_post_ry, &t_post_wx, &t_post_wy, &t_post_mask);

        x11::Window t_post_focus = 0;
        int t_post_revert = 0;
        x11::XGetInputFocus(t_xdpy_post, &t_post_focus, &t_post_revert);

        fprintf(stderr, "DND post-fix: root_child=0x%lx focus=0x%lx revert=%d our_xid=0x%lx\n",
                (unsigned long)t_post_child, (unsigned long)t_post_focus,
                t_post_revert, (unsigned long)x11::gdk_x11_window_get_xid(w));
        fflush(stderr);
    }

    fprintf(stderr, "DND modal: returning t_action=%d\n", (int)t_action);
    fflush(stderr);

    // Restore the original modifier key state
    MCmodifierstate = t_old_modstate;

    return t_action;
}
