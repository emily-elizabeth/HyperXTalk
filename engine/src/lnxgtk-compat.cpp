/* This is a new implementation, specific to the OpenXTalk project.  */


#include "lnxgtk-compat.h"
#include <stdio.h>
#include <dlfcn.h>

// Static members
MCGTKVersion MCGTKCompat::s_version = MCGTKVersion::NONE;
bool MCGTKCompat::s_initialized = false;

// GTK library handles
static void* s_gtk_lib = nullptr;

// Function to detect GTK version at runtime
static MCGTKVersion DetectGTKVersion()
{
    // -- tperry 15-11-2025: Try GTK3 first (GTK4 is too different, needs complete rewrite)
    // GTK4 removed GdkWindow, GdkScreen, and many other APIs that LiveCode depends on
    s_gtk_lib = dlopen("libgtk-3.so.0", RTLD_LAZY | RTLD_LOCAL);
    if (s_gtk_lib != nullptr)
    {
        fprintf(stderr, "GTK3 library found\n");
        return MCGTKVersion::GTK3;
    }
    
    // Try GTK4 as fallback (but it won't work properly without major code changes)
    s_gtk_lib = dlopen("libgtk-4.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (s_gtk_lib != nullptr)
    {
        fprintf(stderr, "GTK4 library found (WARNING: Limited compatibility, GTK3 recommended)\n");
        return MCGTKVersion::GTK4;
    }
    
    fprintf(stderr, "No GTK3 or GTK4 library found\n");
    return MCGTKVersion::NONE;
}

bool MCGTKCompat::Initialize()
{
    if (s_initialized)
        return true;
    
    // Detect GTK version
    s_version = DetectGTKVersion();
    
    if (s_version == MCGTKVersion::NONE)
    {
        fprintf(stderr, "ERROR: No compatible GTK version found (need GTK3 or GTK4)\n");
        return false;
    }
    
    // Initialize GTK
    int argc = 0;
    char** argv = nullptr;
    
    if (!gtk_init_check(&argc, &argv))
    {
        fprintf(stderr, "ERROR: Failed to initialize GTK\n");
        return false;
    }
    
    s_initialized = true;
    
    fprintf(stderr, "GTK initialized successfully (version: %s)\n", GetVersionString());
    
    return true;
}

MCGTKVersion MCGTKCompat::GetVersion()
{
    return s_version;
}

bool MCGTKCompat::IsInitialized()
{
    return s_initialized;
}

void MCGTKCompat::Shutdown()
{
    if (!s_initialized)
        return;
    
    // GTK3 doesn't have a shutdown function, GTK4 does but it's optional
    // Just mark as uninitialized
    s_initialized = false;
    
    if (s_gtk_lib != nullptr)
    {
        dlclose(s_gtk_lib);
        s_gtk_lib = nullptr;
    }
}

const char* MCGTKCompat::GetVersionString()
{
    switch (s_version)
    {
        case MCGTKVersion::GTK3:
            return "GTK3";
        case MCGTKVersion::GTK4:
            return "GTK4";
        default:
            return "NONE";
    }
}

// Widget creation implementations
namespace MCGTKWidget
{
    GtkWidget* CreateWindow(GtkWindowType type)
    {
        return gtk_window_new(type);
    }
    
    GtkWidget* CreateFixed()
    {
        return gtk_fixed_new();
    }
    
    GtkWidget* CreateBox(GtkOrientation orientation, int spacing)
    {
        // -- tperry 12-11-2025: GTK3/4 modern API
        return gtk_box_new(orientation, spacing);
    }
    
    GtkWidget* CreateButton(const char* label)
    {
        if (label)
            return gtk_button_new_with_label(label);
        else
            return gtk_button_new();
    }
    
    GtkWidget* CreateCheckButton(const char* label)
    {
        if (label)
            return gtk_check_button_new_with_label(label);
        else
            return gtk_check_button_new();
    }
    
    GtkWidget* CreateRadioButton(GSList* group, const char* label)
    {
        if (label)
            return gtk_radio_button_new_with_label(group, label);
        else
            return gtk_radio_button_new(group);
    }
    
    GtkWidget* CreateEntry()
    {
        return gtk_entry_new();
    }
    
    GtkWidget* CreateTextView()
    {
        return gtk_text_view_new();
    }
    
    GtkWidget* CreateScrollbar(GtkOrientation orientation)
    {
        // -- tperry 12-11-2025: GTK3/4 modern API
        return gtk_scrollbar_new(orientation, nullptr);
    }
    
    GtkWidget* CreateScale(GtkOrientation orientation)
    {
        // -- tperry 12-11-2025: GTK3/4 modern API
        return gtk_scale_new(orientation, nullptr);
    }
    
    GtkWidget* CreateProgressBar()
    {
        return gtk_progress_bar_new();
    }
    
    GtkWidget* CreateComboBox()
    {
        return gtk_combo_box_new();
    }
    
    GtkWidget* CreateTreeView()
    {
        return gtk_tree_view_new();
    }
    
    GtkWidget* CreateMenuItem(const char* label)
    {
        if (label)
            return gtk_menu_item_new_with_label(label);
        else
            return gtk_menu_item_new();
    }
    
    GtkWidget* CreateMenuBar()
    {
        return gtk_menu_bar_new();
    }
    
    GtkWidget* CreateMenu()
    {
        return gtk_menu_new();
    }
    
    GtkWidget* CreateDrawingArea()
    {
        return gtk_drawing_area_new();
    }
    
    void Show(GtkWidget* widget)
    {
        gtk_widget_show(widget);
    }
    
    void Hide(GtkWidget* widget)
    {
        gtk_widget_hide(widget);
    }
    
    void ShowAll(GtkWidget* widget)
    {
        gtk_widget_show_all(widget);
    }
    
    void Destroy(GtkWidget* widget)
    {
        gtk_widget_destroy(widget);
    }
    
    void SetSizeRequest(GtkWidget* widget, int width, int height)
    {
        gtk_widget_set_size_request(widget, width, height);
    }
    
    void GetSize(GtkWidget* widget, int* width, int* height)
    {
        GtkAllocation allocation;
        gtk_widget_get_allocation(widget, &allocation);
        if (width)
            *width = allocation.width;
        if (height)
            *height = allocation.height;
    }
    
    void ContainerAdd(GtkWidget* container, GtkWidget* child)
    {
        gtk_container_add(GTK_CONTAINER(container), child);
    }
    
    void FixedPut(GtkWidget* fixed, GtkWidget* widget, int x, int y)
    {
        gtk_fixed_put(GTK_FIXED(fixed), widget, x, y);
    }
    
    void FixedMove(GtkWidget* fixed, GtkWidget* widget, int x, int y)
    {
        gtk_fixed_move(GTK_FIXED(fixed), widget, x, y);
    }
    
    void WindowSetTitle(GtkWidget* window, const char* title)
    {
        gtk_window_set_title(GTK_WINDOW(window), title);
    }
    
    void WindowSetDefaultSize(GtkWidget* window, int width, int height)
    {
        gtk_window_set_default_size(GTK_WINDOW(window), width, height);
    }
    
    void WindowMove(GtkWidget* window, int x, int y)
    {
        gtk_window_move(GTK_WINDOW(window), x, y);
    }
    
    void WindowResize(GtkWidget* window, int width, int height)
    {
        gtk_window_resize(GTK_WINDOW(window), width, height);
    }
    
    void WindowSetResizable(GtkWidget* window, bool resizable)
    {
        gtk_window_set_resizable(GTK_WINDOW(window), resizable ? TRUE : FALSE);
    }
    
    void WindowSetDecorated(GtkWidget* window, bool decorated)
    {
        gtk_window_set_decorated(GTK_WINDOW(window), decorated ? TRUE : FALSE);
    }
    
    GdkWindow* GetGdkWindow(GtkWidget* widget)
    {
        return gtk_widget_get_window(widget);
    }
}

// Signal handling implementations
namespace MCGTKSignal
{
    gulong Connect(GtkWidget* widget, const char* signal, GCallback callback, gpointer data)
    {
        return g_signal_connect(widget, signal, callback, data);
    }
    
    void Disconnect(GtkWidget* widget, gulong handler_id)
    {
        g_signal_handler_disconnect(widget, handler_id);
    }
    
    void Block(GtkWidget* widget, gulong handler_id)
    {
        g_signal_handler_block(widget, handler_id);
    }
    
    void Unblock(GtkWidget* widget, gulong handler_id)
    {
        g_signal_handler_unblock(widget, handler_id);
    }
}

// Main loop implementations
namespace MCGTKMainLoop
{
    bool ProcessEvents()
    {
        while (gtk_events_pending())
        {
            gtk_main_iteration_do(FALSE);
        }
        return true;
    }
    
    bool HasPendingEvents()
    {
        return gtk_events_pending() != 0;
    }
    
    void Iteration(bool blocking)
    {
        gtk_main_iteration_do(blocking ? TRUE : FALSE);
    }
}
