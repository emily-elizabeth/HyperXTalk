/* This is a new implementation, specific to the OpenXTalk project.  */

#include "lnxprefix.h"
#include "lnxgtk-window.h"
#include "stack.h"
#include <stdio.h>

MCGTKWindow::MCGTKWindow()
    : m_gtk_window(nullptr)
    , m_container(nullptr)
    , m_canvas(nullptr)
    , m_stack(nullptr)
    , m_delete_handler(0)
    , m_configure_handler(0)
    , m_draw_handler(0)
{
}

MCGTKWindow::~MCGTKWindow()
{
    Destroy();
}

bool MCGTKWindow::Create(MCStack* stack, int x, int y, int width, int height)
{
    if (m_gtk_window != nullptr)
    {
        fprintf(stderr, "MCGTKWindow::Create - window already exists\n");
        return false;
    }
    
    m_stack = stack;
    
    // Create the top-level window
    m_gtk_window = MCGTKWidget::CreateWindow(GTK_WINDOW_TOPLEVEL);
    if (m_gtk_window == nullptr)
    {
        fprintf(stderr, "MCGTKWindow::Create - failed to create GtkWindow\n");
        return false;
    }
    
    // Set initial size and position
    MCGTKWidget::WindowSetDefaultSize(m_gtk_window, width, height);
    MCGTKWidget::WindowMove(m_gtk_window, x, y);
    
    // Create container for absolute positioning (like LiveCode expects)
    m_container = MCGTKWidget::CreateFixed();
    if (m_container == nullptr)
    {
        fprintf(stderr, "MCGTKWindow::Create - failed to create container\n");
        MCGTKWidget::Destroy(m_gtk_window);
        m_gtk_window = nullptr;
        return false;
    }
    
    // Add container to window
    MCGTKWidget::ContainerAdd(m_gtk_window, m_container);
    
    // Create drawing area for LiveCode's custom graphics
    m_canvas = MCGTKWidget::CreateDrawingArea();
    if (m_canvas == nullptr)
    {
        fprintf(stderr, "MCGTKWindow::Create - failed to create canvas\n");
        MCGTKWidget::Destroy(m_gtk_window);
        m_gtk_window = nullptr;
        m_container = nullptr;
        return false;
    }
    
    // Canvas fills the entire window
    MCGTKWidget::SetSizeRequest(m_canvas, width, height);
    MCGTKWidget::FixedPut(m_container, m_canvas, 0, 0);
    
    // Connect signal handlers
    m_delete_handler = MCGTKSignal::Connect(m_gtk_window, "delete-event", 
                                            G_CALLBACK(OnDelete), this);
    m_configure_handler = MCGTKSignal::Connect(m_gtk_window, "configure-event",
                                               G_CALLBACK(OnConfigure), this);
    
    // -- tperry 12-11-2025: GTK4 uses "render" signal, GTK3 uses "draw"
    // For now, try GTK3's "draw" signal (GTK4 will need different approach)
    m_draw_handler = MCGTKSignal::Connect(m_canvas, "draw",
                                          G_CALLBACK(OnDraw), this);
    
    fprintf(stderr, "MCGTKWindow::Create - window created successfully (%dx%d at %d,%d)\n",
            width, height, x, y);
    
    return true;
}

void MCGTKWindow::Destroy()
{
    if (m_gtk_window == nullptr)
        return;
    
    // Disconnect signal handlers
    if (m_delete_handler != 0)
    {
        MCGTKSignal::Disconnect(m_gtk_window, m_delete_handler);
        m_delete_handler = 0;
    }
    if (m_configure_handler != 0)
    {
        MCGTKSignal::Disconnect(m_gtk_window, m_configure_handler);
        m_configure_handler = 0;
    }
    if (m_draw_handler != 0)
    {
        MCGTKSignal::Disconnect(m_canvas, m_draw_handler);
        m_draw_handler = 0;
    }
    
    // Destroy the window (this will destroy all children too)
    MCGTKWidget::Destroy(m_gtk_window);
    
    m_gtk_window = nullptr;
    m_container = nullptr;
    m_canvas = nullptr;
    m_stack = nullptr;
}

void MCGTKWindow::Show()
{
    if (m_gtk_window != nullptr)
    {
        MCGTKWidget::ShowAll(m_gtk_window);
    }
}

void MCGTKWindow::Hide()
{
    if (m_gtk_window != nullptr)
    {
        MCGTKWidget::Hide(m_gtk_window);
    }
}

void MCGTKWindow::Move(int x, int y)
{
    if (m_gtk_window != nullptr)
    {
        MCGTKWidget::WindowMove(m_gtk_window, x, y);
    }
}

void MCGTKWindow::Resize(int width, int height)
{
    if (m_gtk_window != nullptr)
    {
        MCGTKWidget::WindowResize(m_gtk_window, width, height);
        
        // Also resize the canvas
        if (m_canvas != nullptr)
        {
            MCGTKWidget::SetSizeRequest(m_canvas, width, height);
        }
    }
}

void MCGTKWindow::SetGeometry(int x, int y, int width, int height)
{
    Move(x, y);
    Resize(width, height);
}

void MCGTKWindow::GetGeometry(int& x, int& y, int& width, int& height)
{
    if (m_gtk_window != nullptr)
    {
        // Get position
        gtk_window_get_position(GTK_WINDOW(m_gtk_window), &x, &y);
        
        // Get size
        MCGTKWidget::GetSize(m_gtk_window, &width, &height);
    }
}

void MCGTKWindow::SetTitle(const char* title)
{
    if (m_gtk_window != nullptr && title != nullptr)
    {
        MCGTKWidget::WindowSetTitle(m_gtk_window, title);
    }
}

void MCGTKWindow::SetResizable(bool resizable)
{
    if (m_gtk_window != nullptr)
    {
        MCGTKWidget::WindowSetResizable(m_gtk_window, resizable);
    }
}

void MCGTKWindow::SetDecorated(bool decorated)
{
    if (m_gtk_window != nullptr)
    {
        MCGTKWidget::WindowSetDecorated(m_gtk_window, decorated);
    }
}

GdkWindow* MCGTKWindow::GetGdkWindow()
{
    if (m_gtk_window != nullptr)
    {
        return MCGTKWidget::GetGdkWindow(m_gtk_window);
    }
    return nullptr;
}

void MCGTKWindow::AddWidget(GtkWidget* widget, int x, int y)
{
    if (m_container == nullptr || widget == nullptr)
        return;
    if (!GTK_IS_FIXED(m_container) || !GTK_IS_WIDGET(widget))
        return;
    MCGTKWidget::FixedPut(m_container, widget, x, y);
    MCGTKWidget::Show(widget);
}

void MCGTKWindow::MoveWidget(GtkWidget* widget, int x, int y)
{
    if (m_container != nullptr && widget != nullptr)
    {
        MCGTKWidget::FixedMove(m_container, widget, x, y);
    }
}

void MCGTKWindow::RemoveWidget(GtkWidget* widget)
{
    if (m_container != nullptr && widget != nullptr)
    {
        gtk_container_remove(GTK_CONTAINER(m_container), widget);
    }
}

// Static signal callbacks

gboolean MCGTKWindow::OnDelete(GtkWidget* widget, GdkEvent* event, gpointer data)
{
    MCGTKWindow* window = static_cast<MCGTKWindow*>(data);
    
    fprintf(stderr, "MCGTKWindow::OnDelete - window close requested\n");
    
    // TODO: Forward to LiveCode stack's close handler
    // For now, just hide the window
    if (window && window->m_stack)
    {
        // window->m_stack->close();
    }
    
    // Return TRUE to prevent default handler (which would destroy the window)
    return TRUE;
}

gboolean MCGTKWindow::OnConfigure(GtkWidget* widget, GdkEventConfigure* event, gpointer data)
{
    MCGTKWindow* window = static_cast<MCGTKWindow*>(data);
    
    fprintf(stderr, "MCGTKWindow::OnConfigure - window resized/moved to %dx%d at %d,%d\n",
            event->width, event->height, event->x, event->y);
    
    // Resize canvas to match window
    if (window && window->m_canvas)
    {
        MCGTKWidget::SetSizeRequest(window->m_canvas, event->width, event->height);
    }
    
    // TODO: Forward to LiveCode stack's resize handler
    if (window && window->m_stack)
    {
        // window->m_stack->resize(event->width, event->height);
    }
    
    return FALSE;
}

gboolean MCGTKWindow::OnDraw(GtkWidget* widget, cairo_t* cr, gpointer data)
{
    MCGTKWindow* window = static_cast<MCGTKWindow*>(data);
    
    // TODO: Forward to LiveCode's rendering system
    // For now, just draw a simple background
    
    // Get widget size
    int width, height;
    MCGTKWidget::GetSize(widget, &width, &height);
    
    // Draw white background
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);
    
    // Draw some test graphics
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 2.0);
    cairo_rectangle(cr, 10, 10, width - 20, height - 20);
    cairo_stroke(cr);
    
    // Draw text
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 24.0);
    cairo_move_to(cr, 20, 50);
    cairo_show_text(cr, "GTK3/4 Window Prototype");
    
    return FALSE;
}
