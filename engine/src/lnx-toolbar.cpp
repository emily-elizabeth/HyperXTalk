//
// Linux GTK toolbar backend for MCToolbar.
// Uses GtkToolbar (GTK 3). On GTK 4 this class is a no-op stub —
// a GtkBox+GtkButton implementation can replace it later.
//

#ifdef TARGET_PLATFORM_LINUX

#include <gtk/gtk.h>

#include "prefix.h"
#include "toolbar.h"

////////////////////////////////////////////////////////////////////////////////

struct LnxToolbarItemData
{
    MCNewAutoNameRef   name;
    GtkToolItem    *widget;   // nil for separator/space items
};

////////////////////////////////////////////////////////////////////////////////
// GTK image loader: decode PNG bytes via GdkPixbufLoader

static GdkPixbuf *_pixbufFromPNGData(const void *p_bytes, uindex_t p_length)
{
    if (!p_bytes || p_length == 0)
        return NULL;

    GdkPixbufLoader *t_loader = gdk_pixbuf_loader_new_with_type("png", NULL);
    if (!t_loader)
        return NULL;

    GError *t_err = NULL;
    if (!gdk_pixbuf_loader_write(t_loader,
                                 (const guchar *)p_bytes,
                                 (gsize)p_length,
                                 &t_err))
    {
        if (t_err) g_error_free(t_err);
        gdk_pixbuf_loader_close(t_loader, NULL);
        g_object_unref(t_loader);
        return NULL;
    }
    gdk_pixbuf_loader_close(t_loader, NULL);

    GdkPixbuf *t_pixbuf = gdk_pixbuf_loader_get_pixbuf(t_loader);
    if (t_pixbuf)
        g_object_ref(t_pixbuf); // caller owns this ref
    g_object_unref(t_loader);
    return t_pixbuf;
}

////////////////////////////////////////////////////////////////////////////////
// UTF-8 RAII helper: wraps MCStringConvertToUTF8String / MCMemoryDelete.
// GTK uses UTF-8 exclusively; this ensures correct output regardless of the
// system locale setting.

struct MCAutoUTF8String
{
    char *m_str;
    MCAutoUTF8String() : m_str(nil) {}
    ~MCAutoUTF8String() { MCMemoryDelete(m_str); }

    bool Lock(MCStringRef p_src)
    {
        MCMemoryDelete(m_str);
        m_str = nil;
        if (!p_src || MCStringIsEmpty(p_src))
            return false;
        return MCStringConvertToUTF8String(p_src, m_str);
    }

    const char *operator*() const { return m_str ? m_str : ""; }
};

////////////////////////////////////////////////////////////////////////////////

class MCToolbarLinuxBackend : public MCToolbarBackend
{
public:
    MCToolbarLinuxBackend(MCToolbar *p_owner)
        : m_owner(p_owner), m_toolbar(NULL), m_parent(NULL),
          m_item_count(0), m_visible(true),
          m_display_mode(kMCToolbarDisplayModeDefault)
    {
    }

    ~MCToolbarLinuxBackend() override {}

    void Create(void *p_window_handle) override
    {
#if GTK_MAJOR_VERSION < 4
        m_parent = (GtkWidget *)p_window_handle;
        if (!m_parent)
            return;

        m_toolbar = gtk_toolbar_new();
        if (!m_toolbar)
            return;

        _applyDisplayMode();

        // Pack the toolbar at the top of the parent window's vbox.
        // The engine's Linux window is a GtkWindow with a GtkVBox as the
        // top-level container.
        GtkWidget *t_child = gtk_bin_get_child(GTK_BIN(m_parent));
        if (t_child && GTK_IS_BOX(t_child))
        {
            gtk_box_pack_start(GTK_BOX(t_child), m_toolbar,
                               FALSE, FALSE, 0);
            gtk_box_reorder_child(GTK_BOX(t_child), m_toolbar, 0);
        }
        else
        {
            // Fallback: add directly to window
            gtk_container_add(GTK_CONTAINER(m_parent), m_toolbar);
        }

        if (m_visible)
            gtk_widget_show(m_toolbar);
        else
            gtk_widget_hide(m_toolbar);
#endif
    }

    void Destroy() override
    {
#if GTK_MAJOR_VERSION < 4
        if (m_toolbar)
        {
            gtk_widget_destroy(m_toolbar);
            m_toolbar = NULL;
        }
        for (int i = 0; i < m_item_count; i++)
            m_items[i].~LnxToolbarItemData();
        m_item_count = 0;
#endif
    }

    void AddItem(const MCToolbarItem *p_item) override
    {
#if GTK_MAJOR_VERSION < 4
        if (!m_toolbar || m_item_count >= 256)
            return;

        int t_idx = m_item_count;
        new (&m_items[t_idx]) LnxToolbarItemData();
        m_items[t_idx].name.Reset(p_item->GetName());

        GtkToolItem *t_item = NULL;
        MCToolbarItemStyle t_style = p_item->GetStyle();

        if (t_style == kMCToolbarItemStyleSeparator)
        {
            t_item = gtk_separator_tool_item_new();
        }
        else if (t_style == kMCToolbarItemStyleSpace ||
                 t_style == kMCToolbarItemStyleFlexSpace)
        {
            t_item = gtk_separator_tool_item_new();
            gtk_separator_tool_item_set_draw(
                GTK_SEPARATOR_TOOL_ITEM(t_item), FALSE);
            if (t_style == kMCToolbarItemStyleFlexSpace)
                gtk_tool_item_set_expand(t_item, TRUE);
        }
        else
        {
            // Label
            MCAutoUTF8String t_label_utf8;
            const char *t_label_cstr = "";
            if (p_item->GetLabel() && !MCStringIsEmpty(p_item->GetLabel()))
                if (t_label_utf8.Lock(p_item->GetLabel()))
                    t_label_cstr = *t_label_utf8;

            GtkToolButton *t_button = GTK_TOOL_BUTTON(
                gtk_tool_button_new(NULL, t_label_cstr));

            // Icon: prefer cached PNG data from the stack image
            MCDataRef t_img_data = p_item->GetImageData();
            if (t_img_data != nil && !MCDataIsEmpty(t_img_data))
            {
                GdkPixbuf *t_pixbuf = _pixbufFromPNGData(
                    MCDataGetBytePtr(t_img_data),
                    (uindex_t)MCDataGetLength(t_img_data));
                if (t_pixbuf)
                {
                    GtkWidget *t_icon = gtk_image_new_from_pixbuf(t_pixbuf);
                    g_object_unref(t_pixbuf);
                    gtk_tool_button_set_icon_widget(t_button, t_icon);
                    gtk_widget_show(t_icon);
                }
            }
            else if (p_item->GetIcon() && !MCStringIsEmpty(p_item->GetIcon()))
            {
                // Fallback: XDG theme icon name
                MCAutoUTF8String t_icon_utf8;
                if (t_icon_utf8.Lock(p_item->GetIcon()))
                {
                    GtkWidget *t_icon = gtk_image_new_from_icon_name(
                        *t_icon_utf8, GTK_ICON_SIZE_LARGE_TOOLBAR);
                    if (t_icon)
                    {
                        gtk_tool_button_set_icon_widget(t_button, t_icon);
                        gtk_widget_show(t_icon);
                    }
                }
            }

            t_item = GTK_TOOL_ITEM(t_button);

            if (!p_item->GetEnabled())
                gtk_widget_set_sensitive(GTK_WIDGET(t_item), FALSE);

            // Tooltip — skipped; bundled GTK 2 headers predate gtk_widget_set_tooltip_text

            // Store the item name with the button widget so the click callback
            // can look it up by name rather than by array index.  The index-
            // based approach breaks after RemoveItem shifts the array.
            g_object_set_data_full(G_OBJECT(t_button), "item-name",
                                   (gpointer)MCValueRetain(p_item->GetName()),
                                   (GDestroyNotify)MCValueRelease);
            g_object_set_data(G_OBJECT(t_button), "backend", (gpointer)this);
            g_signal_connect(t_button, "clicked",
                             G_CALLBACK(_onItemClicked), NULL);
        }

        m_items[t_idx].widget = t_item;
        gtk_toolbar_insert(GTK_TOOLBAR(m_toolbar), t_item, -1);
        gtk_widget_show(GTK_WIDGET(t_item));
        m_item_count++;
#endif
    }

    void RemoveItem(MCNameRef p_name) override
    {
#if GTK_MAJOR_VERSION < 4
        if (!m_toolbar)
            return;

        for (int i = 0; i < m_item_count; i++)
        {
            if (MCNameIsEqualTo(*m_items[i].name, p_name,
                                kMCCompareCaseless))
            {
                if (m_items[i].widget)
                {
                    gtk_container_remove(GTK_CONTAINER(m_toolbar),
                                        GTK_WIDGET(m_items[i].widget));
                }
                m_items[i].~LnxToolbarItemData();

                for (int j = i; j < m_item_count - 1; j++)
                {
                    m_items[j].name.Reset(*m_items[j + 1].name);
                    m_items[j].widget = m_items[j + 1].widget;
                }

                m_items[m_item_count - 1].~LnxToolbarItemData();
                new (&m_items[m_item_count - 1]) LnxToolbarItemData();

                m_item_count--;
                return;
            }
        }
#endif
    }

    void UpdateItem(const MCToolbarItem *p_item) override
    {
#if GTK_MAJOR_VERSION < 4
        for (int i = 0; i < m_item_count; i++)
        {
            if (MCNameIsEqualTo(*m_items[i].name, p_item->GetName(),
                                kMCCompareCaseless))
            {
                GtkToolItem *t_widget = m_items[i].widget;
                if (!t_widget)
                    return;

                gtk_widget_set_sensitive(GTK_WIDGET(t_widget),
                                        p_item->GetEnabled() ? TRUE : FALSE);

                if (GTK_IS_TOOL_BUTTON(t_widget))
                {
                    GtkToolButton *t_btn = GTK_TOOL_BUTTON(t_widget);

                    // Label
                    if (p_item->GetLabel())
                    {
                        MCAutoUTF8String t_lbl;
                        if (t_lbl.Lock(p_item->GetLabel()))
                            gtk_tool_button_set_label(t_btn, *t_lbl);
                    }

                    // Tooltip — skipped; bundled GTK 2 headers predate gtk_widget_set_tooltip_text

                    // Icon: reload from cached PNG data
                    MCDataRef t_img_data = p_item->GetImageData();
                    if (t_img_data != nil && !MCDataIsEmpty(t_img_data))
                    {
                        GdkPixbuf *t_pixbuf = _pixbufFromPNGData(
                            MCDataGetBytePtr(t_img_data),
                            (uindex_t)MCDataGetLength(t_img_data));
                        if (t_pixbuf)
                        {
                            GtkWidget *t_icon =
                                gtk_image_new_from_pixbuf(t_pixbuf);
                            g_object_unref(t_pixbuf);
                            gtk_tool_button_set_icon_widget(t_btn, t_icon);
                            gtk_widget_show(t_icon);
                        }
                    }
                    else if (p_item->GetIcon() && !MCStringIsEmpty(p_item->GetIcon()))
                    {
                        // Fallback: XDG theme icon name
                        MCAutoUTF8String t_icon_utf8;
                        if (t_icon_utf8.Lock(p_item->GetIcon()))
                        {
                            GtkWidget *t_icon = gtk_image_new_from_icon_name(
                                *t_icon_utf8, GTK_ICON_SIZE_LARGE_TOOLBAR);
                            if (t_icon)
                            {
                                gtk_tool_button_set_icon_widget(t_btn, t_icon);
                                gtk_widget_show(t_icon);
                            }
                        }
                    }
                    else
                    {
                        // Icon was cleared
                        gtk_tool_button_set_icon_widget(t_btn, NULL);
                    }
                }
                return;
            }
        }
#endif
    }

    void ClearItems() override
    {
#if GTK_MAJOR_VERSION < 4
        if (!m_toolbar)
            return;

        for (int i = 0; i < m_item_count; i++)
        {
            if (m_items[i].widget)
            {
                gtk_container_remove(GTK_CONTAINER(m_toolbar),
                                    GTK_WIDGET(m_items[i].widget));
            }
            m_items[i].~LnxToolbarItemData();
        }
        m_item_count = 0;
#endif
    }

    void SetDisplayMode(MCToolbarDisplayMode p_mode) override
    {
        m_display_mode = p_mode;
#if GTK_MAJOR_VERSION < 4
        if (m_toolbar)
            _applyDisplayMode();
#endif
    }

    void SetVisible(bool p_visible) override
    {
        m_visible = p_visible;
#if GTK_MAJOR_VERSION < 4
        if (m_toolbar)
        {
            if (p_visible)
                gtk_widget_show(m_toolbar);
            else
                gtk_widget_hide(m_toolbar);
        }
#endif
    }

    bool GetVisible() override
    {
        return m_visible;
    }

private:
    MCToolbar              *m_owner;
    GtkWidget              *m_toolbar;
    GtkWidget              *m_parent;
    int                     m_item_count;
    bool                    m_visible;
    MCToolbarDisplayMode    m_display_mode;
    LnxToolbarItemData      m_items[256];

    void _applyDisplayMode()
    {
#if GTK_MAJOR_VERSION < 4
        if (!m_toolbar)
            return;
        GtkToolbarStyle t_style;
        switch (m_display_mode)
        {
            case kMCToolbarDisplayModeIconOnly:
                t_style = GTK_TOOLBAR_ICONS;
                break;
            case kMCToolbarDisplayModeLabelOnly:
                t_style = GTK_TOOLBAR_TEXT;
                break;
            case kMCToolbarDisplayModeIconAndLabel:
            case kMCToolbarDisplayModeDefault:
            default:
                t_style = GTK_TOOLBAR_BOTH;
                break;
        }
        gtk_toolbar_set_style(GTK_TOOLBAR(m_toolbar), t_style);
#endif
    }

    // Click callback: reads the item name stored on the button widget so the
    // lookup is immune to array index shifts caused by RemoveItem.
    static void _onItemClicked(GtkToolButton *button, gpointer /*user_data*/)
    {
        MCToolbarLinuxBackend *t_self =
            (MCToolbarLinuxBackend *)g_object_get_data(
                G_OBJECT(button), "backend");
        MCNameRef t_name =
            (MCNameRef)g_object_get_data(G_OBJECT(button), "item-name");

        if (t_self && t_name && t_self->m_owner)
            t_self->m_owner->itemClicked(t_name);
    }
};

////////////////////////////////////////////////////////////////////////////////
// Factory

MCToolbarBackend *MCToolbarCreatePlatformBackend(MCToolbar *p_owner)
{
    return new MCToolbarLinuxBackend(p_owner);
}

#endif // TARGET_PLATFORM_LINUX
