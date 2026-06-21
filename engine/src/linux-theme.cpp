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


#include "platform.h"

#include "globdefs.h"
#include "objdefs.h"
#include "parsedef.h"
#include "filedefs.h"
#include "mcstring.h"
#include "globals.h"
#include "mctheme.h"
#include "util.h"
#include "object.h"
#include "stack.h"
#include "font.h"


#include <gtk/gtk.h>

// -- tperry: GTK3 rewrite — all GtkStyle* usage replaced with GtkStyleContext.
// GtkStyle and gtk_widget_get_style() were deprecated in GTK 3.0 and do not
// reflect CSS-based theme properties correctly on modern GTK3 themes.

// Cached widgets for each control type (used to obtain GtkStyleContext*)
static GtkWidget* s_widgets[kMCPlatformControlTypeMessageBox+1];

// Container for widgets
static GtkWidget* s_widget_container = NULL;

extern "C" int initialise_weak_link_gtk(void);
extern "C" int initialise_weak_link_X11(void);

// Creates a GtkWidget corresponding to the requested control type
static GtkWidget* getWidgetForControlType(MCPlatformControlType p_type, MCPlatformControlPart p_part)
{
    // Do nothing if running in no-UI mode
    if (MCnoui)
        return NULL;

    // Ensure that our container widget exists
    if (s_widget_container == NULL)
    {
        if (!MCscreen -> hasfeature(PLATFORM_FEATURE_NATIVE_THEMES))
            return NULL;

        gtk_init(NULL, NULL);

        // Create a new window
        GtkWidget* t_window;
        t_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        s_widgets[kMCPlatformControlTypeGeneric] = t_window;

        // Ensure it actually exists
        gtk_widget_realize(t_window);

        // Create a container to store our widgets and put it in the window
        s_widget_container = gtk_fixed_new();
        gtk_container_add(GTK_CONTAINER(t_window), GTK_WIDGET(s_widget_container));
        gtk_widget_realize(GTK_WIDGET(s_widget_container));
    }

    // Return the existing widget if possible
    if (s_widgets[p_type] != NULL)
    {
        g_object_ref(s_widgets[p_type]);
        return s_widgets[p_type];
    }

    GtkWidget* t_the_widget;
    t_the_widget = NULL;

    bool t_suppress_add;
    t_suppress_add = false;

    switch (p_type)
    {
        case kMCPlatformControlTypeGeneric:
            t_the_widget = s_widgets[kMCPlatformControlTypeGeneric];
            t_suppress_add = true;
            break;

        case kMCPlatformControlTypeButton:
            t_the_widget = gtk_button_new();
            break;

        case kMCPlatformControlTypeCheckbox:
            t_the_widget = gtk_check_button_new();
            break;

        case kMCPlatformControlTypeRadioButton:
            t_the_widget = gtk_radio_button_new(NULL);
            break;

        case kMCPlatformControlTypeTabButton:
        case kMCPlatformControlTypeTabPane:
            t_the_widget = gtk_notebook_new();
            break;

        case kMCPlatformControlTypeLabel:
            t_the_widget = gtk_label_new("HyperXTalk");
            break;

        case kMCPlatformControlTypeInputField:
            t_the_widget = gtk_entry_new();
            break;

        case kMCPlatformControlTypeList:
            t_the_widget = gtk_tree_view_new();
            break;

        case kMCPlatformControlTypeMenu:
            t_the_widget = gtk_menu_item_new();
            break;

        case kMCPlatformControlTypeMenuItem:
            t_the_widget = gtk_menu_item_new();
            break;

        case kMCPlatformControlTypeOptionMenu:
            t_the_widget = gtk_combo_box_new();
            break;

        case kMCPlatformControlTypePulldownMenu:
            t_the_widget = gtk_menu_item_new();
            break;

        case kMCPlatformControlTypeComboBox:
            t_the_widget = gtk_combo_box_new_with_entry();
            break;

        case kMCPlatformControlTypePopupMenu:
            t_the_widget = gtk_menu_item_new();
            break;

        case kMCPlatformControlTypeProgressBar:
            t_the_widget = gtk_progress_bar_new();
            break;

        case kMCPlatformControlTypeScrollBar:
            // GTK3: gtk_vscrollbar_new removed, use gtk_scrollbar_new
            t_the_widget = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, NULL);
            break;

        case kMCPlatformControlTypeSlider:
            break;

        case kMCPlatformControlTypeSpinArrows:
            break;

        case kMCPlatformControlTypeWindow:
            t_the_widget = s_widgets[kMCPlatformControlTypeGeneric];
            g_object_ref(t_the_widget);
            t_suppress_add = true;
            break;

        case kMCPlatformControlTypeMessageBox:
            break;
    }

    if (t_the_widget == NULL)
        return NULL;

    s_widgets[p_type] = t_the_widget;

    // Add to the container and realize so that styles get set up correctly
    if (!t_suppress_add)
        gtk_fixed_put(GTK_FIXED(s_widget_container), t_the_widget, 0, 0);
    gtk_widget_realize(t_the_widget);

    g_object_ref(t_the_widget);
    return t_the_widget;
}

// Returns the GtkStyleContext for the given control type.
// The caller must NOT unref the returned context — it is owned by the widget.
static GtkStyleContext* getContextForControlType(MCPlatformControlType p_type, MCPlatformControlPart p_part)
{
    // Ensure the widget exists (getWidgetForControlType caches it in s_widgets[])
    if (s_widgets[p_type] == NULL)
    {
        GtkWidget *t_w = getWidgetForControlType(p_type, p_part);
        if (t_w != NULL)
            g_object_unref(t_w); // release extra ref; s_widgets[] holds ownership
    }
    if (s_widgets[p_type] == NULL)
        return NULL;
    return gtk_widget_get_style_context(s_widgets[p_type]);
}

// Converts MCPlatformControlState to GtkStateFlags for use with GtkStyleContext.
static GtkStateFlags stateToFlags(MCPlatformControlState p_state)
{
    if (p_state & kMCPlatformControlStateDisabled)
        return GTK_STATE_FLAG_INSENSITIVE;
    if (p_state & kMCPlatformControlStateSelected)
        return GTK_STATE_FLAG_SELECTED;
    if (p_state & kMCPlatformControlStatePressed)
        return GTK_STATE_FLAG_ACTIVE;
    if (p_state & kMCPlatformControlStateMouseOver)
        return GTK_STATE_FLAG_PRELIGHT;
    return GTK_STATE_FLAG_NORMAL;
}

// Attempts a named colour lookup from the context, trying several fallback names.
// Returns true and fills r_rgba if any name resolves; false otherwise.
static bool lookupNamedColor(GtkStyleContext *p_ctx, const char * const *p_names, GdkRGBA &r_rgba)
{
    for (int i = 0; p_names[i] != NULL; i++)
    {
        if (gtk_style_context_lookup_color(p_ctx, p_names[i], &r_rgba))
            return true;
    }
    return false;
}

// Flushes the cached widgets so they are recreated with the current theme on
// next access.  Call this whenever the GTK theme changes at runtime.
void MCLinuxThemeFlushCache(void)
{
    // Destroy the top-level window, which cascade-destroys every child widget
    // (s_widget_container and all widgets inside it).  Set all widget pointers
    // to NULL before the destroy so no dangling references remain.
    GtkWidget* t_window = s_widgets[kMCPlatformControlTypeGeneric];
    for (int i = 0; i <= (int)kMCPlatformControlTypeMessageBox; i++)
        s_widgets[i] = NULL;
    s_widget_container = NULL;

    if (t_window != NULL)
        gtk_widget_destroy(t_window);
}


bool MCPlatformGetControlThemePropInteger(MCPlatformControlType p_type, MCPlatformControlPart p_part, MCPlatformControlState p_state, MCPlatformThemeProperty p_prop, int& r_int)
{
    if (p_prop != kMCPlatformThemePropertyTextSize)
        return false;

    // We use 12-point Helvetica on Linux traditionally
    if (p_state & kMCPlatformControlStateCompatibility)
    {
        r_int = 12;
        return true;
    }

    GtkStyleContext *t_ctx = getContextForControlType(p_type, p_part);
    if (t_ctx == NULL)
        return false;

    // gtk_style_context_get() with GTK_STYLE_PROPERTY_FONT gives us the theme
    // font description without the deprecated GtkStyle.font_desc field.
    PangoFontDescription *t_desc = NULL;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_style_context_get(t_ctx, GTK_STATE_FLAG_NORMAL,
                          GTK_STYLE_PROPERTY_FONT, &t_desc, NULL);
G_GNUC_END_IGNORE_DEPRECATIONS
    if (t_desc == NULL)
        return false;

    int t_size = pango_font_description_get_size(t_desc) / PANGO_SCALE;
    pango_font_description_free(t_desc);

    if (t_size <= 0)
        return false;

    r_int = t_size;
    return true;
}

bool MCPlatformGetControlThemePropColor(MCPlatformControlType p_type, MCPlatformControlPart p_part, MCPlatformControlState p_state, MCPlatformThemeProperty p_prop, MCColor& r_color)
{
    GtkStyleContext *t_ctx = getContextForControlType(p_type, p_part);
    if (t_ctx == NULL)
        return false;

    GtkStateFlags t_flags = stateToFlags(p_state);
    GdkRGBA t_rgba = {0.0, 0.0, 0.0, 1.0};
    bool t_found = false;

    switch (p_prop)
    {
        case kMCPlatformThemePropertyTextColor:
        {
            // gtk_style_context_get_color() is the authoritative GTK3 API for
            // foreground/label colour and correctly handles CSS :selected,
            // :insensitive, :hover states via the state flags parameter.
            gtk_style_context_get_color(t_ctx, t_flags, &t_rgba);
            t_found = true;
            break;
        }

        case kMCPlatformThemePropertyBackgroundColor:
        {
            bool t_is_field = (p_type == kMCPlatformControlTypeInputField ||
                               p_type == kMCPlatformControlTypeList);

            if (t_flags == GTK_STATE_FLAG_SELECTED && t_is_field)
            {
                // Selected background for text widgets: use named colour.
                // GtkStyle.base[GTK_STATE_SELECTED] returns zeros on CSS themes.
                static const char * const k_sel_names[] = {
                    "theme_selected_bg_color", "accent_bg_color",
                    "selected_bg_color", NULL
                };
                t_found = lookupNamedColor(t_ctx, k_sel_names, t_rgba);
            }
            else if (t_is_field)
            {
                // Normal/hover/disabled base colour for text widgets.
                static const char * const k_base_names[] = {
                    "theme_base_color", "theme_unfocused_base_color", NULL
                };
                if (!lookupNamedColor(t_ctx, k_base_names, t_rgba))
                {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                    gtk_style_context_get_background_color(t_ctx, t_flags, &t_rgba);
G_GNUC_END_IGNORE_DEPRECATIONS
                }
                t_found = true;
            }
            else
            {
                // Suppress disabled state for menus to avoid weird appearance.
                bool t_is_menu = (p_type == kMCPlatformControlTypeMenu     ||
                                  p_type == kMCPlatformControlTypeOptionMenu ||
                                  p_type == kMCPlatformControlTypePopupMenu  ||
                                  p_type == kMCPlatformControlTypeMenuItem);
                if (t_is_menu && t_flags == GTK_STATE_FLAG_INSENSITIVE)
                    t_flags = GTK_STATE_FLAG_NORMAL;

                static const char * const k_bg_names[] = {
                    "theme_bg_color", "theme_unfocused_bg_color", NULL
                };
                if (!lookupNamedColor(t_ctx, k_bg_names, t_rgba))
                {
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                    gtk_style_context_get_background_color(t_ctx, t_flags, &t_rgba);
G_GNUC_END_IGNORE_DEPRECATIONS
                }
                t_found = true;
            }
            break;
        }

        case kMCPlatformThemePropertyShadowColor:
        case kMCPlatformThemePropertyBorderColor:
        {
            // GTK3 themes express borders via CSS; there is no GtkStyle.dark[]
            // equivalent.  Use the "borders" named colour if the theme defines
            // it, otherwise leave the engine to use its own default.
            static const char * const k_border_names[] = {
                "borders", "unfocused_borders", "border_color", NULL
            };
            t_found = lookupNamedColor(t_ctx, k_border_names, t_rgba);
            break;
        }

        case kMCPlatformThemePropertyFocusColor:
            // Let the engine use its default focus colour.
            break;

        case kMCPlatformThemePropertyTopEdgeColor:
        case kMCPlatformThemePropertyLeftEdgeColor:
            // Top/left highlight: let the engine use its default (white). Fine
            // for all GTK3 themes.
            break;

        case kMCPlatformThemePropertyBottomEdgeColor:
        case kMCPlatformThemePropertyRightEdgeColor:
        {
            // Bottom/right shadow edge: GTK3 themes express this via the named
            // "borders" colour.  Returning it avoids the engine's fallback of
            // getsystemfore() (near-black), which makes menu separators and
            // other 3D borders look wrong on flat GTK3 themes.
            static const char * const k_border_names[] = {
                "borders", "unfocused_borders", "border_color", NULL
            };
            t_found = lookupNamedColor(t_ctx, k_border_names, t_rgba);
            break;
        }

        default:
            break;
    }

    if (t_found)
    {
        r_color.red   = (uint2)(CLAMP(t_rgba.red,   0.0, 1.0) * 65535.0);
        r_color.green = (uint2)(CLAMP(t_rgba.green, 0.0, 1.0) * 65535.0);
        r_color.blue  = (uint2)(CLAMP(t_rgba.blue,  0.0, 1.0) * 65535.0);
    }

    return t_found;
}

// Utility function needed by the Linux font code. Gets the family name of the
// font for the given control type.
bool MCPlatformGetControlThemePropString(MCPlatformControlType p_type, MCPlatformControlPart p_part, MCPlatformControlState, MCPlatformThemeProperty p_prop, MCStringRef& r_string)
{
    if (p_prop != kMCPlatformThemePropertyTextFont)
        return false;

    GtkStyleContext *t_ctx = getContextForControlType(p_type, p_part);
    if (t_ctx == NULL)
        return false;

    PangoFontDescription *t_desc = NULL;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_style_context_get(t_ctx, GTK_STATE_FLAG_NORMAL,
                          GTK_STYLE_PROPERTY_FONT, &t_desc, NULL);
G_GNUC_END_IGNORE_DEPRECATIONS
    if (t_desc == NULL)
        return false;

    const char *t_family = pango_font_description_get_family(t_desc);
    bool t_ok = (t_family != NULL) &&
                MCStringCreateWithCString(t_family, r_string);
    pango_font_description_free(t_desc);
    return t_ok;
}

bool MCPlatformGetControlThemePropFont(MCPlatformControlType p_type, MCPlatformControlPart p_part, MCPlatformControlState p_state, MCPlatformThemeProperty p_prop, MCFontRef& r_font)
{
    if (p_prop != kMCPlatformThemePropertyTextFont)
        return false;

    // Compatibility mode: always return 12pt Helvetica
    if (p_state & kMCPlatformControlStateCompatibility)
        return MCFontCreate(MCNAME(DEFAULT_TEXT_FONT), 0, 12, r_font);

    GtkStyleContext *t_ctx = getContextForControlType(p_type, p_part);
    if (t_ctx == NULL)
        return MCFontCreate(MCNAME(DEFAULT_TEXT_FONT), 0, 12, r_font);

    PangoFontDescription *t_desc = NULL;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    gtk_style_context_get(t_ctx, GTK_STATE_FLAG_NORMAL,
                          GTK_STYLE_PROPERTY_FONT, &t_desc, NULL);
G_GNUC_END_IGNORE_DEPRECATIONS
    if (t_desc == NULL)
        return MCFontCreate(MCNAME(DEFAULT_TEXT_FONT), 0, 12, r_font);

    const char *t_family_cstr = pango_font_description_get_family(t_desc);
    if (t_family_cstr == NULL)
        t_family_cstr = DEFAULT_TEXT_FONT;

    int t_size = pango_font_description_get_size(t_desc) / PANGO_SCALE;
    if (t_size <= 0)
        t_size = 12;

    // MCPlatformGetControlThemePropInteger also reads the size from the
    // style context, so call it to get any override the engine may apply.
    MCPlatformGetControlThemePropInteger(p_type, p_part, p_state,
                                         kMCPlatformThemePropertyTextSize,
                                         t_size);

    MCNameRef t_name;
    MCNameCreateWithNativeChars((const char_t*)t_family_cstr,
                                strlen(t_family_cstr), t_name);

    bool t_ok = MCFontCreate(t_name, 0, t_size, r_font);
    MCValueRelease(t_name);
    pango_font_description_free(t_desc);
    return t_ok;
}
