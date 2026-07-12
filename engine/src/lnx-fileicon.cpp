/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

// lnx-fileicon.cpp
//
// Linux implementation of MCFileIconGetForFile / MCFileIconGetForExtension.
//
// Strategy:
//   1. Determine the MIME/content-type for the path or extension via GIO.
//   2. Ask GIO for the themed icon name(s) for that content type.
//   3. Look up the icon in the current GtkIconTheme at the requested size.
//   4. Scale if necessary, then copy the GdkPixbuf pixel data (which is
//      always non-premultiplied RGBA) into an MCImageBitmap.
//
// Compile-time requirements: glib-2.0, gio-2.0, gtk+-2.0 (2.10+).
// These are already linked for the Linux engine build.

#include "prefix.h"

#include "exec-fileicon.h"
#include "imagebitmap.h"

extern "C" int initialise_weak_link_gio(void);
extern "C" int initialise_weak_link_gtk(void);

// We cannot include any GIO headers: every GIO header transitively includes
// giotypes.h / gioenums.h which require a modern GLib (goffset, GVariant,
// GLIB_SYSDEF_AF_UNIX, etc.) that is incompatible with the bundled GTK 2.10 /
// GLib headers used by the rest of the engine.
//
// Instead, forward-declare just the GIO types and functions we need.
// The bundled glib headers (already included via gtk/gtk.h → glib.h) supply
// the GType machinery (G_TYPE_CHECK_INSTANCE_TYPE, etc.) that the macros below
// rely on.
#include <gtk/gtk.h>
#include <string.h>  // strrchr

typedef struct _GIcon       GIcon;
typedef struct _GThemedIcon GThemedIcon;

extern "C"
{
    // g_content_type_*: map a filename / extension to a MIME type and its icon.
    gchar *g_content_type_guess(const gchar *filename,
                                const guchar *data, gsize data_size,
                                gboolean *result_uncertain);
    GIcon *g_content_type_get_icon(const gchar *type);

    // GThemedIcon: icon identified by a list of theme-name strings.
    GType                g_themed_icon_get_type(void) G_GNUC_CONST;
    const gchar * const *g_themed_icon_get_names(GThemedIcon *icon);
}

#define G_TYPE_THEMED_ICON    (g_themed_icon_get_type())
#define G_IS_THEMED_ICON(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), G_TYPE_THEMED_ICON))
#define G_THEMED_ICON(obj)    ((GThemedIcon *)(obj))

////////////////////////////////////////////////////////////////////////////////

// Convert a GdkPixbuf (non-premultiplied RGBA, 8 bits per channel) into a
// new MCImageBitmap (non-premultiplied ARGB uint32_t, host byte order).
// Scales the pixbuf to p_size × p_size first if needed.
// Returns true and sets r_bitmap on success; caller must MCImageFreeBitmap().
static bool s_pixbuf_to_bitmap(GdkPixbuf   *p_pixbuf,
                                uinteger_t   p_size,
                                MCImageBitmap *&r_bitmap)
{
    if (p_pixbuf == NULL)
        return false;

    GdkPixbuf *t_scaled = NULL;
    int t_w = gdk_pixbuf_get_width(p_pixbuf);
    int t_h = gdk_pixbuf_get_height(p_pixbuf);

    if ((uinteger_t)t_w != p_size || (uinteger_t)t_h != p_size)
    {
        t_scaled = gdk_pixbuf_scale_simple(
            p_pixbuf, (int)p_size, (int)p_size, GDK_INTERP_BILINEAR);
        if (t_scaled == NULL)
            return false;
        p_pixbuf = t_scaled;
    }

    int t_n_channels = gdk_pixbuf_get_n_channels(p_pixbuf);
    int t_rowstride  = gdk_pixbuf_get_rowstride(p_pixbuf);
    const guchar *t_src_data = gdk_pixbuf_get_pixels(p_pixbuf);

    MCImageBitmap *t_bitmap = nil;
    if (!MCImageBitmapCreate(p_size, p_size, t_bitmap))
    {
        if (t_scaled) g_object_unref(t_scaled);
        return false;
    }

    uint32_t *t_dst = t_bitmap->data;

    for (uinteger_t y = 0; y < p_size; y++)
    {
        const guchar *t_row = t_src_data + y * t_rowstride;
        for (uinteger_t x = 0; x < p_size; x++)
        {
            uint8_t r = t_row[x * t_n_channels + 0];
            uint8_t g = t_row[x * t_n_channels + 1];
            uint8_t b = t_row[x * t_n_channels + 2];
            uint8_t a = (t_n_channels == 4) ? t_row[x * 4 + 3] : 0xFF;

            // kMCGPixelFormatNative on Linux = kMCGPixelFormatRGBA:
            // pack as R | (G<<8) | (B<<16) | (A<<24) = 0xAABBGGRR.
            t_dst[y * p_size + x] =
                ((uint32_t)a << 24) |
                ((uint32_t)b << 16) |
                ((uint32_t)g <<  8) |
                 (uint32_t)r;
        }
    }

    t_bitmap->has_transparency = true;
    t_bitmap->has_alpha        = true;

    if (t_scaled)
        g_object_unref(t_scaled);

    r_bitmap = t_bitmap;
    return true;
}

////////////////////////////////////////////////////////////////////////////////

// Look up an icon by GIcon in the current GtkIconTheme.
// Returns a new GdkPixbuf reference; caller must g_object_unref().
//
// We use gtk_icon_theme_lookup_icon (available since GTK 2.4) rather than
// gtk_icon_theme_lookup_by_gicon (GTK 2.14+) and GTK_ICON_LOOKUP_GENERIC_FALLBACK
// (GTK 2.12+), as the bundled headers are GTK 2.10.
// For GThemedIcon we iterate the names list ourselves for the same effect.
static GdkPixbuf *s_load_gicon(GIcon *p_gicon, uinteger_t p_size)
{
    if (p_gicon == NULL)
        return NULL;

    GtkIconTheme *t_theme = gtk_icon_theme_get_default();
    if (t_theme == NULL)
        return NULL;

    GtkIconInfo *t_info = NULL;

    if (G_IS_THEMED_ICON(p_gicon))
    {
        // g_themed_icon_get_names returns names in fallback order; try each
        // until we find one the theme knows about.
        const gchar * const *t_names =
            g_themed_icon_get_names(G_THEMED_ICON(p_gicon));
        for (const gchar * const *t_name = t_names;
             *t_name != NULL && t_info == NULL;
             t_name++)
        {
            t_info = gtk_icon_theme_lookup_icon(
                t_theme, *t_name, (gint)p_size,
                GTK_ICON_LOOKUP_USE_BUILTIN);
        }
    }

    if (t_info == NULL)
        return NULL;

    GError *t_error = NULL;
    GdkPixbuf *t_pixbuf = gtk_icon_info_load_icon(t_info, &t_error);
    gtk_icon_info_free(t_info);

    if (t_error)
    {
        g_error_free(t_error);
        return NULL;
    }

    return t_pixbuf;  // caller owns
}

////////////////////////////////////////////////////////////////////////////////

bool MCFileIconGetForFile(MCStringRef p_path,
                          uinteger_t p_size,
                          MCImageBitmap *&r_bitmap)
{
    if (!initialise_weak_link_gio() || !initialise_weak_link_gtk())
        return false;

    MCAutoStringRefAsCString t_cpath;
    if (!t_cpath.Lock(p_path))
        return false;

    // Guess content type from the file name (fast, no I/O).
    // Pass FALSE so GIO doesn't try to sniff the contents.
    gboolean t_uncertain = FALSE;
    gchar *t_content_type =
        g_content_type_guess(*t_cpath, NULL, 0, &t_uncertain);

    if (t_content_type == NULL)
        return false;

    GIcon *t_gicon = g_content_type_get_icon(t_content_type);
    g_free(t_content_type);

    if (t_gicon == NULL)
        return false;

    GdkPixbuf *t_pixbuf = s_load_gicon(t_gicon, p_size);
    g_object_unref(t_gicon);

    if (t_pixbuf == NULL)
        return false;

    bool t_ok = s_pixbuf_to_bitmap(t_pixbuf, p_size, r_bitmap);
    g_object_unref(t_pixbuf);
    return t_ok;
}

bool MCFileIconGetForExtension(MCStringRef p_extension,
                               uinteger_t p_size,
                               MCImageBitmap *&r_bitmap)
{
    if (!initialise_weak_link_gio() || !initialise_weak_link_gtk())
        return false;

    MCAutoStringRefAsCString t_cext;
    if (!t_cext.Lock(p_extension))
        return false;

    // Build a fake filename "dummy.<ext>" so g_content_type_guess can map
    // the extension to a MIME type without touching the file system.
    const char *t_ext_str = *t_cext;
    if (t_ext_str[0] == '.')
        t_ext_str++;   // strip leading dot

    char t_fake_name[256];
    snprintf(t_fake_name, sizeof(t_fake_name), "dummy.%s", t_ext_str);

    gboolean t_uncertain = FALSE;
    gchar *t_content_type =
        g_content_type_guess(t_fake_name, NULL, 0, &t_uncertain);

    if (t_content_type == NULL)
        return false;

    GIcon *t_gicon = g_content_type_get_icon(t_content_type);
    g_free(t_content_type);

    if (t_gicon == NULL)
        return false;

    GdkPixbuf *t_pixbuf = s_load_gicon(t_gicon, p_size);
    g_object_unref(t_gicon);

    if (t_pixbuf == NULL)
        return false;

    bool t_ok = s_pixbuf_to_bitmap(t_pixbuf, p_size, r_bitmap);
    g_object_unref(t_pixbuf);
    return t_ok;
}
