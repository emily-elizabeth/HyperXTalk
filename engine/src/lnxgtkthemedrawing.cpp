/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@netscape.com>  (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * This file contains painting functions for each of the GTK widgets.
 * Adapted from the gtkdrawing.c, and gtk+2.0 source.
 * -- tperry 13-11-2025: Migrated to GTK3 - replaced GtkStyle with GtkStyleContext,
 *    GdkDrawable with GdkWindow/cairo_surface_t, and gtk_paint_* with gtk_render_*
 */

#include "lnxprefix.h"

#include "globdefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "filedefs.h"
#include "globals.h"

#include "util.h"

#include "mctheme.h"
#include "lnxdc.h"
#include "lnxgtkthemedrawing.h"

#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// Forward declarations
static gint calculate_arrow_dimensions(GdkRectangle * rect, GdkRectangle * arrow_rect);

// -- tperry 15-11-2025: GTK 3.22+ cairo context creation for offscreen windows
// For GTK 3.22+, gdk_cairo_create() is deprecated and doesn't work well with offscreen windows
// Instead, we create cairo context directly from the window's surface

static int create_count = 0;
static int destroy_count = 0;

static void safe_cairo_clip(cairo_t *cr, const char *caller) {
    (void)caller;
    // cairo_clip(cr); // -- tperry 15-11-2025: Disabled due to system Cairo 1.16.0 crash
    (void)cr;
}

cairo_t* moz_gdk_create_cairo_context(GdkWindow *window)
{
    // Check if this is an offscreen window by trying to get its widget
    GtkWidget *widget = NULL;
    gdk_window_get_user_data(window, (gpointer*)&widget);
    
    create_count++;
    
    // -- tperry 15-11-2025: WORKAROUND - System Cairo 1.16.0 crashes with cairo_clip()
    // Don't use offscreen windows at all - render directly to the window
    // This avoids both the cairo_clip crash and transparency issues
    
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    cairo_t *cr = gdk_cairo_create(window);
    #pragma GCC diagnostic pop
    
    return cr;
}

// -- tperry 12-11-2025: GTK3 migration - This file contains the GTK3 theme drawing engine
// Converted from Mozilla's GTK2 theme engine to use GTK3 APIs
// Uses GtkStyleContext instead of GtkStyle, and gtk_render_* instead of gtk_paint_*
//
// -- tperry 15-11-2025: GTK3 proper widget rendering approach
// GTK3's CSS-based theming requires widgets to be part of a real widget hierarchy.
// We use gtk_widget_draw() to let GTK3 render widgets to a cairo surface, which we then
// composite into LiveCode's rendering. This ensures full theme compatibility across all
// Linux distributions.

// -- tperry 13-11-2025: GTK3 removed GtkProgressBarOrientation constants, define them locally
// These are used as flags to indicate progress bar direction
#define GTK_PROGRESS_LEFT_TO_RIGHT  0
#define GTK_PROGRESS_RIGHT_TO_LEFT  1
#define GTK_PROGRESS_BOTTOM_TO_TOP  2
#define GTK_PROGRESS_TOP_TO_BOTTOM  3

#define MIN_ARROW_WIDTH 6

// -- tperry 13-11-2025: GTK3 - GtkStyle removed, use GtkStyleContext border/padding
// Helper functions to get border thickness from widget
static inline gint get_widget_xthickness(GtkWidget *widget)
{
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    GtkBorder border;
    gtk_style_context_get_border(context, gtk_style_context_get_state(context), &border);
    return border.left;
}

static inline gint get_widget_ythickness(GtkWidget *widget)
{
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    GtkBorder border;
    gtk_style_context_get_border(context, gtk_style_context_get_state(context), &border);
    return border.top;
}

// Compatibility macros - now accept GtkWidget* directly instead of GtkStyle*
// Old GTK2 code: XTHICKNESS(widget->style)
// New GTK3 code: XTHICKNESS(widget)
#define XTHICKNESS(widget) get_widget_xthickness(widget)
#define YTHICKNESS(widget) get_widget_ythickness(widget)

#define WINDOW_IS_MAPPED(window) ((window) && GDK_IS_WINDOW(window) && gdk_window_is_visible(window))

static GtkWidget *gButtonWidget = 0;
static GtkWidget *gProtoWindow = NULL;
static GtkWidget *gProtoLayout = NULL;
static GtkWidget *gCheckboxWidget = 0;
static GtkWidget *gRadiobuttonWidget = 0;
static GtkWidget *gHorizScrollbarWidget = 0;
static GtkWidget *gVertScrollbarWidget = 0;
static GtkWidget *gEntryWidget = 0;
static GtkWidget *gArrowWidget = 0;
static GtkWidget *gDropdownButtonWidget = 0;
static GtkWidget *gHandleBoxWidget = 0;
static GtkWidget *gFrameWidget = 0;
static GtkWidget *gProgressWidget = 0;
static GtkWidget *gTabWidget = 0;
static GtkWidget *gLabelWidget = 0;
static GtkWidget *gLabelOffscreenWindow = 0;  // parent window that keeps gLabelWidget realized
// -- tperry 13-11-2025: GTK3 removed GtkTooltips, tooltips are now per-widget properties
// static GtkTooltips *gTooltipWidget = 0;  // No longer used in GTK3
static GtkWidget *gOptionbuttonWidget = 0;
static GtkWidget *gSpinbuttonWidget = 0;
static GtkWidget *gMenuitemWidget = 0;
static GtkWidget *gHScaleWidget = 0;
static GtkWidget *gVScaleWidget = 0;

static style_prop_t style_prop_func;

static gint
moz_gtk_label_paint(GdkWindow * drawable, GdkRectangle * rect,
                    GdkRectangle * cliprect);

typedef struct _GtkOptionMenuProps GtkOptionMenuProps;

struct _GtkOptionMenuProps
{
	gboolean interior_focus;
	GtkRequisition indicator_size;
	GtkBorder indicator_spacing;
	gint focus_width;
	gint focus_pad;
};

static const GtkOptionMenuProps default_props =
    {
        TRUE,
        { 7, 13 },
        { 7, 5, 2, 2 },		/* Left, right, top, bottom */
        1,
        0

    };

static gint
moz_gtk_container_paint(GdkWindow * drawable, GdkRectangle * rect,
                        GdkRectangle * cliprect, GtkWidgetState * state,
                        gboolean isradio);



gint moz_gtk_enable_style_props(style_prop_t styleGetProp)
{
	style_prop_func = styleGetProp;
	return MOZ_GTK_SUCCESS;
}






static gint setup_widget_prototype(GtkWidget * widget)
{
	// -- tperry 12-11-2025: GTK3 removed GtkStyle, colormaps handled automatically
	
	// -- tperry 16-11-2025: GTK3 doesn't need a prototype window!
	// Widgets can be created standalone and their GtkStyleContext used directly
	// with gtk_render_* functions to render to cairo surfaces
	// No need to realize the widget - gtk_render_* functions work on unrealized widgets

    g_object_set_data(G_OBJECT(widget), "transparent-bg-hint", GINT_TO_POINTER(TRUE));

	return MOZ_GTK_SUCCESS;
}

static gint ensure_button_widget()
{
	if (!gButtonWidget)
	{
		gButtonWidget = gtk_button_new_with_label("M");
		setup_widget_prototype(gButtonWidget);
	}
	return MOZ_GTK_SUCCESS;
}

static gint ensure_scale_widget()
{
	if (!gHScaleWidget)
	{
		GtkAdjustment *adj = (GtkAdjustment*)gtk_adjustment_new(1, 1, 1, 1, 1, 1);
		// -- tperry: GTK3 - gtk_hscale_new/gtk_vscale_new removed, use gtk_scale_new
		gHScaleWidget = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adj);
		gVScaleWidget = gtk_scale_new(GTK_ORIENTATION_VERTICAL, adj);
		
		// Hide value display - we only want the track and slider, not the text labels
		gtk_scale_set_draw_value(GTK_SCALE(gHScaleWidget), FALSE);
		gtk_scale_set_draw_value(GTK_SCALE(gVScaleWidget), FALSE);

		// Add to the prototype window like all other widgets
		setup_widget_prototype(gHScaleWidget);
		setup_widget_prototype(gVScaleWidget);

		// -- tperry 16-11-2025: Don't unref - gtk_hscale_new/gtk_vscale_new take ownership without adding ref
		// g_object_unref(adj);
	}
	return MOZ_GTK_SUCCESS;
}

static gint ensure_menuitem_widget()
{
	if (!gMenuitemWidget)
	{
		// The menu item must live inside a GtkMenu so that the CSS cascade
		// includes the ".menu" ancestor selector used by most GTK3 themes for
		// hover/prelight styling (e.g. ".menu .menuitem:hover { ... }").
		// We realise the whole hierarchy so the style context is fully resolved.
		GtkWidget *menu = gtk_menu_new();
		gMenuitemWidget = gtk_menu_item_new_with_label("M");
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), gMenuitemWidget);
		gtk_widget_realize(menu);
		gtk_widget_realize(gMenuitemWidget);
		// Keep a ref on the item so it isn't destroyed when the menu is cleaned up.
		g_object_ref(gMenuitemWidget);
	}
	return MOZ_GTK_SUCCESS;
}

static gint ensure_spinbutton_widget()
{
	if (!gSpinbuttonWidget)
	{
		GtkAdjustment *adj = (GtkAdjustment*)gtk_adjustment_new(1, 1, 1, 1, 1, 1);
		gSpinbuttonWidget = gtk_spin_button_new(adj, 1, 1);
		setup_widget_prototype(gSpinbuttonWidget);
		// -- tperry 16-11-2025: Don't unref - gtk_spin_button_new takes ownership without adding ref
		// g_object_unref(adj);
	}
	return MOZ_GTK_SUCCESS;
}


static gint ensure_checkbox_widget()
{
	if (!gCheckboxWidget)
	{
		gCheckboxWidget = gtk_check_button_new_with_label("M");
		setup_widget_prototype(gCheckboxWidget);
		// Add CSS class for proper GTK3 theme styling
		GtkStyleContext *context = gtk_widget_get_style_context(gCheckboxWidget);
		gtk_style_context_add_class(context, GTK_STYLE_CLASS_CHECK);
	}
	return MOZ_GTK_SUCCESS;
}

static gint ensure_radiobutton_widget()
{
	if (!gRadiobuttonWidget)
	{
		gRadiobuttonWidget =
		    gtk_radio_button_new_with_label(NULL, "M");
		setup_widget_prototype(gRadiobuttonWidget);
		// Add CSS class for proper GTK3 theme styling
		GtkStyleContext *context = gtk_widget_get_style_context(gRadiobuttonWidget);
		gtk_style_context_add_class(context, GTK_STYLE_CLASS_RADIO);
	}
	return MOZ_GTK_SUCCESS;
}

static gint ensure_optionbutton_widget()
{
	if (!gOptionbuttonWidget)
	{
		// -- tperry 13-11-2025: GTK3 - GtkOptionMenu removed, use GtkComboBox
		gOptionbuttonWidget = gtk_combo_box_new();
		setup_widget_prototype(gOptionbuttonWidget);
	}
	return MOZ_GTK_SUCCESS;
}

static gint ensure_scrollbar_widget()
{
	if (!gVertScrollbarWidget)
	{
		// -- tperry: GTK3 - gtk_vscrollbar_new removed, use gtk_scrollbar_new
		gVertScrollbarWidget = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, NULL);
		setup_widget_prototype(gVertScrollbarWidget);
		// Add CSS classes for proper GTK3 theme styling
		GtkStyleContext *context = gtk_widget_get_style_context(gVertScrollbarWidget);
		gtk_style_context_add_class(context, GTK_STYLE_CLASS_SCROLLBAR);
		gtk_style_context_add_class(context, GTK_STYLE_CLASS_VERTICAL);
		// gtk_widget_show() removed: showing an unparented scrollbar causes
		// GTK to schedule an internal size-allocate with a 0×0 rect, which is
		// smaller than the minimum arrow sizes → gtk_box_gadget_distribute
		// asserts 'size >= 0'.  The style context connects to the display's
		// CSS providers without needing the widget to be shown.
	}
	if (!gHorizScrollbarWidget)
	{
		// -- tperry: GTK3 - gtk_hscrollbar_new removed, use gtk_scrollbar_new
		gHorizScrollbarWidget = gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, NULL);
		setup_widget_prototype(gHorizScrollbarWidget);
		// Add CSS classes for proper GTK3 theme styling
		GtkStyleContext *context = gtk_widget_get_style_context(gHorizScrollbarWidget);
		gtk_style_context_add_class(context, GTK_STYLE_CLASS_SCROLLBAR);
		gtk_style_context_add_class(context, GTK_STYLE_CLASS_HORIZONTAL);
	}
	return MOZ_GTK_SUCCESS;
}

static gint ensure_entry_widget()
{
	if (!gEntryWidget)
	{
		gEntryWidget = gtk_entry_new();

		setup_widget_prototype(gEntryWidget);
	}
	return MOZ_GTK_SUCCESS;
}

static gint ensure_arrow_widget()
{
	if (!gArrowWidget)
	{
		gDropdownButtonWidget = gtk_button_new();
		setup_widget_prototype(gDropdownButtonWidget);
		// -- tperry: GTK3 - GtkArrow removed in GTK3.14; use GtkImage as child placeholder.
		//            Style context from it is used with gtk_render_arrow() which just
		//            needs any context (renders using the foreground colour).
		gArrowWidget = gtk_image_new();
		gtk_container_add(GTK_CONTAINER(gDropdownButtonWidget),
		                    gArrowWidget);
		// gtk_widget_realize() removed: gDropdownButtonWidget has no parent
		// window so gArrowWidget is unanchored — realize would fire the GTK3
		// assertion 'anchored || GTK_IS_INVISIBLE' and then cause a BadWindow
		// X error when GTK tried to read properties from the non-existent
		// GDK window.  Realization is not needed: all callers use
		// gtk_widget_get_style_context() / gtk_render_arrow(), both of which
		// work on unrealized widgets.
	}
	return MOZ_GTK_SUCCESS;
}

static gint ensure_handlebox_widget()
{
	if (!gHandleBoxWidget)
	{
		// -- tperry: GTK3 - GtkHandleBox deprecated in GTK3.4 / removed later.
		//            Use GtkToolbar which gives the correct "toolbar" CSS node
		//            for background/frame rendering of toolbars and grippers.
		gHandleBoxWidget = gtk_toolbar_new();
		setup_widget_prototype(gHandleBoxWidget);
	}
	return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - GtkTooltips removed, use GtkWindow with tooltip style class
static GtkWidget *gTooltipWindow = NULL;

static gint ensure_tooltip_widget()
{
	if (!gTooltipWindow)
	{
		// In GTK3, create a window with the tooltip style class for theme rendering
		gTooltipWindow = gtk_window_new(GTK_WINDOW_POPUP);
		GtkStyleContext *context = gtk_widget_get_style_context(gTooltipWindow);
		gtk_style_context_add_class(context, GTK_STYLE_CLASS_TOOLTIP);
		gtk_widget_realize(gTooltipWindow);
	}
	return MOZ_GTK_SUCCESS;
}

static gint ensure_tab_widget()
{
	if (!gTabWidget)
	{
		gTabWidget = gtk_notebook_new();
		setup_widget_prototype(gTabWidget);
	}
	return MOZ_GTK_SUCCESS;
}

static gint ensure_label_widget()
{
	if (!gLabelWidget)
	{
		// The label must be realized inside an offscreen window so that its
		// GtkStyleContext is connected to the actual display's CSS providers.
		// Without this, gtk_style_context_lookup_color() cannot find named
		// theme colours like @theme_selected_bg_color.
		gLabelOffscreenWindow = gtk_offscreen_window_new();
		gLabelWidget = gtk_label_new("M");
		gtk_container_add(GTK_CONTAINER(gLabelOffscreenWindow), gLabelWidget);
		gtk_widget_realize(gLabelOffscreenWindow);
		gtk_widget_realize(gLabelWidget);
		// Don't destroy gLabelOffscreenWindow — that would unrealize gLabelWidget.
		// It is cleaned up in moz_gtk_shutdown().
	}
	return MOZ_GTK_SUCCESS;
}

static gint ensure_progress_widget()
{

	if (!gProgressWidget)
	{
		gProgressWidget = gtk_progress_bar_new();
		setup_widget_prototype(gProgressWidget);
		// -- tperry 16-11-2025: Show widget so GTK will render it
		gtk_widget_show(gProgressWidget);
	}
	return MOZ_GTK_SUCCESS;
}

static gint ensure_frame_widget()
{
	if (!gFrameWidget)
	{
		gFrameWidget = gtk_frame_new(NULL);
		setup_widget_prototype(gFrameWidget);
	}
	return MOZ_GTK_SUCCESS;
}

static GtkStateType ConvertGtkState(GtkWidgetState * state)
{
	if (state->disabled)
		return GTK_STATE_INSENSITIVE;
	else if (state->inHover)
		return (state->active ? GTK_STATE_ACTIVE : GTK_STATE_PRELIGHT);
	else if(state->active)
		return GTK_STATE_ACTIVE;
	else
		return GTK_STATE_NORMAL;
}

// -- tperry 13-11-2025: GTK3 removed GdkGC - these functions are no longer needed
// In GTK3, Cairo handles all transformations and drawing
// These functions are commented out as they use GdkGC which doesn't exist in GTK3
/*
static gint TSOffsetStyleGCArray(GdkGC ** gcs, gint xorigin, gint yorigin)
{
	// No-op in GTK3 - GdkGC doesn't exist
	return MOZ_GTK_SUCCESS;
}
*/

// This function signature is also updated to remove GtkStyle parameter
// In GTK3, we don't need to offset GCs - Cairo handles transformations
/*
static gint TSOffsetStyleGCs(GtkStyle * style, gint xorigin, gint yorigin)
{
	// No-op in GTK3 - Cairo handles transformations
	return MOZ_GTK_SUCCESS;
}
*/

static int moz_gtk_generic_container_paint(GdkWindow * drawable,
        GdkRectangle * rect,
        GdkRectangle * cliprect,
        GtkWidgetState * state,
        GtkWidget *widget,
        const gchar *name)
{
	// -- tperry 12-11-2025: GTK3 uses GtkStyleContext and gtk_render functions
	GtkStateType state_type;
	GtkStyleContext *context = gtk_widget_get_style_context(widget);
	
	if(state)
		state_type = ConvertGtkState(state);
	else
		state_type = GTK_STATE_NORMAL;

	if (state_type != GTK_STATE_NORMAL
	        && state_type != GTK_STATE_PRELIGHT)
		state_type = GTK_STATE_NORMAL;

	// Create cairo context from drawable (GdkWindow in GTK3)
	cairo_t *cr = moz_gdk_create_cairo_context((GdkWindow*)drawable);
	if (cliprect) {
		gdk_cairo_rectangle(cr, cliprect);
		safe_cairo_clip(cr, "moz_gtk_generic_container_paint");
	}

	// GTK3: Use gtk_render_background instead of gtk_paint_flat_box
	gtk_render_background(context, cr,
	                      rect->x, rect->y, rect->width, rect->height);

	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}


gint
moz_gtk_widget_get_focus(GtkWidget* widget, gboolean* interior_focus,
                         gint* focus_width, gint* focus_pad) 
{
    gtk_widget_style_get (widget,
                          "interior-focus", interior_focus,
                          "focus-line-width", focus_width,
                          "focus-padding", focus_pad,
                          NULL);

    return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_button_get_default_overflow(gint* border_top, gint* border_left,
                                    gint* border_bottom, gint* border_right)
{
    GtkBorder* default_outside_border;

    ensure_button_widget();
    gtk_widget_style_get(gButtonWidget,
                         "default-outside-border", &default_outside_border,
                         NULL);

    if (default_outside_border) {
        *border_top = default_outside_border->top;
        *border_left = default_outside_border->left;
        *border_bottom = default_outside_border->bottom;
        *border_right = default_outside_border->right;
        gtk_border_free(default_outside_border);
    } else {
        *border_top = *border_left = *border_bottom = *border_right = 0;
    }
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_button_get_default_border(gint* border_top, gint* border_left,
                                  gint* border_bottom, gint* border_right)
{
    GtkBorder* default_border;

    ensure_button_widget();
    gtk_widget_style_get(gButtonWidget,
                         "default-border", &default_border,
                         NULL);

    if (default_border) {
        *border_top = default_border->top;
        *border_left = default_border->left;
        *border_bottom = default_border->bottom;
        *border_right = default_border->right;
        gtk_border_free(default_border);
    } else {
        *border_top = *border_left = *border_bottom = *border_right = 1;
    }
    return MOZ_GTK_SUCCESS;
}

static gint
moz_gtk_button_paint(GdkWindow * drawable, GdkRectangle * rect,
                     GdkRectangle * cliprect, GtkWidgetState * state,
                     GtkReliefStyle relief, GtkWidget * widget)
{
    // -- tperry 12-11-2025: GTK3 conversion
    GtkShadowType shadow_type;
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    GtkStateType button_state = ConvertGtkState(state);
    GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
    gint x = rect->x, y=rect->y, width=rect->width, height=rect->height;

    gboolean interior_focus;
    gint focus_width, focus_pad;

    moz_gtk_widget_get_focus(widget, &interior_focus, &focus_width, &focus_pad);

    // GTK3: No need for gdk_window_set_back_pixmap or gdk_window_clear_area
    
    // Convert state to GTK3 state flags
    if (button_state == GTK_STATE_ACTIVE)
        state_flags = GTK_STATE_FLAG_ACTIVE;
    else if (button_state == GTK_STATE_PRELIGHT)
        state_flags = GTK_STATE_FLAG_PRELIGHT;
    else if (button_state == GTK_STATE_INSENSITIVE)
        state_flags = GTK_STATE_FLAG_INSENSITIVE;
    
    gtk_style_context_set_state(context, state_flags);

    // GTK3: Use state flags instead of widget flags
    if (state->isDefault)
        state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_ACTIVE);

    // -- tperry 13-11-2025: GTK3 - GtkButton members are private, use gtk_button_set_relief()
    gtk_button_set_relief(GTK_BUTTON(widget), relief);

    if (state->focused && !state->disabled)
        state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_FOCUSED);
    
    gtk_style_context_set_state(context, state_flags);

    if (!interior_focus && state->focused) {
        x += focus_width + focus_pad;
        y += focus_width + focus_pad;
        width -= 2 * (focus_width + focus_pad);
        height -= 2 * (focus_width + focus_pad);
    }

    shadow_type = button_state == GTK_STATE_ACTIVE ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
    
    // Create cairo context for drawing
    cairo_t *cr = moz_gdk_create_cairo_context(drawable);
    if (cliprect) {
        // cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
        // cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
    }
 
    if (state->isDefault && relief == GTK_RELIEF_NORMAL) {
        /* handle default borders both outside and inside the button */
        gint default_top, default_left, default_bottom, default_right;
        moz_gtk_button_get_default_overflow(&default_top, &default_left,
                                            &default_bottom, &default_right);
        x -= default_left;
        y -= default_top;
        width += default_left + default_right;
        height += default_top + default_bottom;
        
        // GTK3: Use gtk_render_background and gtk_render_frame
        gtk_render_background(context, cr, x, y, width, height);
        gtk_render_frame(context, cr, x, y, width, height);

        moz_gtk_button_get_default_border(&default_top, &default_left,
                                          &default_bottom, &default_right);
        x += default_left;
        y += default_top;
        width -= (default_left + default_right);
        height -= (default_top + default_bottom);
    }
 
    if (relief != GTK_RELIEF_NONE ||
        (button_state != GTK_STATE_NORMAL &&
         button_state != GTK_STATE_INSENSITIVE)) {
        // GTK3: Use gtk_render functions
        gtk_render_background(context, cr, x, y, width, height);
        gtk_render_frame(context, cr, x, y, width, height);
    }

    if (state->focused) {
        // GTK3: Get border/padding from style context
        GtkBorder border;
        gtk_style_context_get_border(context, state_flags, &border);
        
        if (interior_focus) {
            x += border.left + focus_pad;
            y += border.top + focus_pad;
            width -= (border.left + border.right) + 2 * focus_pad;
            height -= (border.top + border.bottom) + 2 * focus_pad;
        } else {
            x -= focus_width + focus_pad;
            y -= focus_width + focus_pad;
            width += 2 * (focus_width + focus_pad);
            height += 2 * (focus_width + focus_pad);
        }

        // GTK3: Use gtk_render_focus
        gtk_render_focus(context, cr, x, y, width, height);
    }

    cairo_destroy(cr);
    
    // GTK3: Reset state flags
    gtk_style_context_set_state(context, GTK_STATE_FLAG_NORMAL);
    return MOZ_GTK_SUCCESS;
}

gint moz_gtk_initDL()
{
	return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_checkbox_get_metrics(gint * indicator_size, gint * indicator_spacing)
{
	ensure_checkbox_widget();

	if (indicator_size)
	{
		gtk_widget_style_get(gCheckboxWidget, "indicator-size",
		                       indicator_size, NULL);
	}

	if (indicator_spacing)
	{
		gtk_widget_style_get(gCheckboxWidget, "indicator-spacing",
		                       indicator_spacing, NULL);
	}

	return MOZ_GTK_SUCCESS;
}

gint moz_gtk_radiobutton_get_metrics(gint * indicator_size,
                                     gint * indicator_spacing)
{
	ensure_radiobutton_widget();

	if (indicator_size)
	{
		gtk_widget_style_get(gRadiobuttonWidget, "indicator-size",
		                       indicator_size, NULL);
	}

	if (indicator_spacing)
	{
		gtk_widget_style_get(gRadiobuttonWidget, "indicator-spacing",
		                       indicator_spacing, NULL);
	}

	return MOZ_GTK_SUCCESS;
}

// -- tperry 16-11-2025: New function to render scrollbar thumb using gtk_widget_draw
cairo_surface_t*
moz_gtk_scrollbar_thumb_paint_to_surface(GtkThemeWidgetType widget,
                                          GdkRectangle * rect,
                                          GtkWidgetState * state,
                                          int *out_width, int *out_height)
{
    GtkWidget *scrollbar;
    GtkAdjustment *adj;

    ensure_scrollbar_widget();

    if (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL)
        scrollbar = gHorizScrollbarWidget;
    else
        scrollbar = gVertScrollbarWidget;

    adj = gtk_range_get_adjustment(GTK_RANGE(scrollbar));

    int width = rect->width;
    int height = rect->height;
    
    if (out_width) *out_width = width;
    if (out_height) *out_height = height;

    // Set adjustment values to position the thumb
    gtk_adjustment_set_lower(adj, 0);
    gtk_adjustment_set_value(adj, state->curpos);
    gtk_adjustment_set_upper(adj, state->maxpos);
    gtk_adjustment_set_page_size(adj, (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL) ? width / 4 : height / 4);

    // Set widget size allocation
    GtkAllocation allocation;
    allocation.x = 0;
    allocation.y = 0;
    allocation.width = width;
    allocation.height = height;
    gtk_widget_size_allocate(scrollbar, &allocation);
    
    // Clear the alloc_needed flag by getting preferred size
    GtkRequisition minimum_size, natural_size;
    gtk_widget_get_preferred_size(scrollbar, &minimum_size, &natural_size);

    // Create surface for rendering
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);
    
    // Clear to transparent
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    
    // Use neutral gray color RGB(137,137,137)
    double red = 137.0 / 255.0;
    double green = 137.0 / 255.0;
    double blue = 137.0 / 255.0;
    
    // Draw rounded rectangle with 6px corners
    double radius = 6.0;
    double x = 0, y = 0;
    double w = width, h = height;
    
    // Create rounded rectangle path
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - radius, y + radius, radius, -M_PI/2, 0);
    cairo_arc(cr, x + w - radius, y + h - radius, radius, 0, M_PI/2);
    cairo_arc(cr, x + radius, y + h - radius, radius, M_PI/2, M_PI);
    cairo_arc(cr, x + radius, y + radius, radius, M_PI, 3*M_PI/2);
    cairo_close_path(cr);
    
    // Fill with hilite color
    cairo_set_source_rgb(cr, red, green, blue);
    cairo_fill(cr);
    
    cairo_destroy(cr);
    cairo_surface_flush(surface);
    
    return surface;
}

// -- tperry 15-11-2025: New function to render scrollbar trough to cairo surface
cairo_surface_t*
moz_gtk_scrollbar_trough_paint_to_surface(GtkThemeWidgetType widget,
                                           GdkRectangle * rect,
                                           GtkWidgetState * state,
                                           int *out_width, int *out_height)
{
    GtkWidget *scrollbar;

    ensure_scrollbar_widget();

    if (widget == MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL)
        scrollbar = gHorizScrollbarWidget;
    else
        scrollbar = gVertScrollbarWidget;

    int width = rect->width;
    int height = rect->height;
    
    if (out_width) *out_width = width;
    if (out_height) *out_height = height;

    GtkStyleContext *context = gtk_widget_get_style_context(scrollbar);
    gtk_style_context_save(context);
    gtk_style_context_add_class(context, GTK_STYLE_CLASS_TROUGH);
    
    // Set state
    GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
    gtk_style_context_set_state(context, state_flags);
    
    // Create surface for rendering
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);
    
    // Get button background color for the track (same as progress bar and scale track)
    ensure_button_widget();
    GtkStyleContext *button_context = gtk_widget_get_style_context(gButtonWidget);
    GdkRGBA bg_color = {0.8, 0.8, 0.8, 1.0};
    if (!gtk_style_context_lookup_color(button_context, "theme_bg_color", &bg_color))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_style_context_get_background_color(button_context, GTK_STATE_FLAG_NORMAL, &bg_color);
G_GNUC_END_IGNORE_DEPRECATIONS
    }
    
    // Fill track with button background color
    cairo_set_source_rgba(cr, bg_color.red, bg_color.green, bg_color.blue, bg_color.alpha);
    cairo_paint(cr);
    
    cairo_destroy(cr);
    gtk_style_context_restore(context);
    
    return surface;
}

// -- tperry 16-11-2025: New function to render progressbar using gtk_widget_draw
// *** DO NOT MODIFY - This function works correctly with GTK3 theme colors ***
// *** Uses gtk_widget_draw() which requires the prototype window to be shown ***
cairo_surface_t*
moz_gtk_progressbar_paint_to_surface(GdkRectangle * rect,
                                      int *out_width, int *out_height)
{
    ensure_progress_widget();
    
    int width = rect->width;
    int height = rect->height;
    
    if (out_width) *out_width = width;
    if (out_height) *out_height = height;
    
    // Set progress to 0 for trough-only rendering
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gProgressWidget), 0.0);
    
    // Set widget size allocation
    GtkAllocation allocation;
    allocation.x = 0;
    allocation.y = 0;
    allocation.width = width;
    allocation.height = height;
    gtk_widget_size_allocate(gProgressWidget, &allocation);
    
    // Clear the alloc_needed flag
    GtkRequisition minimum_size, natural_size;
    gtk_widget_get_preferred_size(gProgressWidget, &minimum_size, &natural_size);
    
    // Create surface for rendering
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);
    
    // Get button background color for the trough
    ensure_button_widget();
    GtkStyleContext *button_context = gtk_widget_get_style_context(gButtonWidget);
    GdkRGBA bg_color = {0.8, 0.8, 0.8, 1.0};
    if (!gtk_style_context_lookup_color(button_context, "theme_bg_color", &bg_color))
    {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
        gtk_style_context_get_background_color(button_context, GTK_STATE_FLAG_NORMAL, &bg_color);
G_GNUC_END_IGNORE_DEPRECATIONS
    }
    
    // Fill trough with button background color
    cairo_set_source_rgba(cr, bg_color.red, bg_color.green, bg_color.blue, bg_color.alpha);
    cairo_paint(cr);
    
    cairo_destroy(cr);
    
    return surface;
}

// -- tperry 16-11-2025: New function to render progress chunk using gtk_widget_draw
// *** DO NOT MODIFY - This function works correctly with GTK3 theme colors ***
// *** The rect size represents the filled portion, fraction is set to 100% ***
// *** Uses gtk_widget_draw() which requires the prototype window to be shown ***
cairo_surface_t*
moz_gtk_progress_chunk_paint_to_surface(GdkRectangle * rect,
                                         gint flags,
                                         int *out_width, int *out_height)
{
    ensure_progress_widget();
    
    int width = rect->width;
    int height = rect->height;
    
    if (out_width) *out_width = width;
    if (out_height) *out_height = height;
    
    // Set orientation and inverted property
    GtkOrientation orientation = GTK_ORIENTATION_HORIZONTAL;
    gboolean inverted = FALSE;
    
    if (flags & GTK_PROGRESS_TOP_TO_BOTTOM) {
        orientation = GTK_ORIENTATION_VERTICAL;
        inverted = FALSE;
    }
    else if (flags & GTK_PROGRESS_BOTTOM_TO_TOP) {
        orientation = GTK_ORIENTATION_VERTICAL;
        inverted = TRUE;
    }
    else if (flags & GTK_PROGRESS_RIGHT_TO_LEFT) {
        orientation = GTK_ORIENTATION_HORIZONTAL;
        inverted = TRUE;
    }
    else {
        orientation = GTK_ORIENTATION_HORIZONTAL;
        inverted = FALSE;
    }
    
    gtk_orientable_set_orientation(GTK_ORIENTABLE(gProgressWidget), orientation);
    gtk_progress_bar_set_inverted(GTK_PROGRESS_BAR(gProgressWidget), inverted);
    
    // The width/height passed in represents the filled portion
    // We need to render a progress bar that shows this much fill
    // Set fraction to 1.0 (100%) since the rect already represents the filled size
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(gProgressWidget), 1.0);
    
    // Set widget size allocation
    GtkAllocation allocation;
    allocation.x = 0;
    allocation.y = 0;
    allocation.width = width;
    allocation.height = height;
    gtk_widget_size_allocate(gProgressWidget, &allocation);
    
    // Clear the alloc_needed flag
    GtkRequisition minimum_size, natural_size;
    gtk_widget_get_preferred_size(gProgressWidget, &minimum_size, &natural_size);
    
    // Create surface for rendering
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);
    
    // Get the hilite color (same as LiveCode's "get the hilitecolor")
    uint2 r, g, b;
    moz_gtk_get_widget_color(GTK_STATE_SELECTED, r, g, b);
    
    // Convert from uint16 (0-65535) to double (0.0-1.0)
    double red = r / 65535.0;
    double green = g / 65535.0;
    double blue = b / 65535.0;
    
    // Fill progress chunk with hilite color
    cairo_set_source_rgb(cr, red, green, blue);
    cairo_paint(cr);
    
    cairo_destroy(cr);
    
    return surface;
}

// Forward declarations for helpers defined later in this file
static cairo_surface_t* make_transparent_surface(int width, int height, cairo_t **cr_out);
static GtkStyleContext* build_scale_trough_context(bool is_horizontal, GtkStateFlags state_flags);
static GtkStyleContext* build_scale_slider_context(bool is_horizontal, GtkStateFlags state_flags);

// -- tperry 16-11-2025 / 20-06-2026: Render scale track.
// Uses a standalone CSS node path context (scale > trough) so GTK3.20+ CSS
// node-name selectors match, with the real widget's context set as parent so
// the full theme CSS cascade is inherited.
cairo_surface_t*
moz_gtk_scale_track_paint_to_surface(GtkThemeWidgetType type,
                                      GdkRectangle * rect,
                                      int *out_width, int *out_height)
{
    int width = rect->width;
    int height = rect->height;

    if (out_width) *out_width = width;
    if (out_height) *out_height = height;

    bool is_horizontal = (type == MOZ_GTK_SCALE_TRACK_HORIZONTAL);
    ensure_scale_widget();

    // Build CSS node path context; parent = real widget context for theme cascade
    GtkStyleContext *ctx = build_scale_trough_context(is_horizontal, GTK_STATE_FLAG_NORMAL);
    gtk_style_context_set_parent(ctx,
        gtk_widget_get_style_context(is_horizontal ? gHScaleWidget : gVScaleWidget));

    cairo_t *cr;
    cairo_surface_t *surface = make_transparent_surface(width, height, &cr);

    // Trough is a thin strip — query CSS min-size, fall back to 4px
    gint trough_thick = 0;
    gtk_style_context_get(ctx, GTK_STATE_FLAG_NORMAL,
        is_horizontal ? "min-height" : "min-width", &trough_thick, NULL);
    if (trough_thick < 2) trough_thick = 4;

    double tx, ty, tw, th;
    if (is_horizontal) {
        tx = 0; tw = width;
        ty = (height - trough_thick) / 2.0;
        th = trough_thick;
    } else {
        ty = 0; th = height;
        tx = (width - trough_thick) / 2.0;
        tw = trough_thick;
    }

    gtk_render_background(ctx, cr, tx, ty, tw, th);
    gtk_render_frame    (ctx, cr, tx, ty, tw, th);
    g_object_unref(ctx);

    cairo_destroy(cr);
    return surface;
}

// -- tperry 20-06-2026: Render scale thumb (slider knob).
// CSS node path: scale > trough > slider. Parent = real widget context.
// gtk_render_slider() applies border-radius so Adwaita renders a circle.
cairo_surface_t*
moz_gtk_scale_thumb_paint_to_surface(GtkThemeWidgetType type,
                                      GdkRectangle * rect,
                                      GtkWidgetState * state,
                                      int *out_width, int *out_height)
{
    int width = rect->width;
    int height = rect->height;

    if (out_width) *out_width = width;
    if (out_height) *out_height = height;

    bool is_horizontal = (type == MOZ_GTK_SCALE_THUMB_HORIZONTAL);

    GtkStateFlags sf = GTK_STATE_FLAG_NORMAL;
    if (state && state->disabled)        sf = GTK_STATE_FLAG_INSENSITIVE;
    else if (state && state->active)     sf = (GtkStateFlags)(sf | GTK_STATE_FLAG_ACTIVE);
    else if (state && state->inHover)    sf = (GtkStateFlags)(sf | GTK_STATE_FLAG_PRELIGHT);

    ensure_scale_widget();

    // Build CSS node path context; parent = real widget context for theme cascade
    GtkStyleContext *ctx = build_scale_slider_context(is_horizontal, sf);
    gtk_style_context_set_parent(ctx,
        gtk_widget_get_style_context(is_horizontal ? gHScaleWidget : gVScaleWidget));

    cairo_t *cr;
    cairo_surface_t *surface = make_transparent_surface(width, height, &cr);

    // Query natural thumb size from CSS; fall back to a sensible square
    gint thumb_w = 0, thumb_h = 0;
    gtk_style_context_get(ctx, sf,
        "min-width",  &thumb_w,
        "min-height", &thumb_h, NULL);
    if (thumb_w < 4) thumb_w = MIN(width,  14);
    if (thumb_h < 4) thumb_h = MIN(height, 14);
    // Clamp to rect bounds so the thumb isn't clipped by the surface edges
    thumb_w = MIN(thumb_w, width);
    thumb_h = MIN(thumb_h, height);

    // Center the thumb within the allocated rect
    double tx = (width  - thumb_w) / 2.0;
    double ty = (height - thumb_h) / 2.0;

    // gtk_render_slider applies border-radius (circle in Adwaita)
    gtk_render_slider(ctx, cr, tx, ty, thumb_w, thumb_h,
        is_horizontal ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

    g_object_unref(ctx);

    cairo_destroy(cr);
    return surface;
}

// -- tperry 15-11-2025: New function to render dropdown arrow to cairo surface
cairo_surface_t*
moz_gtk_dropdown_arrow_paint_to_surface(GdkRectangle * rect,
                                         GtkWidgetState * state,
                                         int *out_width, int *out_height)
{
    ensure_arrow_widget();
    
    int width = rect->width;
    int height = rect->height;
    
    if (out_width) *out_width = width;
    if (out_height) *out_height = height;
    
    // Create surface for rendering
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);
    
    // Clear to transparent
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    
    // First render the button background using our button renderer
    GtkStyleContext *button_context = gtk_widget_get_style_context(gDropdownButtonWidget);
    gtk_style_context_save(button_context);
    
    GtkStateType button_state = ConvertGtkState(state);
    GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
    
    if (button_state == GTK_STATE_ACTIVE)
        state_flags = GTK_STATE_FLAG_ACTIVE;
    else if (button_state == GTK_STATE_PRELIGHT)
        state_flags = GTK_STATE_FLAG_PRELIGHT;
    else if (button_state == GTK_STATE_INSENSITIVE)
        state_flags = GTK_STATE_FLAG_INSENSITIVE;
    
    gtk_style_context_set_state(button_context, state_flags);
    
    // Render button background
    gtk_render_background(button_context, cr, 0, 0, width, height);
    gtk_render_frame(button_context, cr, 0, 0, width, height);
    
    gtk_style_context_restore(button_context);
    
    // Calculate arrow position (mirrors gtkbutton's child positioning)
    GdkRectangle arrow_rect, real_arrow_rect;
    arrow_rect.x = 1 + XTHICKNESS(gDropdownButtonWidget);
    arrow_rect.y = 1 + YTHICKNESS(gDropdownButtonWidget);
    arrow_rect.width = MAX(1, width - arrow_rect.x * 2);
    arrow_rect.height = MAX(1, height - arrow_rect.y * 2);
    
    calculate_arrow_dimensions(&arrow_rect, &real_arrow_rect);
    
    real_arrow_rect.width = real_arrow_rect.height = (int)
                            (MIN(real_arrow_rect.width, real_arrow_rect.height) * 0.9);
    
    real_arrow_rect.x = (gint)
                        floor(arrow_rect.x +
                              ((arrow_rect.width - real_arrow_rect.width) / 2) + 0.5);
    real_arrow_rect.y = (gint)
                        floor(arrow_rect.y +
                              ((arrow_rect.height - real_arrow_rect.height) / 2) + 0.5);
    
    // Render the arrow
    GtkStyleContext *arrow_context = gtk_widget_get_style_context(gArrowWidget);
    gtk_style_context_save(arrow_context);
    
    state_flags = GTK_STATE_FLAG_NORMAL;
    if (state->disabled)
        state_flags = GTK_STATE_FLAG_INSENSITIVE;
    else if (state->active)
        state_flags = GTK_STATE_FLAG_ACTIVE;
    else if (state->inHover)
        state_flags = GTK_STATE_FLAG_PRELIGHT;
    
    gtk_style_context_set_state(arrow_context, state_flags);
    
    // Render arrow pointing down
    gtk_render_arrow(arrow_context, cr, G_PI, // Down arrow
                     real_arrow_rect.x, real_arrow_rect.y,
                     MIN(real_arrow_rect.width, real_arrow_rect.height));
    
    gtk_style_context_restore(arrow_context);
    cairo_destroy(cr);
    
    return surface;
}

// -- tperry 15-11-2025: New function to render option button to cairo surface
cairo_surface_t*
moz_gtk_optionbutton_paint_to_surface(GdkRectangle * rect,
                                       GtkWidgetState * state,
                                       int *out_width, int *out_height)
{
    ensure_optionbutton_widget();
    GtkWidget *widget = gOptionbuttonWidget;
    
    int width = rect->width;
    int height = rect->height;
    
    if (out_width) *out_width = width;
    if (out_height) *out_height = height;
    
    // Get border width
    gint border_width = gtk_container_get_border_width(GTK_CONTAINER(widget));
    
    GdkRectangle button_area;
    button_area.x = border_width;
    button_area.y = border_width;
    button_area.width = width - 2 * border_width;
    button_area.height = height - 2 * border_width;
    
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gtk_style_context_save(context);
    
    // Convert state
    GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
    if (state) {
        if (state->disabled)
            state_flags = GTK_STATE_FLAG_INSENSITIVE;
        else if (state->active)
            state_flags = GTK_STATE_FLAG_ACTIVE;
        else if (state->inHover)
            state_flags = GTK_STATE_FLAG_PRELIGHT;
        if (state->focused)
            state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_FOCUSED);
    }
    
    gtk_style_context_set_state(context, state_flags);
    
    // Create surface for rendering
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);
    
    // Clear to transparent
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    
    // Render combobox button
    gtk_render_background(context, cr, button_area.x, button_area.y,
                          button_area.width, button_area.height);
    gtk_render_frame(context, cr, button_area.x, button_area.y,
                     button_area.width, button_area.height);
    
    // Render focus if needed
    if (state && state->focused)
    {
        gtk_render_focus(context, cr, 0, 0, width, height);
    }
    
    cairo_destroy(cr);
    gtk_style_context_restore(context);
    
    return surface;
}

// -- tperry 15-11-2025: New function to render button to cairo surface
cairo_surface_t*
moz_gtk_button_paint_to_surface(GdkRectangle * rect,
                                 GtkWidgetState * state,
                                 GtkReliefStyle relief,
                                 int *out_width, int *out_height)
{
    ensure_button_widget();
    GtkWidget *widget = gButtonWidget;
    
    int width = rect->width;
    int height = rect->height;
    
    if (out_width) *out_width = width;
    if (out_height) *out_height = height;
    
    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    gtk_style_context_save(context);
    
    GtkStateType button_state = ConvertGtkState(state);
    GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
    
    gboolean interior_focus;
    gint focus_width, focus_pad;
    moz_gtk_widget_get_focus(widget, &interior_focus, &focus_width, &focus_pad);
    
    // Convert state to GTK3 state flags
    if (button_state == GTK_STATE_ACTIVE)
        state_flags = GTK_STATE_FLAG_ACTIVE;
    else if (button_state == GTK_STATE_PRELIGHT)
        state_flags = GTK_STATE_FLAG_PRELIGHT;
    else if (button_state == GTK_STATE_INSENSITIVE)
        state_flags = GTK_STATE_FLAG_INSENSITIVE;
    
    if (state->isDefault)
        state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_ACTIVE);
    
    gtk_button_set_relief(GTK_BUTTON(widget), relief);
    
    if (state->focused && !state->disabled)
        state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_FOCUSED);
    
    gtk_style_context_set_state(context, state_flags);
    
    // Calculate rendering area
    gint x = 0, y = 0;
    gint render_width = width, render_height = height;
    
    if (!interior_focus && state->focused) {
        x += focus_width + focus_pad;
        y += focus_width + focus_pad;
        render_width -= 2 * (focus_width + focus_pad);
        render_height -= 2 * (focus_width + focus_pad);
    }
    
    GtkShadowType shadow_type = button_state == GTK_STATE_ACTIVE ? GTK_SHADOW_IN : GTK_SHADOW_OUT;
    
    // Create surface for rendering
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);
    
    // Clear to transparent
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    
    // Handle default button borders
    if (state->isDefault && relief == GTK_RELIEF_NORMAL) {
        gint default_top, default_left, default_bottom, default_right;
        moz_gtk_button_get_default_overflow(&default_top, &default_left,
                                            &default_bottom, &default_right);
        x -= default_left;
        y -= default_top;
        render_width += default_left + default_right;
        render_height += default_top + default_bottom;
        
        gtk_render_background(context, cr, x, y, render_width, render_height);
        gtk_render_frame(context, cr, x, y, render_width, render_height);
        
        moz_gtk_button_get_default_border(&default_top, &default_left,
                                          &default_bottom, &default_right);
        x += default_left;
        y += default_top;
        render_width -= (default_left + default_right);
        render_height -= (default_top + default_bottom);
    }
    
    // Render button background and frame
    if (relief != GTK_RELIEF_NONE ||
        (button_state != GTK_STATE_NORMAL &&
         button_state != GTK_STATE_INSENSITIVE)) {
        gtk_render_background(context, cr, x, y, render_width, render_height);
        gtk_render_frame(context, cr, x, y, render_width, render_height);
    }
    
    // Render focus indicator
    if (state->focused) {
        GtkBorder border;
        gtk_style_context_get_border(context, state_flags, &border);
        
        if (interior_focus) {
            x += border.left + focus_pad;
            y += border.top + focus_pad;
            render_width -= (border.left + border.right) + 2 * focus_pad;
            render_height -= (border.top + border.bottom) + 2 * focus_pad;
        } else {
            x -= focus_width + focus_pad;
            y -= focus_width + focus_pad;
            render_width += 2 * (focus_width + focus_pad);
            render_height += 2 * (focus_width + focus_pad);
        }
        
        gtk_render_focus(context, cr, x, y, render_width, render_height);
    }
    
    cairo_destroy(cr);
    gtk_style_context_restore(context);
    
    return surface;
}

// Build a fresh GtkStyleContext targeting the indicator CSS subnode.
//
// In GTK3's CSS node model the visible indicator is a child node:
//   checkbutton > check     (for checkboxes)
//   radiobutton > radio     (for radio buttons)
//
// gtk_widget_get_style_context(gCheckboxWidget) returns the "checkbutton"
// node.  Calling gtk_render_check() on that node produces nothing visible
// on modern themes — the check mark is owned by the "check" child node.
// We build the correct path here so gtk_render_check/option work.
//
// gtk_widget_path_iter_set_object_name() requires GTK >= 3.20 (2016),
// which is satisfied by any currently supported Ubuntu/Debian release.
static GtkStyleContext*
build_indicator_context(gboolean isradio, GtkStateFlags state_flags)
{
    GtkWidgetPath *path = gtk_widget_path_new();

    // Parent node: checkbutton / radiobutton
    gint parent = gtk_widget_path_append_type(
        path, isradio ? GTK_TYPE_RADIO_BUTTON : GTK_TYPE_CHECK_BUTTON);
    (void)parent;

    // Child indicator node: "check" or "radio"
    gint child = gtk_widget_path_append_type(
        path, isradio ? GTK_TYPE_RADIO_BUTTON : GTK_TYPE_CHECK_BUTTON);
    gtk_widget_path_iter_set_object_name(path, child, isradio ? "radio" : "check");

    GtkStyleContext *ctx = gtk_style_context_new();
    gtk_style_context_set_path(ctx, path);
    gtk_style_context_set_screen(ctx, gdk_screen_get_default());
    gtk_style_context_set_state(ctx, state_flags);
    gtk_widget_path_free(path);
    return ctx;
}

// -- tperry 15-11-2025: New function to render toggle to cairo surface and return it
// This allows capturing the rendered output without going through GdkWindow
cairo_surface_t*
moz_gtk_toggle_paint_to_surface(GdkRectangle * rect,
                                 GtkWidgetState * state,
                                 gboolean selected, gboolean isradio,
                                 int *out_width, int *out_height)
{
    gint indicator_size, indicator_spacing;

    if (isradio)
        moz_gtk_radio_get_metrics(&indicator_size, &indicator_spacing);
    else
        moz_gtk_checkbox_get_metrics(&indicator_size, &indicator_spacing);

    int width  = indicator_size;
    int height = indicator_size;

    if (out_width)  *out_width  = width;
    if (out_height) *out_height = height;

    // Compose GTK3 state flags
    GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
    if (state->active)   state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_ACTIVE);
    if (state->inHover)  state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_PRELIGHT);
    if (state->disabled) state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_INSENSITIVE);
    if (selected)        state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_CHECKED);

    // Use a style context rooted at the indicator subnode, not the button node
    GtkStyleContext *context = build_indicator_context(isradio, state_flags);

    // Create surface for rendering
    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    gtk_render_background(context, cr, 0, 0, width, height);
    gtk_render_frame(context, cr, 0, 0, width, height);
    if (isradio)
        gtk_render_option(context, cr, 0, 0, width, height);
    else
        gtk_render_check(context, cr, 0, 0, width, height);

    cairo_destroy(cr);
    g_object_unref(context);

    return surface;
}

static gint
moz_gtk_toggle_paint(GdkWindow * drawable, GdkRectangle * rect,
                     GdkRectangle * cliprect, GtkWidgetState * state,
                     gboolean selected, gboolean isradio)
{
    gint indicator_size, indicator_spacing;

    if (isradio)
        moz_gtk_radio_get_metrics(&indicator_size, &indicator_spacing);
    else
        moz_gtk_checkbox_get_metrics(&indicator_size, &indicator_spacing);

    gint x      = rect->x + indicator_spacing;
    gint y      = rect->y + (rect->height - indicator_size) / 2;
    gint width  = indicator_size;
    gint height = indicator_size;

    GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
    if (state->active)   state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_ACTIVE);
    if (state->inHover)  state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_PRELIGHT);
    if (state->disabled) state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_INSENSITIVE);
    if (selected)        state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_CHECKED);

    GtkStyleContext *context = build_indicator_context(isradio, state_flags);

    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *offscreen_cr = cairo_create(surface);

    cairo_set_operator(offscreen_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(offscreen_cr);
    cairo_set_operator(offscreen_cr, CAIRO_OPERATOR_OVER);

    gtk_render_background(context, offscreen_cr, 0, 0, width, height);
    gtk_render_frame(context, offscreen_cr, 0, 0, width, height);
    if (isradio)
        gtk_render_option(context, offscreen_cr, 0, 0, width, height);
    else
        gtk_render_check(context, offscreen_cr, 0, 0, width, height);

    cairo_t *target_cr = moz_gdk_create_cairo_context(drawable);
    cairo_set_source_surface(target_cr, surface, x, y);
    cairo_paint(target_cr);

    cairo_destroy(offscreen_cr);
    cairo_surface_destroy(surface);
    cairo_destroy(target_cr);
    g_object_unref(context);

    return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - GtkMisc removed, use margin and alignment properties
static gint
calculate_arrow_dimensions(GdkRectangle * rect, GdkRectangle * arrow_rect)
{
	// Get margin and alignment from widget properties
	gint xpad = gtk_widget_get_margin_start(gArrowWidget) + gtk_widget_get_margin_end(gArrowWidget);
	gint ypad = gtk_widget_get_margin_top(gArrowWidget) + gtk_widget_get_margin_bottom(gArrowWidget);
	
	// GTK3: alignment is typically 0.5 (centered) for arrows
	gfloat xalign = 0.5;
	gfloat yalign = 0.5;

	gint extent = MIN(rect->width - xpad * 2,
	                  rect->height - ypad * 2);

	arrow_rect->x = (gint)(
	                    (rect->x + xpad) *
	                    (1.0 - xalign) +
	                    (rect->x + rect->width - extent - xpad) *
	                    xalign);

	arrow_rect->y = (gint)(
	                    (rect->y + ypad) *
	                    (1.0 - yalign) +
	                    (rect->y + rect->height - extent - ypad) *
	                    yalign);

	arrow_rect->width = arrow_rect->height = extent;

	return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_scrollbar_button_paint(GdkWindow * drawable, GdkRectangle * rect,
                               GdkRectangle * cliprect,
                               GtkWidgetState * state, GtkArrowType type)
{
	GdkRectangle button_rect;
	GdkRectangle arrow_rect;
	GtkWidget *scrollbar;

	ensure_scrollbar_widget();

	if (type < 2)
		scrollbar = gVertScrollbarWidget;
	else
		scrollbar = gHorizScrollbarWidget;

	ensure_arrow_widget();

	GtkStyleContext *context = gtk_widget_get_style_context(scrollbar);
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	if (cliprect) {
		// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
		// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	}
	
	// Convert state
	GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
	if (state->disabled)
		state_flags = GTK_STATE_FLAG_INSENSITIVE;
	else if (state->active)
		state_flags = GTK_STATE_FLAG_ACTIVE;
	else if (state->inHover)
		state_flags = GTK_STATE_FLAG_PRELIGHT;
	
	gtk_style_context_set_state(context, state_flags);
	
	// Render trough background
	gtk_render_background(context, cr, rect->x, rect->y, rect->width, rect->height);
	gtk_render_frame(context, cr, rect->x, rect->y, rect->width, rect->height);

	calculate_arrow_dimensions(rect, &button_rect);

	// Render button
	gtk_render_background(context, cr, button_rect.x, button_rect.y, 
	                      button_rect.width, button_rect.height);
	gtk_render_frame(context, cr, button_rect.x, button_rect.y,
	                 button_rect.width, button_rect.height);

	// Calculate arrow position
	arrow_rect.width = button_rect.width / 2;
	arrow_rect.height = button_rect.height / 2;
	arrow_rect.x = button_rect.x + (button_rect.width - arrow_rect.width) / 2;
	arrow_rect.y = button_rect.y + (button_rect.height - arrow_rect.height) / 2;

	// Render arrow
	gtk_render_arrow(context, cr, 
	                 type == GTK_ARROW_UP ? 0 : 
	                 type == GTK_ARROW_DOWN ? G_PI :
	                 type == GTK_ARROW_LEFT ? -G_PI/2 : G_PI/2,
	                 arrow_rect.x, arrow_rect.y,
	                 MIN(arrow_rect.width, arrow_rect.height));

	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}


// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_scrollbar_trough_paint(GtkThemeWidgetType widget,
                               GdkWindow * drawable, GdkRectangle * rect,
                               GdkRectangle * cliprect,
                               GtkWidgetState * state)
{
	GtkWidget *scrollbar;

	ensure_scrollbar_widget();

	if (widget == MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL)
		scrollbar = gHorizScrollbarWidget;
	else
		scrollbar = gVertScrollbarWidget;

	GtkStyleContext *context = gtk_widget_get_style_context(scrollbar);
	
	// GTK3: Save context and add proper CSS classes
	gtk_style_context_save(context);
	gtk_style_context_add_class(context, GTK_STYLE_CLASS_TROUGH);
	
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	// -- tperry 15-11-2025: WORKAROUND - System Cairo 1.16.0 crashes in cairo_clip()
	// Skip clipping entirely - it causes crashes with system Cairo
	if (cliprect) {
		// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
		// cairo_clip(cr);
	}
	
	// Set state
	GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
	gtk_style_context_set_state(context, state_flags);
	
	// Render trough
	gtk_render_background(context, cr, rect->x, rect->y, rect->width, rect->height);
	gtk_render_frame(context, cr, rect->x, rect->y, rect->width, rect->height);

	// Render focus if needed
	if (state->focused)
	{
		gtk_render_focus(context, cr, rect->x, rect->y, rect->width, rect->height);
	}

	cairo_destroy(cr);
	
	// GTK3: Restore context
	gtk_style_context_restore(context);
	
	return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_scrollbar_thumb_paint(GtkThemeWidgetType widget,
                              GdkWindow * drawable, GdkRectangle * rect,
                              GdkRectangle * cliprect, GtkWidgetState * state)
{
	GtkWidget *scrollbar;
	GtkAdjustment *adj;

	ensure_scrollbar_widget();

	if (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL)
		scrollbar = gHorizScrollbarWidget;
	else
		scrollbar = gVertScrollbarWidget;

	adj = gtk_range_get_adjustment(GTK_RANGE(scrollbar));

	int thumbborder = 1;

	// Adjust rectangle for thumb border
	if (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL)
	{
		rect->x += thumbborder;
		rect->width -= thumbborder * 2;
		gtk_adjustment_set_page_size(adj, rect->width - 2);
	}
	else
	{
		rect->y += thumbborder;
		rect->height -= thumbborder * 2;
		gtk_adjustment_set_page_size(adj, rect->height - 2);
	}

	// Set adjustment values
	gtk_adjustment_set_lower(adj, 0);
	gtk_adjustment_set_value(adj, state->curpos);
	gtk_adjustment_set_upper(adj, state->maxpos);

	GtkStyleContext *context = gtk_widget_get_style_context(scrollbar);
	
	// GTK3: Save context and add proper CSS classes
	gtk_style_context_save(context);
	gtk_style_context_add_class(context, GTK_STYLE_CLASS_SLIDER);
	
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	if (cliprect) {
		// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
		// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	}
	
	// Set state
	GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
	if (state->inHover || state->active)
		state_flags = GTK_STATE_FLAG_PRELIGHT;
	
	gtk_style_context_set_state(context, state_flags);
	
	// Render slider/thumb
	gtk_render_slider(context, cr, rect->x, rect->y, rect->width, rect->height,
	                  (widget == MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL) ?
	                  GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);

	cairo_destroy(cr);
	
	// GTK3: Restore context
	gtk_style_context_restore(context);
	
	return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_gripper_paint(GdkWindow * drawable, GdkRectangle * rect,
                      GdkRectangle * cliprect, GtkWidgetState * state)
{
	ensure_handlebox_widget();
	
	GtkStyleContext *context = gtk_widget_get_style_context(gHandleBoxWidget);
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	if (cliprect) {
		// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
		// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	}
	
	// Convert state
	GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
	if (state->disabled)
		state_flags = GTK_STATE_FLAG_INSENSITIVE;
	else if (state->active)
		state_flags = GTK_STATE_FLAG_ACTIVE;
	else if (state->inHover)
		state_flags = GTK_STATE_FLAG_PRELIGHT;
	
	gtk_style_context_set_state(context, state_flags);
	
	// Render handlebox
	gtk_render_background(context, cr, rect->x, rect->y, rect->width, rect->height);
	gtk_render_frame(context, cr, rect->x, rect->y, rect->width, rect->height);

	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_entry_frame_paint(GdkWindow * drawable, GdkRectangle * rect,
                          GdkRectangle * cliprect, GtkWidgetState * state)
{
    gint x, y, width = rect->width, height = rect->height;
    gboolean interior_focus;
    gint focus_width;

	GtkWidget *widget;
	ensure_entry_widget();
	widget = gEntryWidget;

    gtk_widget_style_get(widget,
                         "interior-focus", &interior_focus,
                         "focus-line-width", &focus_width,
                         NULL);

    gtk_widget_set_sensitive(widget, !state->disabled);

    /* Get the position of the inner window, see _gtk_entry_get_borders */
    x = XTHICKNESS(widget);
    y = YTHICKNESS(widget);

    if (!interior_focus) {
        x += focus_width;
        y += focus_width;
    }

    /* Now paint the shadow and focus border */
    x = rect->x;
    y = rect->y;

    if (state->focused && !state->disabled) {
        if (!interior_focus) {
            /* Indent the border a little bit if we have exterior focus */
            x += focus_width;
            y += focus_width;
            width -= 2 * focus_width;
            height -= 2 * focus_width;
        }
    }

    GtkStyleContext *context = gtk_widget_get_style_context(widget);
    cairo_t *cr = moz_gdk_create_cairo_context(drawable);
    
    // Set clip region
    if (cliprect) {
        // cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
        // cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
    }
    
    // Set state flags
    GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
    if (state->disabled)
        state_flags = GTK_STATE_FLAG_INSENSITIVE;
    if (state->focused && !state->disabled)
        state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_FOCUSED);
    
    gtk_style_context_set_state(context, state_flags);
    
    // Render entry frame
    gtk_render_background(context, cr, x, y, width, height);
    gtk_render_frame(context, cr, x, y, width, height);

    // Render focus if needed
    if (state->focused && !state->disabled && !interior_focus) {
        gtk_render_focus(context, cr, rect->x, rect->y, rect->width, rect->height);
    }

    cairo_destroy(cr);
    return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_entry_paint(GdkWindow * drawable, GdkRectangle * rect,
                    GdkRectangle * cliprect, GtkWidgetState * state)
{
	gint x, y;

	ensure_entry_widget();

	moz_gtk_generic_container_paint(drawable, rect, cliprect, state,
	                                gEntryWidget, "viewportbin");

	/* paint the background first */
	x = XTHICKNESS(gEntryWidget);
	y = YTHICKNESS(gEntryWidget);

	GtkStyleContext *context = gtk_widget_get_style_context(gEntryWidget);
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	if (cliprect) {
		// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
		// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	}
	
	gtk_style_context_set_state(context, GTK_STATE_FLAG_NORMAL);
	
	// Render entry background
	gtk_render_background(context, cr, rect->x + x, rect->y + y,
	                      rect->width - 2 * x, rect->height - 2 * y);

	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_dropdown_arrow_paint(GdkWindow * drawable, GdkRectangle * rect,
                             GdkRectangle * cliprect, GtkWidgetState * state)
{
	GdkRectangle arrow_rect, real_arrow_rect;

	ensure_arrow_widget();
	moz_gtk_button_paint(drawable, rect, cliprect, state,
	                     GTK_RELIEF_NORMAL, gDropdownButtonWidget);

	/* This mirrors gtkbutton's child positioning */
	arrow_rect.x = rect->x + 1 + XTHICKNESS(gDropdownButtonWidget);
	arrow_rect.y = rect->y + 1 + YTHICKNESS(gDropdownButtonWidget);
	arrow_rect.width = MAX(1, rect->width - (arrow_rect.x - rect->x) * 2);
	arrow_rect.height = MAX(1, rect->height - (arrow_rect.y - rect->y) * 2);

	calculate_arrow_dimensions(&arrow_rect, &real_arrow_rect);

	real_arrow_rect.width = real_arrow_rect.height = (int)
	                        (MIN(real_arrow_rect.width, real_arrow_rect.height) * 0.9);

	real_arrow_rect.x = (gint)
	                    floor(arrow_rect.x +
	                          ((arrow_rect.width - real_arrow_rect.width) / 2) + 0.5);
	real_arrow_rect.y = (gint)
	                    floor(arrow_rect.y +
	                          ((arrow_rect.height - real_arrow_rect.height) / 2) +
	                          0.5);

	GtkStyleContext *context = gtk_widget_get_style_context(gArrowWidget);
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	if (cliprect) {
		// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
		// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	}
	
	// Convert state
	GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
	if (state->disabled)
		state_flags = GTK_STATE_FLAG_INSENSITIVE;
	else if (state->active)
		state_flags = GTK_STATE_FLAG_ACTIVE;
	else if (state->inHover)
		state_flags = GTK_STATE_FLAG_PRELIGHT;
	
	gtk_style_context_set_state(context, state_flags);
	
	// Render arrow pointing down
	gtk_render_arrow(context, cr, G_PI, // Down arrow
	                 real_arrow_rect.x, real_arrow_rect.y,
	                 MIN(real_arrow_rect.width, real_arrow_rect.height));

	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_container_paint(GdkWindow * drawable, GdkRectangle * rect,
                        GdkRectangle * cliprect, GtkWidgetState * state,
                        gboolean isradio)
{
	GtkWidget *widget;

	if (isradio)
	{
		ensure_radiobutton_widget();
		widget = gRadiobuttonWidget;
	}
	else
	{
		ensure_checkbox_widget();
		widget = gCheckboxWidget;
	}

	GtkStyleContext *context = gtk_widget_get_style_context(widget);
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	if (cliprect) {
		// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
		// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	}
	
	// Convert state - only use NORMAL or PRELIGHT
	GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
	if (state->inHover && !state->disabled)
		state_flags = GTK_STATE_FLAG_PRELIGHT;
	
	gtk_style_context_set_state(context, state_flags);
	
	// Render background
	gtk_render_background(context, cr, rect->x, rect->y, rect->width, rect->height);

	// Render focus if needed
	if (state->focused)
	{
		gtk_render_focus(context, cr, rect->x, rect->y, rect->width, rect->height);
	}

	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_toolbar_paint(GdkWindow * drawable, GdkRectangle * rect,
                      GdkRectangle * cliprect)
{
	ensure_handlebox_widget();

	GtkStyleContext *context = gtk_widget_get_style_context(gHandleBoxWidget);
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	if (cliprect) {
		// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
		// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	}
	
	gtk_style_context_set_state(context, GTK_STATE_FLAG_NORMAL);
	
	// Render toolbar background and frame
	gtk_render_background(context, cr, rect->x, rect->y, rect->width, rect->height);
	gtk_render_frame(context, cr, rect->x, rect->y, rect->width, rect->height);

	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_tooltip_paint(GdkWindow * drawable, GdkRectangle * rect,
                      GdkRectangle * cliprect)
{
	ensure_tooltip_widget();
	
	GtkStyleContext *context = gtk_widget_get_style_context(gTooltipWindow);
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
	// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	
	// Render tooltip background
	gtk_render_background(context, cr, rect->x, rect->y, rect->width, rect->height);
	gtk_render_frame(context, cr, rect->x, rect->y, rect->width, rect->height);
	
	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_frame_paint(GdkWindow * drawable, GdkRectangle * rect,
                    GdkRectangle * cliprect)
{
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);

	ensure_frame_widget();

	// Render base background first
	GtkStyleContext *context = gtk_widget_get_style_context(gFrameWidget);
	gtk_style_context_set_state(context, GTK_STATE_FLAG_NORMAL);
	gtk_render_background(context, cr, rect->x, rect->y, rect->width, rect->height);
	
	// Set clip region for frame
	if (cliprect) {
		// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
		// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	}
	
	// Render frame
	context = gtk_widget_get_style_context(gFrameWidget);
	gtk_style_context_set_state(context, GTK_STATE_FLAG_NORMAL);
	gtk_render_frame(context, cr, rect->x, rect->y, rect->width, rect->height);

	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_listbox_paint(GdkWindow * drawable, GdkRectangle * rect,
                      GdkRectangle * cliprect)
{
	int xw, yw;
	ensure_entry_widget();

	moz_gtk_get_widget_border(MOZ_GTK_FRAME, &xw, &yw);

	moz_gtk_frame_paint(drawable, rect, cliprect);

	rect->x += xw;
	rect->y += yw;
	rect->width -= (xw * 2);
	rect->height -= (yw * 2);

	// Render listbox interior background using theme's base color
	GtkStyleContext *context = gtk_widget_get_style_context(gEntryWidget);
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	gtk_style_context_set_state(context, GTK_STATE_FLAG_NORMAL);
	gtk_render_background(context, cr, rect->x, rect->y, rect->width, rect->height);
	
	cairo_destroy(cr);

	return MOZ_GTK_SUCCESS;
}

void spinbutton_get_rects(GtkArrowType type, GdkRectangle *rect,
                          GdkRectangle &buttonrect, GdkRectangle &arrowrect)
{
	gint arrow_size;
	int x, y, width, height;
	int h, w;

	ensure_spinbutton_widget();

	// -- tperry 13-11-2025: GTK3 - widget->style removed, pass widget directly
	arrow_size = rect->width - (2 * XTHICKNESS(gSpinbuttonWidget));

	width = arrow_size + 2 * XTHICKNESS(gSpinbuttonWidget);

	if(type == GTK_ARROW_UP)
	{
		x = rect->x;
		y = rect->y;
		height = (int)floor(rect->height / 2);

	}
	else
	{
		x = rect->x;
		y = rect->y + (int)floor(rect->height / 2);
		height = (rect->height + 1) / 2;
	}

	buttonrect.x = x;
	buttonrect.y = y;
	buttonrect.width = width;
	buttonrect.height = height;


	////
	height = rect->height;

	if (type == GTK_ARROW_DOWN)
	{
		y = height / 2;
		height = height - y - 2;
	}
	else
	{
		y = 2;
		height = height / 2 - 2;
	}

	width -= 3;

	x = 1;

	w = width / 2;
	w -= w % 2 - 1; /* force odd */
	h = (w + 1) / 2;

	x += (width - w) / 2;
	y += (height - h) / 2;

	arrowrect.x = x;
	arrowrect.y = y;
	arrowrect.width = w;
	arrowrect.height = h;
}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static void
spinbutton_arrow_paint(GtkArrowType type, GdkRectangle *rect, GdkWindow *d,
                       GtkStateType state_type, GtkShadowType shadow_type)
{
	GdkRectangle buttonrect, arrowrect;

	spinbutton_get_rects(type, rect, buttonrect, arrowrect);

	GtkStyleContext *context = gtk_widget_get_style_context(gSpinbuttonWidget);
	cairo_t *cr = moz_gdk_create_cairo_context(d);
	
	// Convert state
	GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
	if (state_type == GTK_STATE_ACTIVE)
		state_flags = GTK_STATE_FLAG_ACTIVE;
	else if (state_type == GTK_STATE_PRELIGHT)
		state_flags = GTK_STATE_FLAG_PRELIGHT;
	else if (state_type == GTK_STATE_INSENSITIVE)
		state_flags = GTK_STATE_FLAG_INSENSITIVE;
	
	gtk_style_context_set_state(context, state_flags);
	
	// Render button background
	gtk_render_background(context, cr, buttonrect.x, buttonrect.y, 
	                      buttonrect.width, buttonrect.height);
	gtk_render_frame(context, cr, buttonrect.x, buttonrect.y,
	                 buttonrect.width, buttonrect.height);

	// Render arrow
	gtk_render_arrow(context, cr,
	                 type == GTK_ARROW_UP ? 0 : G_PI,
	                 arrowrect.x, arrowrect.y,
	                 MIN(arrowrect.width, arrowrect.height));
	
	cairo_destroy(cr);


}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_scale_track_paint(GtkThemeWidgetType type,
                          GdkWindow * drawable,
                          GdkRectangle * rect,
                          GdkRectangle * cliprect,
                          gint flags)
{
	ensure_scale_widget();

	GtkWidget *scale;

	if (type == MOZ_GTK_SCALE_TRACK_HORIZONTAL)
		scale = gHScaleWidget;
	else
		scale = gVScaleWidget;

    // AL-2014-01-16: [[ Bug 11656 ]] Don't paint box around slider trough.
	// moz_gtk_label_paint(drawable, rect, cliprect);

	// Set widget size allocation
	GtkAllocation allocation;
	allocation.x = rect->x;
	allocation.y = rect->y;
	allocation.width = rect->width;
	allocation.height = rect->height;
	gtk_widget_size_allocate(scale, &allocation);

	GtkStyleContext *context = gtk_widget_get_style_context(scale);
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	if (cliprect) {
		// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
		// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	}
	
	// Add trough CSS class for proper theming
	gtk_style_context_save(context);
	gtk_style_context_add_class(context, GTK_STYLE_CLASS_TROUGH);
	gtk_style_context_set_state(context, GTK_STATE_FLAG_NORMAL);
	
	// Render scale trough
	gtk_render_background(context, cr, rect->x, rect->y, rect->width, rect->height);
	gtk_render_frame(context, cr, rect->x, rect->y, rect->width, rect->height);
	
	gtk_style_context_restore(context);

	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}

// -- tperry 16-11-2025: New function to render scale thumb (slider knob)
// Uses manual gtk_render_slider since gtk_widget_draw doesn't work for scales
static gint
moz_gtk_scale_thumb_paint(GtkThemeWidgetType type,
                          GdkWindow * drawable,
                          GdkRectangle * rect,
                          GdkRectangle * cliprect,
                          GtkWidgetState * state)
{
	ensure_scale_widget();
	
	GtkWidget *scale;
	GtkOrientation orientation;
	
	// Set widget size allocation
	GtkAllocation allocation;
	allocation.x = rect->x;
	allocation.y = rect->y;
	allocation.width = rect->width;
	allocation.height = rect->height;
	
	if (type == MOZ_GTK_SCALE_THUMB_HORIZONTAL) {
		scale = gHScaleWidget;
		orientation = GTK_ORIENTATION_HORIZONTAL;
	} else {
		scale = gVScaleWidget;
		orientation = GTK_ORIENTATION_VERTICAL;
	}
	
	gtk_widget_size_allocate(scale, &allocation);
	
	GtkStyleContext *context = gtk_widget_get_style_context(scale);
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set state
	GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
	if (state->disabled)
		state_flags = GTK_STATE_FLAG_INSENSITIVE;
	else if (state->active)
		state_flags = GTK_STATE_FLAG_ACTIVE;
	else if (state->inHover)
		state_flags = GTK_STATE_FLAG_PRELIGHT;
	
	gtk_style_context_save(context);
	gtk_style_context_add_class(context, GTK_STYLE_CLASS_SLIDER);
	gtk_style_context_set_state(context, state_flags);
	
	// Render slider (thumb/knob)
	gtk_render_slider(context, cr,
	                  rect->x, rect->y, rect->width, rect->height,
	                  orientation);
	
	gtk_style_context_restore(context);
	cairo_destroy(cr);
	
	return MOZ_GTK_SUCCESS;
}

// -- direct-render path (used by drawtheme_gtk3_direct, avoids the slow
// dual-offscreen alpha-extraction loop in drawtheme_calc_alpha).
// Renders the menu item in PRELIGHT state and falls back to named theme
// colours if the render produces a fully-transparent surface.
cairo_surface_t*
moz_gtk_menuitem_paint_to_surface(GdkRectangle *rect, int *out_width, int *out_height)
{
	ensure_menuitem_widget();

	int width  = rect->width;
	int height = rect->height;
	if (out_width)  *out_width  = width;
	if (out_height) *out_height = height;

	cairo_surface_t *surface =
	    cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cr = cairo_create(surface);

	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

	GtkStyleContext *context = gtk_widget_get_style_context(gMenuitemWidget);
	gtk_style_context_save(context);
	gtk_style_context_set_state(context, GTK_STATE_FLAG_PRELIGHT);
	gtk_render_background(context, cr, 0, 0, width, height);
	gtk_render_frame(context, cr, 0, 0, width, height);
	gtk_style_context_restore(context);
	cairo_destroy(cr);

	// If the render produced nothing (fully transparent centre pixel),
	// fall back to named theme selection colours.
	cairo_surface_flush(surface);
	unsigned char *data   = cairo_image_surface_get_data(surface);
	int            stride = cairo_image_surface_get_stride(surface);
	int            cx     = width / 2, cy = height / 2;
	unsigned char *px     = data + cy * stride + cx * 4;
	if (px[3] == 0)
	{
		ensure_label_widget();
		GtkStyleContext *lctx = gtk_widget_get_style_context(gLabelWidget);
		GdkRGBA rgba = {0.25, 0.55, 0.85, 1.0}; // safe default
		if (!gtk_style_context_lookup_color(lctx, "theme_selected_bg_color", &rgba))
		if (!gtk_style_context_lookup_color(lctx, "accent_bg_color",          &rgba))
		    gtk_style_context_lookup_color(lctx, "selected_bg_color",         &rgba);
		cr = cairo_create(surface);
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
		cairo_set_source_rgba(cr, rgba.red, rgba.green, rgba.blue, 1.0);
		cairo_paint(cr);
		cairo_destroy(cr);
	}

	return surface;
}

// ---------------------------------------------------------------------------
// Surface-rendering variants for widgets previously handled by the slow
// drawtheme_calc_alpha (dual-offscreen-window) path.  These let
// drawtheme_gtk3_direct render directly to an ARGB32 cairo surface,
// avoiding two GtkOffscreenWindow create/realize/destroy cycles per call.
// ---------------------------------------------------------------------------

// Helper: allocate a transparent ARGB32 surface and return both surface and cr.
static cairo_surface_t*
make_transparent_surface(int width, int height, cairo_t **cr_out)
{
	cairo_surface_t *s = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cr = cairo_create(s);
	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	*cr_out = cr;
	return s;
}

// MOZ_GTK_TOOLBAR — menubar / toolbar background
cairo_surface_t*
moz_gtk_toolbar_paint_to_surface(GdkRectangle *rect, GtkWidgetState *state,
                                  int *out_width, int *out_height)
{
	ensure_handlebox_widget();
	int width = rect->width, height = rect->height;
	if (out_width)  *out_width  = width;
	if (out_height) *out_height = height;

	cairo_t *cr;
	cairo_surface_t *surface = make_transparent_surface(width, height, &cr);

	GtkStyleContext *context = gtk_widget_get_style_context(gHandleBoxWidget);
	gtk_style_context_save(context);

	GtkStateFlags sf = GTK_STATE_FLAG_NORMAL;
	if (state && state->disabled)  sf = GTK_STATE_FLAG_INSENSITIVE;
	else if (state && state->active)  sf = (GtkStateFlags)(sf | GTK_STATE_FLAG_ACTIVE);
	else if (state && state->inHover) sf = (GtkStateFlags)(sf | GTK_STATE_FLAG_PRELIGHT);
	gtk_style_context_set_state(context, sf);

	gtk_render_background(context, cr, 0, 0, width, height);
	gtk_render_frame(context, cr, 0, 0, width, height);

	gtk_style_context_restore(context);
	cairo_destroy(cr);
	return surface;
}

// MOZ_GTK_FRAME — status-bar panels
cairo_surface_t*
moz_gtk_frame_paint_to_surface(GdkRectangle *rect, int *out_width, int *out_height)
{
	ensure_frame_widget();
	int width = rect->width, height = rect->height;
	if (out_width)  *out_width  = width;
	if (out_height) *out_height = height;

	cairo_t *cr;
	cairo_surface_t *surface = make_transparent_surface(width, height, &cr);

	GtkStyleContext *context = gtk_widget_get_style_context(gFrameWidget);
	gtk_style_context_save(context);
	gtk_style_context_set_state(context, GTK_STATE_FLAG_NORMAL);

	gtk_render_background(context, cr, 0, 0, width, height);
	gtk_render_frame(context, cr, 0, 0, width, height);

	gtk_style_context_restore(context);
	cairo_destroy(cr);
	return surface;
}

// MOZ_GTK_TOOLTIP
cairo_surface_t*
moz_gtk_tooltip_paint_to_surface(GdkRectangle *rect, int *out_width, int *out_height)
{
	ensure_tooltip_widget();
	int width = rect->width, height = rect->height;
	if (out_width)  *out_width  = width;
	if (out_height) *out_height = height;

	cairo_t *cr;
	cairo_surface_t *surface = make_transparent_surface(width, height, &cr);

	GtkStyleContext *context = gtk_widget_get_style_context(gTooltipWindow);
	gtk_render_background(context, cr, 0, 0, width, height);
	gtk_render_frame(context, cr, 0, 0, width, height);

	cairo_destroy(cr);
	return surface;
}

// MOZ_GTK_ENTRY_FRAME — text-field border / focus ring
cairo_surface_t*
moz_gtk_entry_frame_paint_to_surface(GdkRectangle *rect, GtkWidgetState *state,
                                      int *out_width, int *out_height)
{
	ensure_entry_widget();
	int width = rect->width, height = rect->height;
	if (out_width)  *out_width  = width;
	if (out_height) *out_height = height;

	gboolean interior_focus = TRUE;
	gint focus_width = 1;
	gtk_widget_style_get(gEntryWidget,
	                     "interior-focus", &interior_focus,
	                     "focus-line-width", &focus_width,
	                     NULL);
	gtk_widget_set_sensitive(gEntryWidget, state ? !state->disabled : TRUE);

	GtkStyleContext *context = gtk_widget_get_style_context(gEntryWidget);
	gtk_style_context_save(context);

	GtkStateFlags sf = GTK_STATE_FLAG_NORMAL;
	if (state && state->disabled)
		sf = GTK_STATE_FLAG_INSENSITIVE;
	if (state && state->focused && !state->disabled)
		sf = (GtkStateFlags)(sf | GTK_STATE_FLAG_FOCUSED);
	gtk_style_context_set_state(context, sf);

	cairo_t *cr;
	cairo_surface_t *surface = make_transparent_surface(width, height, &cr);

	int x = 0, y = 0, w = width, h = height;
	if (state && state->focused && !state->disabled && !interior_focus) {
		x += focus_width; y += focus_width;
		w -= 2 * focus_width; h -= 2 * focus_width;
	}

	gtk_render_background(context, cr, x, y, w, h);
	gtk_render_frame(context, cr, x, y, w, h);

	if (state && state->focused && !state->disabled && !interior_focus)
		gtk_render_focus(context, cr, 0, 0, width, height);

	gtk_style_context_restore(context);
	cairo_destroy(cr);
	return surface;
}

// MOZ_GTK_ENTRY — text-field interior background
cairo_surface_t*
moz_gtk_entry_paint_to_surface(GdkRectangle *rect, GtkWidgetState *state,
                                int *out_width, int *out_height)
{
	ensure_entry_widget();
	int width = rect->width, height = rect->height;
	if (out_width)  *out_width  = width;
	if (out_height) *out_height = height;

	int x = XTHICKNESS(gEntryWidget);
	int y = YTHICKNESS(gEntryWidget);

	GtkStyleContext *context = gtk_widget_get_style_context(gEntryWidget);
	gtk_style_context_save(context);
	gtk_style_context_set_state(context, GTK_STATE_FLAG_NORMAL);

	cairo_t *cr;
	cairo_surface_t *surface = make_transparent_surface(width, height, &cr);

	gtk_render_background(context, cr, x, y, width - 2 * x, height - 2 * y);

	gtk_style_context_restore(context);
	cairo_destroy(cr);
	return surface;
}

// ---------------------------------------------------------------------------
// Legacy GdkWindow paint shim — still called by the old drawtheme_calc_alpha
// path for any widget type that reaches it; kept for completeness.
static gint
moz_gtk_menuitem_paint(GdkWindow * drawable, GdkRectangle * rect,
                       GdkRectangle * cliprect)
{
	int w, h;
	cairo_surface_t *surface = moz_gtk_menuitem_paint_to_surface(rect, &w, &h);
	if (!surface) return MOZ_GTK_UNKNOWN_WIDGET;

	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	cairo_set_source_surface(cr, surface, rect->x, rect->y);
	cairo_paint(cr);
	cairo_destroy(cr);
	cairo_surface_destroy(surface);
	return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_spinbutton_paint(GdkWindow * drawable, GdkRectangle * rect,
                         GdkRectangle * cliprect, GtkWidgetState *state,
                         gint flags)
{
	ensure_spinbutton_widget();

	GtkShadowType shadow_type;
	int xw, yw;

	gtk_widget_style_get (GTK_WIDGET (gSpinbuttonWidget),
	                        "shadow_type", &shadow_type, NULL);

	moz_gtk_get_widget_border(MOZ_GTK_FRAME, &xw, &yw);

	// Render spinbutton box if shadow is not NONE
	if(shadow_type != GTK_SHADOW_NONE)
	{
		GtkStyleContext *context = gtk_widget_get_style_context(gSpinbuttonWidget);
		cairo_t *cr = moz_gdk_create_cairo_context(drawable);
		
		gtk_style_context_set_state(context, GTK_STATE_FLAG_NORMAL);
		gtk_render_background(context, cr, rect->x, rect->y, rect->width, rect->height);
		gtk_render_frame(context, cr, rect->x, rect->y, rect->width, rect->height);
		
		cairo_destroy(cr);
	}

	shadow_type = GTK_SHADOW_OUT;
	GtkStateType state_type = GTK_STATE_NORMAL;

	if(state->active)
	{
		shadow_type = GTK_SHADOW_IN;
	}

	// Paint up arrow
	if(flags == GTK_POS_TOP)
	{
		state_type = ConvertGtkState(state);
		spinbutton_arrow_paint(GTK_ARROW_UP, rect, drawable,
		                       state_type, shadow_type);
	}
	else
		spinbutton_arrow_paint(GTK_ARROW_UP, rect, drawable,
		                       GTK_STATE_NORMAL, GTK_SHADOW_OUT);

	// Paint down arrow
	if(flags == GTK_POS_BOTTOM)
	{
		state_type = ConvertGtkState(state);
		spinbutton_arrow_paint(GTK_ARROW_DOWN, rect, drawable,
		                       state_type, shadow_type);
	}
	else
		spinbutton_arrow_paint(GTK_ARROW_DOWN, rect, drawable,
		                       GTK_STATE_NORMAL, GTK_SHADOW_OUT);

	return MOZ_GTK_SUCCESS;
}


// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_progressbar_paint(GdkWindow * drawable, GdkRectangle * rect,
                          GdkRectangle * cliprect)
{
	ensure_progress_widget();

	GtkStyleContext *context = gtk_widget_get_style_context(gProgressWidget);
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	if (cliprect) {
		// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
		// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	}
	
	gtk_style_context_set_state(context, GTK_STATE_FLAG_NORMAL);
	
	// Render progressbar trough
	gtk_render_background(context, cr, rect->x, rect->y, rect->width, rect->height);
	gtk_render_frame(context, cr, rect->x, rect->y, rect->width, rect->height);

	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_progress_chunk_paint(GdkWindow * drawable, GdkRectangle * rect,
                             GdkRectangle * cliprect, gint flags)
{
	ensure_progress_widget();

	// -- tperry 13-11-2025: GTK3 - GtkProgressBarOrientation removed, use GtkOrientation and inverted property
	GtkOrientation orientation = GTK_ORIENTATION_HORIZONTAL;
	gboolean inverted = FALSE;

	if(flags & GTK_PROGRESS_TOP_TO_BOTTOM) {
		orientation = GTK_ORIENTATION_VERTICAL;
		inverted = FALSE;
	}
	else if(flags & GTK_PROGRESS_BOTTOM_TO_TOP) {
		orientation = GTK_ORIENTATION_VERTICAL;
		inverted = TRUE;
	}
	else if(flags & GTK_PROGRESS_RIGHT_TO_LEFT) {
		orientation = GTK_ORIENTATION_HORIZONTAL;
		inverted = TRUE;
	}
	else {
		orientation = GTK_ORIENTATION_HORIZONTAL;
		inverted = FALSE;
	}

	gtk_orientable_set_orientation(GTK_ORIENTABLE(gProgressWidget), orientation);
	gtk_progress_bar_set_inverted(GTK_PROGRESS_BAR(gProgressWidget), inverted);

	int border = XTHICKNESS(gProgressWidget);

	GtkStyleContext *context = gtk_widget_get_style_context(gProgressWidget);
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	if (cliprect) {
		// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
		// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	}
	
	// Render trough background
	gtk_style_context_set_state(context, GTK_STATE_FLAG_NORMAL);
	gtk_render_background(context, cr, rect->x - border, rect->y - border,
	                      rect->width + border, rect->height + border);
	gtk_render_frame(context, cr, rect->x - border, rect->y - border,
	                 rect->width + border, rect->height + border);

	// Render progress bar
	gtk_style_context_set_state(context, GTK_STATE_FLAG_PRELIGHT);
	gtk_render_activity(context, cr, rect->x, rect->y, rect->width, rect->height);

	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
static gint
moz_gtk_label_paint(GdkWindow * drawable, GdkRectangle * rect,
                    GdkRectangle * cliprect)
{
	ensure_label_widget();

	GtkStyleContext *context = gtk_widget_get_style_context(gLabelWidget);
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	if (cliprect) {
		// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
		// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	}
	
	gtk_style_context_set_state(context, GTK_STATE_FLAG_NORMAL);
	
	// Render label background
	gtk_render_background(context, cr, rect->x, rect->y, rect->width, rect->height);

	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}

// -- tperry 13-11-2025: GTK3 - GtkOptionMenu removed, this function is no longer needed
// GtkComboBox doesn't use the same properties structure
/*
static void
gtk_option_menu_get_props (GtkOptionMenu       *option_menu,
                           GtkOptionMenuProps  *props)
{
	// This function is not needed for GtkComboBox in GTK3
}
*/

// -- tperry 13-11-2025: GTK3 - rewrite for GtkComboBox instead of GtkOptionMenu
static gint
moz_gtk_optionbutton_paint(GdkWindow * drawable, GdkRectangle * area,
                           GdkRectangle * cliprect, GtkWidgetState *state)
{
	ensure_optionbutton_widget();
	GtkWidget *widget = gOptionbuttonWidget;

	// Get border width using GTK3 API
	gint border_width = gtk_container_get_border_width(GTK_CONTAINER(widget));

	GdkRectangle button_area;
	button_area.x = cliprect->x + border_width;
	button_area.y = cliprect->y + border_width;
	button_area.width = cliprect->width - 2 * border_width;
	button_area.height = cliprect->height - 2 * border_width;

	GtkStyleContext *context = gtk_widget_get_style_context(widget);
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	if (area) {
		// cairo_rectangle(cr, area->x, area->y, area->width, area->height);
		// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	}
	
	// Convert state
	GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
	if (state) {
		if (state->disabled)
			state_flags = GTK_STATE_FLAG_INSENSITIVE;
		else if (state->active)
			state_flags = GTK_STATE_FLAG_ACTIVE;
		else if (state->inHover)
			state_flags = GTK_STATE_FLAG_PRELIGHT;
		if (state->focused)
			state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_FOCUSED);
	}
	
	gtk_style_context_set_state(context, state_flags);
	
	// Render combobox button
	gtk_render_background(context, cr, button_area.x, button_area.y,
	                      button_area.width, button_area.height);
	gtk_render_frame(context, cr, button_area.x, button_area.y,
	                 button_area.width, button_area.height);

	// Render focus if needed
	if(state && state->focused)
	{
		gtk_render_focus(context, cr, cliprect->x, cliprect->y,
		                 cliprect->width, cliprect->height);
	}

	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}

// -- tperry 20-06-2026: Build a style context targeting the "tab" CSS child node of a
// GtkNotebook.  gtk_widget_get_style_context(gTabWidget) returns the "notebook" node;
// calling gtk_render_extension() on that node produces solid black on modern themes
// because the visual rendering belongs to the "tab" child CSS node.
//
// The GTK3 CSS node hierarchy for notebook tabs is:
//   notebook > header.{top|bottom|left|right} > tabs > tab
// A 2-level path (notebook > tab) doesn't match Adwaita's CSS selectors and
// produces transparent output.  We build the full 4-level hierarchy here.
// -- tperry 20-06-2026: Build a style context for a GtkScale trough (track).
// GTK3 CSS node path: scale.horizontal > trough  (or scale.vertical > trough)
static GtkStyleContext*
build_scale_trough_context(bool is_horizontal, GtkStateFlags state_flags)
{
    GtkWidgetPath *path = gtk_widget_path_new();

    gint p0 = gtk_widget_path_append_type(path, GTK_TYPE_SCALE);
    gtk_widget_path_iter_set_object_name(path, p0, "scale");
    gtk_widget_path_iter_add_class(path, p0, is_horizontal ? "horizontal" : "vertical");

    gint p1 = gtk_widget_path_append_type(path, GTK_TYPE_WIDGET);
    gtk_widget_path_iter_set_object_name(path, p1, "trough");

    GtkStyleContext *ctx = gtk_style_context_new();
    gtk_style_context_set_path(ctx, path);
    gtk_style_context_set_screen(ctx, gdk_screen_get_default());
    gtk_style_context_set_state(ctx, state_flags);
    gtk_widget_path_free(path);
    return ctx;
}

// -- tperry 20-06-2026: Build a style context for a GtkScale slider (thumb).
// GTK3 CSS node path: scale.horizontal > trough > slider
static GtkStyleContext*
build_scale_slider_context(bool is_horizontal, GtkStateFlags state_flags)
{
    GtkWidgetPath *path = gtk_widget_path_new();

    gint p0 = gtk_widget_path_append_type(path, GTK_TYPE_SCALE);
    gtk_widget_path_iter_set_object_name(path, p0, "scale");
    gtk_widget_path_iter_add_class(path, p0, is_horizontal ? "horizontal" : "vertical");

    gint p1 = gtk_widget_path_append_type(path, GTK_TYPE_WIDGET);
    gtk_widget_path_iter_set_object_name(path, p1, "trough");

    gint p2 = gtk_widget_path_append_type(path, GTK_TYPE_WIDGET);
    gtk_widget_path_iter_set_object_name(path, p2, "slider");

    GtkStyleContext *ctx = gtk_style_context_new();
    gtk_style_context_set_path(ctx, path);
    gtk_style_context_set_screen(ctx, gdk_screen_get_default());
    gtk_style_context_set_state(ctx, state_flags);
    gtk_widget_path_free(path);
    return ctx;
}

// -- tperry 20-06-2026: Build a style context for one half of a GtkSpinButton.
// GTK3 CSS node path: spinbutton > button.up  or  spinbutton > button.down
static GtkStyleContext*
build_spinbutton_button_context(bool is_up, GtkStateFlags state_flags)
{
    GtkWidgetPath *path = gtk_widget_path_new();

    // Level 1: spinbutton
    gint p0 = gtk_widget_path_append_type(path, GTK_TYPE_SPIN_BUTTON);
    gtk_widget_path_iter_set_object_name(path, p0, "spinbutton");

    // Level 2: button.up or button.down
    gint p1 = gtk_widget_path_append_type(path, GTK_TYPE_BUTTON);
    gtk_widget_path_iter_set_object_name(path, p1, "button");
    gtk_widget_path_iter_add_class(path, p1, is_up ? "up" : "down");

    GtkStyleContext *ctx = gtk_style_context_new();
    gtk_style_context_set_path(ctx, path);
    gtk_style_context_set_screen(ctx, gdk_screen_get_default());
    gtk_style_context_set_state(ctx, state_flags);

    gtk_widget_path_free(path);
    return ctx;
}

static GtkStyleContext*
build_tab_context(GtkPositionType gap_side, GtkStateFlags state_flags)
{
    GtkWidgetPath *path = gtk_widget_path_new();

    // Level 1: notebook
    gint p0 = gtk_widget_path_append_type(path, GTK_TYPE_NOTEBOOK);
    gtk_widget_path_iter_set_object_name(path, p0, "notebook");

    // Level 2: header with position class
    // gap_side is which side of the tab connects to the panel; invert to get tab strip edge
    const char *pos_class;
    switch (gap_side) {
        case GTK_POS_BOTTOM: pos_class = "top";    break;
        case GTK_POS_TOP:    pos_class = "bottom"; break;
        case GTK_POS_RIGHT:  pos_class = "left";   break;
        case GTK_POS_LEFT:   pos_class = "right";  break;
        default:             pos_class = "top";    break;
    }
    gint p1 = gtk_widget_path_append_type(path, GTK_TYPE_BOX);
    gtk_widget_path_iter_set_object_name(path, p1, "header");
    gtk_widget_path_iter_add_class(path, p1, pos_class);

    // Level 3: tabs container
    gint p2 = gtk_widget_path_append_type(path, GTK_TYPE_BOX);
    gtk_widget_path_iter_set_object_name(path, p2, "tabs");

    // Level 4: individual tab
    gint p3 = gtk_widget_path_append_type(path, GTK_TYPE_BOX);
    gtk_widget_path_iter_set_object_name(path, p3, "tab");

    GtkStyleContext *ctx = gtk_style_context_new();
    gtk_style_context_set_path(ctx, path);
    gtk_style_context_set_screen(ctx, gdk_screen_get_default());
    gtk_style_context_set_state(ctx, state_flags);

    gtk_widget_path_free(path);
    return ctx;
}

// -- tperry 20-06-2026: Render both spinbutton arrow buttons to a cairo surface.
// flags mirrors the legacy moz_gtk_spinbutton_paint convention:
//   GTK_POS_TOP    → up button is active/prelight
//   GTK_POS_BOTTOM → down button is active/prelight
//   0              → both buttons normal
cairo_surface_t*
moz_gtk_spinbutton_paint_to_surface(GdkRectangle *rect, GtkWidgetState *state, gint flags,
                                     int *out_width, int *out_height)
{
    int width  = rect->width;
    int height = rect->height;
    if (out_width)  *out_width  = width;
    if (out_height) *out_height = height;


    cairo_t *cr;
    cairo_surface_t *surface = make_transparent_surface(width, height, &cr);

    int half = height / 2;

    // Determine per-button state flags
    GtkStateFlags up_state   = GTK_STATE_FLAG_NORMAL;
    GtkStateFlags down_state = GTK_STATE_FLAG_NORMAL;

    if (state && state->disabled)
    {
        up_state   = GTK_STATE_FLAG_INSENSITIVE;
        down_state = GTK_STATE_FLAG_INSENSITIVE;
    }
    else
    {
        if (flags == GTK_POS_TOP)
            up_state   = state->active ? GTK_STATE_FLAG_ACTIVE : GTK_STATE_FLAG_PRELIGHT;
        if (flags == GTK_POS_BOTTOM)
            down_state = state->active ? GTK_STATE_FLAG_ACTIVE : GTK_STATE_FLAG_PRELIGHT;
    }

    // Render up button (top half)
    {
        GtkStyleContext *ctx = build_spinbutton_button_context(true, up_state);

        gtk_render_background(ctx, cr, 0, 0, width, half);
        gtk_render_frame    (ctx, cr, 0, 0, width, half);

        double arrow_size = MIN(width, half) * 0.5;
        if (arrow_size < 1.0) arrow_size = 1.0;
        gtk_render_arrow(ctx, cr, 0.0,
                         (width  - arrow_size) / 2.0,
                         (half   - arrow_size) / 2.0,
                         arrow_size);
        g_object_unref(ctx);
    }

    // Render down button (bottom half)
    {
        int bot_h = height - half;
        GtkStyleContext *ctx = build_spinbutton_button_context(false, down_state);

        gtk_render_background(ctx, cr, 0, half, width, bot_h);
        gtk_render_frame    (ctx, cr, 0, half, width, bot_h);

        double arrow_size = MIN(width, bot_h) * 0.5;
        if (arrow_size < 1.0) arrow_size = 1.0;
        gtk_render_arrow(ctx, cr, G_PI,
                         (width - arrow_size) / 2.0,
                         half + (bot_h - arrow_size) / 2.0,
                         arrow_size);
        g_object_unref(ctx);
    }

    cairo_destroy(cr);
    return surface;
}

// -- tperry 20-06-2026: Render a notebook tab to a cairo surface (direct GTK3 path)
cairo_surface_t*
moz_gtk_tab_paint_to_surface(GdkRectangle *rect, GtkWidgetState *state, gint flags,
                              int *out_width, int *out_height)
{
    int width  = rect->width;
    int height = rect->height;

    if (out_width)  *out_width  = width;
    if (out_height) *out_height = height;

    // gap_side: which side of the tab shape connects to the panel (opens toward panel)
    GtkPositionType gap_side;
    if      (flags & MOZ_GTK_TAB_POS_BOTTOM) gap_side = GTK_POS_TOP;
    else if (flags & MOZ_GTK_TAB_POS_LEFT)   gap_side = GTK_POS_RIGHT;
    else if (flags & MOZ_GTK_TAB_POS_RIGHT)  gap_side = GTK_POS_LEFT;
    else                                      gap_side = GTK_POS_BOTTOM;

    gboolean selected = (flags & MOZ_GTK_TAB_SELECTED) ? TRUE : FALSE;

    // Merge widget interaction state into the CSS state flags
    // selected → :checked, hover → :hover (:prelight), disabled → :insensitive
    GtkStateFlags state_flags = selected ? GTK_STATE_FLAG_CHECKED : GTK_STATE_FLAG_NORMAL;
    if (state && state->inHover)  state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_PRELIGHT);
    if (state && state->disabled) state_flags = (GtkStateFlags)(state_flags | GTK_STATE_FLAG_INSENSITIVE);

    GtkStyleContext *context = build_tab_context(gap_side, state_flags);

    cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t *cr = cairo_create(surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR); cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    // gtk_render_extension renders background + border with the appropriate side open
    gtk_render_extension(context, cr, 0, 0, width, height, gap_side);

    cairo_destroy(cr);
    g_object_unref(context);
    return surface;
}

static gint
moz_gtk_tab_paint(GdkWindow * drawable, GdkRectangle * rect,
                  GdkRectangle * cliprect, gint flags)
{
	/*
	 * In order to get the correct shadows and highlights, GTK paints
	 * tabs right-to-left (end-to-beginning, to be generic), leaving
	 * out the active tab, and then paints the current tab once
	 * everything else is painted.  In addition, GTK uses a 2-pixel
	 * overlap between adjacent tabs (this value is hard-coded in
	 * gtknotebook.c).  For purposes of mapping to gecko's frame
	 * positions, we put this overlap on the far edge of the frame
	 * (i.e., for a horizontal/top tab strip, we shift the left side
	 * of each tab 2px to the left, into the neighboring tab's frame
	 * rect.  The right 2px * of a tab's frame will be referred to as
	 * the "overlap area".
	 *
	 * Since we can't guarantee painting order with gecko, we need to
	 * manage the overlap area manually. There are three types of tab
	 * boundaries we need to handle:
	 *
	 * * two non-active tabs: In this case, we just have both tabs
	 *   paint normally.
	 *
	 * * non-active to active tab: Here, we need the tab on the left to paint
	 *                             itself normally, then paint the edge of the
	 *                             active tab in its overlap area.
	 *
	 * * active to non-active tab: In this case, we just have both tabs paint
	 *                             normally.
	 *
	 * We need to make an exception for the first tab - since there is

	 * no tab to the left to paint the overlap area, we do _not_ shift
	 * the tab left by 2px.
	 */

	// -- tperry 13-11-2025: GTK3 - rewrite to use GtkStyleContext and gtk_render_*
	ensure_tab_widget();

	GtkStyleContext *context = gtk_widget_get_style_context(gTabWidget);
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	if (cliprect) {
		// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
		// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	}
	
	// Render notebook background
	gtk_style_context_set_state(context, GTK_STATE_FLAG_NORMAL);
	gtk_render_background(context, cr, rect->x, rect->y, rect->width, rect->height);

	// Adjust for tab overlap (except first tab)
	if (!(flags & MOZ_GTK_TAB_FIRST))
	{
		rect->x -= 2;
		rect->width += 2;
	}

	// Determine tab position
	GtkPositionType t;
	if(flags & MOZ_GTK_TAB_POS_BOTTOM)
		t = GTK_POS_TOP;
	else if(flags & MOZ_GTK_TAB_POS_LEFT)
		t = GTK_POS_RIGHT;
	else if(flags & MOZ_GTK_TAB_POS_RIGHT)
		t = GTK_POS_LEFT;
	else
		t = GTK_POS_BOTTOM;

	// Set state for tab (selected or not)
	GtkStateFlags state_flags = (flags & MOZ_GTK_TAB_SELECTED) ? 
	                             GTK_STATE_FLAG_NORMAL : GTK_STATE_FLAG_ACTIVE;
	gtk_style_context_set_state(context, state_flags);
	
	// Render tab extension
	gtk_render_extension(context, cr, rect->x, rect->y, rect->width, rect->height, t);

	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}


// -- tperry 15-11-2025: GTK3 - rewrite to use GtkStyleContext and proper theme color lookup
void moz_gtk_get_widget_color(GtkStateType state,
                              uint2 &red,uint2 &green,uint2 &blue)
{
	// gtk_style_context_get_background_color() was deprecated in GTK 3.16 and
	// returns black (0,0,0,1) on modern themes that use CSS nodes/gradients
	// rather than simple GdkRGBA properties.  We use named theme colors instead,
	// falling back to a render-and-sample approach if the named colors are absent.

	GdkRGBA rgba = {0.0, 0.0, 0.0, 1.0};

	// GTK3 style contexts are available on unrealized widgets; ensure we have one.
	ensure_label_widget();
	GtkStyleContext *ctx = gtk_widget_get_style_context(gLabelWidget);

	if (state == GTK_STATE_SELECTED)
	{
		// Most GTK3 themes define @theme_selected_bg_color.
		// GNOME 42+ / libadwaita themes use @accent_bg_color instead.
		if (!gtk_style_context_lookup_color(ctx, "theme_selected_bg_color", &rgba) &&
		    !gtk_style_context_lookup_color(ctx, "accent_bg_color", &rgba))
		{
			// Last resort: render a GtkListBoxRow in selected state to a tiny
			// surface and sample the centre pixel.
			GtkWidget *window  = gtk_offscreen_window_new();
			GtkWidget *listbox = gtk_list_box_new();
			GtkWidget *row     = gtk_list_box_row_new();
			gtk_container_add(GTK_CONTAINER(listbox), row);
			gtk_container_add(GTK_CONTAINER(window),  listbox);
			gtk_widget_show_all(window);
			gtk_list_box_select_row(GTK_LIST_BOX(listbox), GTK_LIST_BOX_ROW(row));

			GtkStyleContext *row_ctx = gtk_widget_get_style_context(row);
			cairo_surface_t *surface =
			    cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 4, 4);
			cairo_t *cr = cairo_create(surface);
			gtk_style_context_save(row_ctx);
			gtk_style_context_set_state(row_ctx,
			    (GtkStateFlags)(GTK_STATE_FLAG_SELECTED | GTK_STATE_FLAG_FOCUSED));
			gtk_render_background(row_ctx, cr, 0, 0, 4, 4);
			gtk_style_context_restore(row_ctx);
			cairo_destroy(cr);

			cairo_surface_flush(surface);
			unsigned char *data   = cairo_image_surface_get_data(surface);
			int            stride = cairo_image_surface_get_stride(surface);
			// Centre pixel; CAIRO_FORMAT_ARGB32 is pre-multiplied BGRA on LE
			unsigned char *px = data + 2 * stride + 2 * 4;
			unsigned char  a  = px[3];
			if (a > 0)
			{
				rgba.red   = px[2] / (double)a;
				rgba.green = px[1] / (double)a;
				rgba.blue  = px[0] / (double)a;
				rgba.alpha = 1.0;
			}
			// else: leave rgba at {0,0,0,1} — theme rendered nothing, keep fallback

			cairo_surface_destroy(surface);
			gtk_widget_destroy(window);

			// If we still have black (theme renders transparent selection),
			// try querying the selection colour from the label's style context
			// directly, ignoring alpha — some themes set it as a CSS property
			// even if they don't render a background here.
			if (rgba.red == 0.0 && rgba.green == 0.0 && rgba.blue == 0.0)
			{
				// Try @selected_bg_color (older theme name) as a final attempt.
				gtk_style_context_lookup_color(ctx, "selected_bg_color", &rgba);
			}
		}
	}
	else
	{
		// Normal / active / insensitive: try @theme_bg_color first.
		if (!gtk_style_context_lookup_color(ctx, "theme_bg_color", &rgba))
		{
			// Fallback: ask the style context directly (may return black on
			// some newer themes, but it is acceptable for non-selected state).
			GtkStateFlags state_flags = GTK_STATE_FLAG_NORMAL;
			if (state == GTK_STATE_ACTIVE)
				state_flags = GTK_STATE_FLAG_ACTIVE;
			else if (state == GTK_STATE_PRELIGHT)
				state_flags = GTK_STATE_FLAG_PRELIGHT;
			else if (state == GTK_STATE_INSENSITIVE)
				state_flags = GTK_STATE_FLAG_INSENSITIVE;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
			gtk_style_context_get_background_color(ctx, state_flags, &rgba);
G_GNUC_END_IGNORE_DEPRECATIONS
		}
	}

	// Clamp to [0,1] before scaling to avoid overflow on pre-multiplied samples
	red   = (uint2)(CLAMP(rgba.red,   0.0, 1.0) * 65535.0);
	green = (uint2)(CLAMP(rgba.green, 0.0, 1.0) * 65535.0);
	blue  = (uint2)(CLAMP(rgba.blue,  0.0, 1.0) * 65535.0);
}

// -- HyperXTalk: Read the foreground (text) colour for the given GTK state.
// Adapted from the GTK2 version (which used GtkStyle::fg[]); rewritten for GTK3
// to use GtkStyleContext and gtk_style_context_get_color().
void moz_gtk_get_widget_fg_color(GtkStateFlags state_flags,
                                 uint2 &red, uint2 &green, uint2 &blue)
{
    ensure_label_widget();
    if (gLabelWidget == nullptr)
        return;
    GtkStyleContext *context = gtk_widget_get_style_context(gLabelWidget);
    GdkRGBA color = {0, 0, 0, 1};
    gtk_style_context_get_color(context, state_flags, &color);
    red   = (uint2)(color.red   * 65535.0);
    green = (uint2)(color.green * 65535.0);
    blue  = (uint2)(color.blue  * 65535.0);
}

// -- tperry 16-11-2025: New function to render tab panels to cairo surface
cairo_surface_t*
moz_gtk_tabpanels_paint_to_surface(GdkRectangle * rect, int y, int w,
                                    int *out_width, int *out_height)
{
	ensure_tab_widget();
	
	int width = rect->width;
	int height = rect->height;
	
	if (out_width) *out_width = width;
	if (out_height) *out_height = height;
	
	// Create surface for rendering
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cr = cairo_create(surface);
	
	// Clear to transparent
	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	
	// Get button background color (same as scrollbar track, progress bar trough)
	ensure_button_widget();
	GtkStyleContext *button_context = gtk_widget_get_style_context(gButtonWidget);
	GdkRGBA bg_color = {0.8, 0.8, 0.8, 1.0};
	if (!gtk_style_context_lookup_color(button_context, "theme_bg_color", &bg_color))
	{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		gtk_style_context_get_background_color(button_context, GTK_STATE_FLAG_NORMAL, &bg_color);
G_GNUC_END_IGNORE_DEPRECATIONS
	}

	// Fill tab panel background with button background color
	cairo_set_source_rgba(cr, bg_color.red, bg_color.green, bg_color.blue, bg_color.alpha);
	cairo_rectangle(cr, 0, 0, width, height);
	cairo_fill(cr);
	
	cairo_destroy(cr);
	cairo_surface_flush(surface);
	
	return surface;
}

// -- tperry 13-11-2025: GTK3 - legacy GdkWindow rendering (deprecated)
static gint
moz_gtk_tabpanels_paint(GdkWindow * drawable, GdkRectangle * rect,
                        GdkRectangle * cliprect, int y, int w)
{
	ensure_tab_widget();
	
	cairo_t *cr = moz_gdk_create_cairo_context(drawable);
	
	// Set clip region
	if (cliprect) {
		// cairo_rectangle(cr, cliprect->x, cliprect->y, cliprect->width, cliprect->height);
		// cairo_clip(cr); // -- tperry 15-11-2025: Disabled - system Cairo 1.16.0 crashes
	}
	
	// Get button background color (same as scrollbar track, progress bar trough)
	ensure_button_widget();
	GtkStyleContext *button_context = gtk_widget_get_style_context(gButtonWidget);
	GdkRGBA bg_color = {0.8, 0.8, 0.8, 1.0};
	if (!gtk_style_context_lookup_color(button_context, "theme_bg_color", &bg_color))
	{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		gtk_style_context_get_background_color(button_context, GTK_STATE_FLAG_NORMAL, &bg_color);
G_GNUC_END_IGNORE_DEPRECATIONS
	}

	// Fill tab panel background with button background color
	cairo_set_source_rgba(cr, bg_color.red, bg_color.green, bg_color.blue, bg_color.alpha);
	cairo_rectangle(cr, rect->x, rect->y, rect->width, rect->height);
	cairo_fill(cr);
	
	// -- tperry 16-11-2025: Disabled frame_gap - causes GTK warnings and renders black background
	// GtkStyleContext *context = gtk_widget_get_style_context(gTabWidget);
	// gtk_render_frame_gap(context, cr, rect->x, rect->y, rect->width, rect->height,
	//                      GTK_POS_TOP, y, y + w);
	
	cairo_destroy(cr);
	return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_widget_border(GtkThemeWidgetType widget, gint * xthickness,
                          gint * ythickness)
{
	GtkWidget *w;
	switch (widget)
	{
	case MOZ_GTK_BUTTON:
		ensure_button_widget();
		w = gButtonWidget;
		break;
	case MOZ_GTK_TOOLBAR:
		ensure_handlebox_widget();
		w = gHandleBoxWidget;
		break;
	case MOZ_GTK_ENTRY:
		ensure_entry_widget();
		w = gEntryWidget;
		break;
	case MOZ_GTK_DROPDOWN_ARROW:
		ensure_arrow_widget();
		w = gDropdownButtonWidget;
		break;
	case MOZ_GTK_TABPANELS:
		ensure_tab_widget();
		w = gTabWidget;
		break;
	case MOZ_GTK_PROGRESSBAR:
		ensure_progress_widget();
		w = gProgressWidget;
		break;
	case MOZ_GTK_FRAME:
		ensure_frame_widget();
		w = gFrameWidget;
		break;
	case MOZ_GTK_CHECKBUTTON_CONTAINER:
	case MOZ_GTK_RADIOBUTTON_CONTAINER:
		/* This is a hardcoded value. */
		if (xthickness)
			*xthickness = 1;
		if (ythickness)
			*ythickness = 1;
		return MOZ_GTK_SUCCESS;
		break;
	case MOZ_GTK_CHECKBUTTON:
	case MOZ_GTK_RADIOBUTTON:
	case MOZ_GTK_MENUITEMHIGHLIGHT:
	case MOZ_GTK_SCROLLBAR_BUTTON:
	case MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL:
	case MOZ_GTK_SCROLLBAR_TRACK_VERTICAL:
	case MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL:
	case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:

	case MOZ_GTK_GRIPPER:
	case MOZ_GTK_TOOLTIP:
	case MOZ_GTK_LABEL:
	case MOZ_GTK_PROGRESS_CHUNK:
	case MOZ_GTK_TAB:
		/* These widgets have no borders, since they are not containers. */
		if (xthickness)
			*xthickness = 0;
		if (ythickness)
			*ythickness = 0;
		return MOZ_GTK_SUCCESS;
	default:
		//g_warning("Unsupported widget type: %d", widget);
		return MOZ_GTK_UNKNOWN_WIDGET;
	}

	// -- tperry 13-11-2025: GTK3 - widget->style removed, pass widget directly
	if (xthickness)
		*xthickness = XTHICKNESS(w);
	if (ythickness)
		*ythickness = YTHICKNESS(w);

	return MOZ_GTK_SUCCESS;
}

gint moz_gtk_get_dropdown_arrow_size(gint * width, gint * height)
{
	ensure_arrow_widget();

	/*
	 * First get the border of the dropdown arrow, then add in the requested
	 * size of the arrow.  Note that the minimum arrow size is fixed at
	 * 11 pixels.
	 */

	if (width)
	{
		// -- tperry 13-11-2025: GTK3 - widget->style removed, pass widget directly
		*width = 2 * (1 + XTHICKNESS(gDropdownButtonWidget));
		// -- tperry 13-11-2025: GTK3 - GtkMisc removed, use margin properties
		gint xmargin = gtk_widget_get_margin_start(gArrowWidget) + gtk_widget_get_margin_end(gArrowWidget);
		*width += 11 + xmargin;
	}
	if (height)
	{
		// -- tperry 13-11-2025: GTK3 - widget->style removed, pass widget directly
		*height = 2 * (1 + YTHICKNESS(gDropdownButtonWidget));
		// -- tperry 13-11-2025: GTK3 - GtkMisc removed, use margin properties
		gint ymargin = gtk_widget_get_margin_top(gArrowWidget) + gtk_widget_get_margin_bottom(gArrowWidget);
		*height += 11 + ymargin;
	}

	return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_get_slider_metrics(gint * slider_width, gint * trough_border,
                           gint * stepper_size, gint * stepper_spacing,
                           gint * min_slider_size, gint *focus_line_width,
                           gint * focus_padding)
{
	ensure_scale_widget();

	if(focus_line_width)
		gtk_widget_style_get (GTK_WIDGET(gHScaleWidget),
		                        "focus-line-width", &focus_line_width, NULL);

	if(focus_padding)
		gtk_widget_style_get (GTK_WIDGET(gHScaleWidget),
		                        "focus-padding", &focus_padding,
		                        NULL);


	if (slider_width)
	{
		gtk_widget_style_get(gHScaleWidget, "slider_width",
		                       slider_width, NULL);
	}

	if (trough_border)
	{
		gtk_widget_style_get(gHScaleWidget, "trough_border",
		                       trough_border, NULL);
	}

	if (stepper_size)
	{
		gtk_widget_style_get(gHScaleWidget, "stepper_size",
		                       stepper_size, NULL);
	}

	if (stepper_spacing)
	{
		gtk_widget_style_get(gHScaleWidget, "stepper_spacing",
		                       stepper_spacing, NULL);
	}

	// -- tperry 15-11-2025: GTK3 removed min-slider-length property, use CSS min-width/height
	if (min_slider_size)
	{
		// GTK3: Get minimum slider size from CSS or use default
		GtkStyleContext *context = gtk_widget_get_style_context(gHScaleWidget);
		gint min_width, min_height;
		gtk_widget_get_size_request(gHScaleWidget, &min_width, &min_height);
		// Default to 21 pixels if not set (GTK3 default)
		*min_slider_size = (min_width > 0) ? min_width : 21;
	}

	return MOZ_GTK_SUCCESS;
}


gint
moz_gtk_get_scrollbar_metrics(gint * slider_width, gint * trough_border,
                              gint * stepper_size, gint * stepper_spacing,
                              gint * min_slider_size, gint *focus_line_width,
                              gint * focus_padding)
{
	ensure_scrollbar_widget();

	if(focus_line_width)
		gtk_widget_style_get(GTK_WIDGET(gHorizScrollbarWidget),
		                        "focus-line-width", &focus_line_width, NULL);

	if(focus_padding)
		gtk_widget_style_get(GTK_WIDGET(gHorizScrollbarWidget),
		                        "focus-padding", &focus_padding,
		                        NULL);


	if (slider_width)
	{
		gtk_widget_style_get(gHorizScrollbarWidget, "slider_width",
		                       slider_width, NULL);
	}

	if (trough_border)
	{
		gtk_widget_style_get(gHorizScrollbarWidget, "trough_border",
		                       trough_border, NULL);
	}

	if (stepper_size)
	{
		// -- tperry 16-11-2025: GTK3 modern themes don't use stepper arrows
		*stepper_size = 0;
	}

	if (stepper_spacing)
	{
		gtk_widget_style_get(gHorizScrollbarWidget, "stepper_spacing",
		                       stepper_spacing, NULL);
		// -- tperry 16-11-2025: No spacing needed if no steppers
		*stepper_spacing = 0;
	}

	// -- tperry 15-11-2025: GTK3 removed min-slider-length property, use CSS min-width/height
	if (min_slider_size)
	{
		// GTK3: Get minimum slider size from CSS or use default
		GtkStyleContext *context = gtk_widget_get_style_context(gHorizScrollbarWidget);
		gint min_width, min_height;
		gtk_widget_get_size_request(gHorizScrollbarWidget, &min_width, &min_height);
		// Default to 21 pixels if not set (GTK3 default for scrollbar thumb)
		*min_slider_size = (min_height > 0) ? min_height : 21;
	}

	return MOZ_GTK_SUCCESS;
}

gint
moz_gtk_widget_paint(GtkThemeWidgetType widget, GdkWindow * drawable,
                     GdkRectangle * rect, GdkRectangle * cliprect,
                     GtkWidgetState * state, gint flags)
{
	switch (widget)
	{
	case MOZ_GTK_LABEL:
		ensure_label_widget();
		return moz_gtk_label_paint(drawable, rect, cliprect);
		break;
	case MOZ_GTK_BUTTON:
		ensure_button_widget();
		return moz_gtk_button_paint(drawable, rect, cliprect,
		                            state,
		                            (GtkReliefStyle) flags,
		                            gButtonWidget);
		break;
	case MOZ_GTK_CHECKBUTTON:
	case MOZ_GTK_RADIOBUTTON:
		ensure_radiobutton_widget();
		ensure_checkbox_widget();

		return moz_gtk_toggle_paint(drawable, rect, cliprect,
		                            state, (gboolean) flags,
		                            (widget ==
		                             MOZ_GTK_RADIOBUTTON));
		break;
	case MOZ_GTK_SCROLLBAR_BUTTON:
		return moz_gtk_scrollbar_button_paint(drawable, rect,
		                                      cliprect, state,
		                                      (GtkArrowType)
		                                      flags);
		break;
	case MOZ_GTK_SCROLLBAR_TRACK_HORIZONTAL:
	case MOZ_GTK_SCROLLBAR_TRACK_VERTICAL:
		return moz_gtk_scrollbar_trough_paint(widget,
		                                      drawable, rect,
		                                      cliprect,
		                                      state);
		break;
	case MOZ_GTK_SCROLLBAR_THUMB_HORIZONTAL:
	case MOZ_GTK_SCROLLBAR_THUMB_VERTICAL:
		return moz_gtk_scrollbar_thumb_paint(widget, drawable,
		                                     rect, cliprect,
		                                     state);
		break;
	case MOZ_GTK_GRIPPER:
		return moz_gtk_gripper_paint(drawable, rect, cliprect,
		                             state);
		break;
	case MOZ_GTK_ENTRY:
		return moz_gtk_entry_paint(drawable, rect, cliprect,
		                           state);
		break;
	case MOZ_GTK_ENTRY_FRAME:
		return moz_gtk_entry_frame_paint(drawable, rect, cliprect,
		                                 state);
		break;
	case MOZ_GTK_DROPDOWN_ARROW:
		return moz_gtk_dropdown_arrow_paint(drawable, rect,
		                                    cliprect, state);
		break;
	case MOZ_GTK_CHECKBUTTON_CONTAINER:
	case MOZ_GTK_RADIOBUTTON_CONTAINER:
		return moz_gtk_container_paint(drawable, rect,
		                               cliprect, state,
		                               (widget ==
		                                MOZ_GTK_RADIOBUTTON_CONTAINER));
		break;
	case MOZ_GTK_TOOLBAR:
		return moz_gtk_toolbar_paint(drawable, rect,
		                             cliprect);
		break;
	case MOZ_GTK_TOOLTIP:
		return moz_gtk_tooltip_paint(drawable, rect,
		                             cliprect);
		break;
	case MOZ_GTK_FRAME:
		return moz_gtk_frame_paint(drawable, rect, cliprect);
		break;
	case MOZ_GTK_PROGRESSBAR:
		return moz_gtk_progressbar_paint(drawable, rect,
		                                 cliprect);
		break;
	case MOZ_GTK_PROGRESS_CHUNK:
		return moz_gtk_progress_chunk_paint(drawable, rect,
		                                    cliprect, flags);
		break;
	case MOZ_GTK_TABPANELS:
		{
			return moz_gtk_tabpanels_paint(drawable, rect,
			                               cliprect, state->curpos,
			                               state->maxpos);
		}
		break;
	case MOZ_GTK_OPTIONBUTTON:
		return moz_gtk_optionbutton_paint(drawable, rect,
		                                  cliprect, state);
		break;
	case MOZ_GTK_TAB:
		return moz_gtk_tab_paint(drawable, rect, cliprect,
		                         flags);
		break;
	case MOZ_GTK_LISTBOX:
		return moz_gtk_listbox_paint(drawable, rect, cliprect);
		break;
	case MOZ_GTK_SPINBUTTON:
		return moz_gtk_spinbutton_paint(drawable, rect, cliprect, state, flags);
		break;
	case MOZ_GTK_MENUITEMHIGHLIGHT:
		return moz_gtk_menuitem_paint(drawable, rect, cliprect);
		break;
	case MOZ_GTK_SCALE_TRACK_VERTICAL:
	case MOZ_GTK_SCALE_TRACK_HORIZONTAL:
		return moz_gtk_scale_track_paint(widget, drawable, rect, cliprect, flags);
		break;
	case MOZ_GTK_SCALE_THUMB_VERTICAL:
	case MOZ_GTK_SCALE_THUMB_HORIZONTAL:
		return moz_gtk_scale_thumb_paint(widget, drawable, rect, cliprect, state);
		break;

	default:
		//g_warning("Unknown widget type: %d", widget);
		break;
	}

	return MOZ_GTK_UNKNOWN_WIDGET;
}

void moz_gtk_invalidate_caches()
{
	// Nothing to invalidate currently — MCimagecache is flushed by the caller.
}

gint moz_gtk_shutdown()
{
    // Do NOT call gtk_widget_destroy() here.
    //
    // moz_gtk_shutdown() is only called during process exit.  Immediately
    // after this function returns, MCScreenDC::close() calls
    // gdk_display_close(), which sends XCloseDisplay(); the X server then
    // frees every X11 resource owned by the client.  The OS reclaims the
    // heap memory.
    //
    // Calling gtk_widget_destroy() at this point is unsafe: GTK's internal
    // style-attachment state can be left pointing at windows that are
    // already gone, causing crashes inside gtk_widget_unrealize().
    //
    // Simply null the pointers.  The widgets are leaked, but that is
    // harmless given that the process is about to exit.
    gButtonWidget         = nullptr;
    gCheckboxWidget       = nullptr;
    gRadiobuttonWidget    = nullptr;
    gHorizScrollbarWidget = nullptr;
    gVertScrollbarWidget  = nullptr;
    gEntryWidget          = nullptr;
    gArrowWidget          = nullptr;
    gDropdownButtonWidget = nullptr;
    gHandleBoxWidget      = nullptr;
    gFrameWidget          = nullptr;
    gProgressWidget       = nullptr;
    gTabWidget            = nullptr;
    gLabelOffscreenWindow = nullptr;
    gLabelWidget          = nullptr;
    gOptionbuttonWidget   = nullptr;
    gSpinbuttonWidget     = nullptr;
    gMenuitemWidget       = nullptr;
    gHScaleWidget         = nullptr;
    gVScaleWidget         = nullptr;

    // GTK3: gTooltipWindow is a GtkWindow created in ensure_tooltip_widget().
    // Null it rather than destroying it for the same safety reasons above.
    gTooltipWindow        = nullptr;

    // gProtoWindow / gProtoLayout: in GTK3 setup_widget_prototype() no longer
    // creates a real prototype window (it is a no-op aside from setting object
    // data), but null them defensively should the pointers ever be set.
    gProtoWindow          = nullptr;
    gProtoLayout          = nullptr;

    return MOZ_GTK_SUCCESS;
}
