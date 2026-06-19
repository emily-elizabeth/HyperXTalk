/* This is a new implementation, specific to the OpenXTalk project.  */

//
// GTK Window Wrapper
// Wraps GtkWindow to provide LiveCode stack window functionality
//

#ifndef __LNXGTK_WINDOW_H__
#define __LNXGTK_WINDOW_H__

#include "lnxgtk-compat.h"

// Forward declarations
class MCStack;

// GTK Window wrapper for LiveCode stacks
class MCGTKWindow
{
public:
    MCGTKWindow();
    ~MCGTKWindow();
    
    // Create the window
    bool Create(MCStack* stack, int x, int y, int width, int height);
    
    // Destroy the window
    void Destroy();
    
    // Show/hide
    void Show();
    void Hide();
    
    // Position and size
    void Move(int x, int y);
    void Resize(int width, int height);
    void SetGeometry(int x, int y, int width, int height);
    void GetGeometry(int& x, int& y, int& width, int& height);
    
    // Window properties
    void SetTitle(const char* title);
    void SetResizable(bool resizable);
    void SetDecorated(bool decorated);
    
    // Get GTK widget
    GtkWidget* GetGtkWindow() { return m_gtk_window; }
    GtkWidget* GetContainer() { return m_container; }
    GtkWidget* GetCanvas() { return m_canvas; }
    
    // Get underlying GdkWindow (for X11 compatibility during transition)
    GdkWindow* GetGdkWindow();
    
    // Add a native widget to the window
    void AddWidget(GtkWidget* widget, int x, int y);
    void MoveWidget(GtkWidget* widget, int x, int y);
    void RemoveWidget(GtkWidget* widget);
    
private:
    // GTK widgets
    GtkWidget* m_gtk_window;    // The GtkWindow
    GtkWidget* m_container;     // GtkFixed container for absolute positioning
    GtkWidget* m_canvas;        // GtkDrawingArea for LiveCode graphics
    
    // Associated LiveCode stack
    MCStack* m_stack;
    
    // Signal handlers
    gulong m_delete_handler;
    gulong m_configure_handler;
    gulong m_draw_handler;
    
    // Static signal callbacks
    static gboolean OnDelete(GtkWidget* widget, GdkEvent* event, gpointer data);
    static gboolean OnConfigure(GtkWidget* widget, GdkEventConfigure* event, gpointer data);
    static gboolean OnDraw(GtkWidget* widget, cairo_t* cr, gpointer data);
};

#endif // __LNXGTK_WINDOW_H__
