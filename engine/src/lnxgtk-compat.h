/* This is a new implementation, specific to the OpenXTalk project.  */
//
// GTK3/4 Compatibility Layer
// Provides runtime detection and abstraction for GTK3 vs GTK4
//

#ifndef __LNXGTK_COMPAT_H__
#define __LNXGTK_COMPAT_H__

#include <gtk/gtk.h>

// GTK version enumeration
enum class MCGTKVersion
{
    NONE,
    GTK3,
    GTK4
};

// GTK compatibility layer
class MCGTKCompat
{
public:
    // Initialize GTK and detect version
    // Returns true if GTK was successfully initialized
    static bool Initialize();
    
    // Get the detected GTK version
    static MCGTKVersion GetVersion();
    
    // Check if GTK is initialized
    static bool IsInitialized();
    
    // Shutdown GTK
    static void Shutdown();
    
    // Get version string for debugging
    static const char* GetVersionString();
    
private:
    static MCGTKVersion s_version;
    static bool s_initialized;
};

// Widget creation helpers that work with both GTK3 and GTK4
namespace MCGTKWidget
{
    // Window creation
    GtkWidget* CreateWindow(GtkWindowType type);
    
    // Container widgets
    GtkWidget* CreateFixed();
    GtkWidget* CreateBox(GtkOrientation orientation, int spacing);
    
    // Control widgets
    GtkWidget* CreateButton(const char* label);
    GtkWidget* CreateCheckButton(const char* label);
    GtkWidget* CreateRadioButton(GSList* group, const char* label);
    GtkWidget* CreateEntry();
    GtkWidget* CreateTextView();
    GtkWidget* CreateScrollbar(GtkOrientation orientation);
    GtkWidget* CreateScale(GtkOrientation orientation);
    GtkWidget* CreateProgressBar();
    GtkWidget* CreateComboBox();
    GtkWidget* CreateTreeView();
    GtkWidget* CreateMenuItem(const char* label);
    GtkWidget* CreateMenuBar();
    GtkWidget* CreateMenu();
    
    // Drawing area for custom graphics
    GtkWidget* CreateDrawingArea();
    
    // Widget operations
    void Show(GtkWidget* widget);
    void Hide(GtkWidget* widget);
    void ShowAll(GtkWidget* widget);
    void Destroy(GtkWidget* widget);
    
    // Size and position
    void SetSizeRequest(GtkWidget* widget, int width, int height);
    void GetSize(GtkWidget* widget, int* width, int* height);
    
    // Container operations
    void ContainerAdd(GtkWidget* container, GtkWidget* child);
    void FixedPut(GtkWidget* fixed, GtkWidget* widget, int x, int y);
    void FixedMove(GtkWidget* fixed, GtkWidget* widget, int x, int y);
    
    // Window operations
    void WindowSetTitle(GtkWidget* window, const char* title);
    void WindowSetDefaultSize(GtkWidget* window, int width, int height);
    void WindowMove(GtkWidget* window, int x, int y);
    void WindowResize(GtkWidget* window, int width, int height);
    void WindowSetResizable(GtkWidget* window, bool resizable);
    void WindowSetDecorated(GtkWidget* window, bool decorated);
    
    // Get underlying GdkWindow (for X11 compatibility)
    GdkWindow* GetGdkWindow(GtkWidget* widget);
}

// Event handling helpers
namespace MCGTKSignal
{
    // Connect signal handlers
    gulong Connect(GtkWidget* widget, const char* signal, GCallback callback, gpointer data);
    void Disconnect(GtkWidget* widget, gulong handler_id);
    
    // Block/unblock signals
    void Block(GtkWidget* widget, gulong handler_id);
    void Unblock(GtkWidget* widget, gulong handler_id);
}

// Main loop integration
namespace MCGTKMainLoop
{
    // Process pending GTK events (non-blocking)
    bool ProcessEvents();
    
    // Check if there are pending events
    bool HasPendingEvents();
    
    // Run main loop iteration
    void Iteration(bool blocking);
}

#endif // __LNXGTK_COMPAT_H__
