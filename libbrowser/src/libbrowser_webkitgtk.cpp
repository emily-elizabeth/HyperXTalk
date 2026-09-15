/* Copyright (C) 2026 HyperXTalk contributors.

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

// WebKitGTK browser backend for Linux.
//
// Design notes
// ------------
// Library loading: all WebKit/JSC/GTK symbols are resolved at runtime via
// dlopen()/dlsym() into the wk{} struct below.  This avoids a hard link-time
// dependency on webkit2gtk and works around the "two GObject type tables"
// crash: the engine statically links GLib/GTK, so loading a bundled WebKit
// that brings its own shared GLib would register duplicate types.  We therefore
// try SYSTEM webkit2gtk-4.1 first, fall back to 4.0, then bundled.
//
// Embedding: WebKitWebView is housed in a GTK_WINDOW_TOPLEVEL container.
// native-layer-x11.cpp embeds it via gtk_socket_add_id().
// GetNativeLayer() returns the container window's X11 XID.
//
// NOTE: We do NOT use GtkPlug (XEMBED client) because GtkPlug ignores raw
// XMoveResizeWindow / ConfigureNotify events — it only resizes when GtkSocket
// sends XEMBED_SIZE_CHANGE.  A plain GTK_WINDOW_TOPLEVEL processes
// ConfigureNotify normally, so native-layer-x11's XMoveResizeWindow correctly
// triggers a WebKit layout at the right size.
//
// Event pumping: WebKit2GTK drives rendering and IPC through GLib's main
// context.  We register MCLinuxWebKitGTKRunloopAction via
// MCBrowserAddRunloopAction so the GTK main context is iterated on every
// engine runloop tick.  Without this WebKit stalls and nothing renders.
//
// JavaScript handlers: UserContentManager + window.webkit.messageHandlers
// injection.  The script-message-received signal passes a JSCValue* (4.1) or
// WebKitJavascriptResult* (4.0); we detect which API is present at dlsym
// time and handle accordingly.

#include "core.h"
#include "libbrowser_internal.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

// Pull in GTK/GDK/X11 type definitions only — we do NOT link against these
// directly; symbol addresses come from dlsym().
#include <gtk/gtk.h>
#include <gdk/gdkx.h>   // gdk_x11_window_get_xid, Window type

////////////////////////////////////////////////////////////////////////////////
// WebKit / JSC opaque type aliases

typedef struct _WebKitWebView            WebKitWebView;
typedef struct _WebKitSettings           WebKitSettings;
typedef struct _WebKitWebContext         WebKitWebContext;
typedef struct _WebKitUserContentManager WebKitUserContentManager;
typedef struct _WebKitUserScript         WebKitUserScript;
typedef struct _WebKitPolicyDecision     WebKitPolicyDecision;
typedef struct _WebKitNavigationPolicyDecision WebKitNavigationPolicyDecision;
typedef struct _WebKitNavigationAction   WebKitNavigationAction;
typedef struct _WebKitURIRequest         WebKitURIRequest;
typedef struct _WebKitJavascriptResult   WebKitJavascriptResult; // 4.0 only
typedef struct _JSCValue                 JSCValue;
typedef struct _WebKitOptionMenu         WebKitOptionMenu;
typedef struct _WebKitOptionMenuItem     WebKitOptionMenuItem;

// WebKitLoadEvent
enum {
    WEBKIT_LOAD_STARTED    = 0,
    WEBKIT_LOAD_REDIRECTED = 1,
    WEBKIT_LOAD_COMMITTED  = 2,
    WEBKIT_LOAD_FINISHED   = 3,
};

// WebKitPolicyDecisionType
enum {
    WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION  = 0,
    WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION  = 1,
    WEBKIT_POLICY_DECISION_TYPE_RESPONSE           = 2,
};

// WebKitNavigationType
enum {
    WEBKIT_NAVIGATION_TYPE_LINK_CLICKED      = 0,
    WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED    = 1,
    WEBKIT_NAVIGATION_TYPE_BACK_FORWARD      = 2,
    WEBKIT_NAVIGATION_TYPE_RELOAD            = 3,
    WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED  = 4,
    WEBKIT_NAVIGATION_TYPE_OTHER             = 5,
};

// WebKitUserScriptInjectionTime / WebKitUserContentInjectedFrames
enum {
    WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START = 0,
    WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END   = 1,
    WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES       = 0,
    WEBKIT_USER_CONTENT_INJECT_TOP_FRAME        = 1,
};

////////////////////////////////////////////////////////////////////////////////
// Runtime symbol table

static struct WKSymbols
{
    void *libwebkit;
    void *libjavascriptcore;

    // ---- WebKitWebView ----
    WebKitWebView* (*webkit_web_view_new_with_user_content_manager)(WebKitUserContentManager*);
    WebKitWebView* (*webkit_web_view_new_with_settings)(WebKitSettings*);
    WebKitWebView* (*webkit_web_view_new_with_related_view)(WebKitWebView*);
    void     (*webkit_web_view_load_uri)(WebKitWebView*, const gchar*);
    void     (*webkit_web_view_load_html)(WebKitWebView*, const gchar*, const gchar*);
    gboolean (*webkit_web_view_can_go_back)(WebKitWebView*);
    gboolean (*webkit_web_view_can_go_forward)(WebKitWebView*);
    void     (*webkit_web_view_go_back)(WebKitWebView*);
    void     (*webkit_web_view_go_forward)(WebKitWebView*);
    void     (*webkit_web_view_reload)(WebKitWebView*);
    void     (*webkit_web_view_stop_loading)(WebKitWebView*);
    const gchar* (*webkit_web_view_get_uri)(WebKitWebView*);
    gdouble  (*webkit_web_view_get_estimated_load_progress)(WebKitWebView*);
    WebKitSettings* (*webkit_web_view_get_settings)(WebKitWebView*);
    WebKitUserContentManager* (*webkit_web_view_get_user_content_manager)(WebKitWebView*);
    gboolean (*webkit_web_view_get_tls_info)(WebKitWebView*, void*, guint*);
    // JavaScript evaluation — try 4.1 API first, fall back to 4.0
    void  (*webkit_web_view_evaluate_javascript)(WebKitWebView*, const gchar*, gssize,
                                                  const gchar*, const gchar*,
                                                  GCancellable*, GAsyncReadyCallback, gpointer);
    void* (*webkit_web_view_evaluate_javascript_finish)(WebKitWebView*, GAsyncResult*, GError**);
    void  (*webkit_web_view_run_javascript)(WebKitWebView*, const gchar*, GCancellable*,
                                             GAsyncReadyCallback, gpointer);
    WebKitJavascriptResult* (*webkit_web_view_run_javascript_finish)(WebKitWebView*,
                                                                      GAsyncResult*, GError**);
    // Snapshot (WebKit 2.8+) — Cairo fallback on WebKit 2.44+ (Fedora) where
    // gtk_widget_draw() is a compositor no-op.
    void             (*webkit_web_view_get_snapshot)(WebKitWebView*, int, int,
                                                      GCancellable*, GAsyncReadyCallback, gpointer);
    cairo_surface_t* (*webkit_web_view_get_snapshot_finish)(WebKitWebView*, GAsyncResult*, GError**);

    // ---- WebKitSettings ----
    WebKitSettings* (*webkit_settings_new)(void);
    const gchar* (*webkit_settings_get_user_agent)(WebKitSettings*);
    void (*webkit_settings_set_user_agent)(WebKitSettings*, const gchar*);
    void (*webkit_settings_set_enable_javascript)(WebKitSettings*, gboolean);
    void (*webkit_settings_set_javascript_can_open_windows_automatically)(WebKitSettings*, gboolean);
    // Added in WebKit 2.16.  Guarded by NULL-check at call site.
    // We use value 2 = WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER directly
    // (numeric value is stable across WebKit versions).
    void (*webkit_settings_set_hardware_acceleration_policy)(WebKitSettings*, guint);

    // ---- UserContentManager ----
    WebKitUserContentManager* (*webkit_user_content_manager_new)(void);
    gboolean (*webkit_user_content_manager_register_script_message_handler)(WebKitUserContentManager*, const gchar*);
    void (*webkit_user_content_manager_unregister_script_message_handler)(WebKitUserContentManager*, const gchar*);
    void (*webkit_user_content_manager_add_script)(WebKitUserContentManager*, WebKitUserScript*);
    void (*webkit_user_content_manager_remove_all_scripts)(WebKitUserContentManager*);

    // ---- UserScript ----
    WebKitUserScript* (*webkit_user_script_new)(const gchar*, int, int, const gchar* const*, const gchar* const*);
    void (*webkit_user_script_unref)(WebKitUserScript*);

    // ---- Policy ----
    void (*webkit_policy_decision_use)(WebKitPolicyDecision*);
    void (*webkit_policy_decision_ignore)(WebKitPolicyDecision*);
    WebKitNavigationAction* (*webkit_navigation_policy_decision_get_navigation_action)(WebKitNavigationPolicyDecision*);
    WebKitURIRequest* (*webkit_navigation_action_get_request)(WebKitNavigationAction*);
    int (*webkit_navigation_action_get_navigation_type)(WebKitNavigationAction*);
    const gchar* (*webkit_uri_request_get_uri)(WebKitURIRequest*);

    // ---- JavascriptResult (4.0 only — null on 4.1) ----
    JSCValue* (*webkit_javascript_result_get_js_value)(WebKitJavascriptResult*);

    // ---- OptionMenu (show-option-menu signal, WebKit 2.18+, optional) ----
    guint                 (*webkit_option_menu_get_n_items)(WebKitOptionMenu*);
    WebKitOptionMenuItem* (*webkit_option_menu_get_item)(WebKitOptionMenu*, guint);
    const gchar*          (*webkit_option_menu_item_get_label)(WebKitOptionMenuItem*);
    gboolean              (*webkit_option_menu_item_is_enabled)(WebKitOptionMenuItem*);
    gboolean              (*webkit_option_menu_item_is_group_label)(WebKitOptionMenuItem*);
    gboolean              (*webkit_option_menu_item_is_selected)(WebKitOptionMenuItem*);
    void                  (*webkit_option_menu_activate_item)(WebKitOptionMenu*, guint);
    void                  (*webkit_option_menu_close)(WebKitOptionMenu*);

    // ---- JSCValue ----
    gboolean (*jsc_value_is_string)(JSCValue*);
    gboolean (*jsc_value_is_null)(JSCValue*);
    gboolean (*jsc_value_is_undefined)(JSCValue*);
    gchar*   (*jsc_value_to_string)(JSCValue*);

    // ---- GObject / GLib ----
    gulong   (*g_signal_connect_data)(gpointer, const gchar*, GCallback, gpointer, GClosureNotify, GConnectFlags);
    void     (*g_signal_handler_disconnect)(gpointer, gulong);
    void     (*g_object_unref)(gpointer);
    gpointer (*g_object_ref)(gpointer);
    void     (*g_free)(gpointer);
    gchar*   (*fn_g_strdup)(const gchar*);  // avoid GLib's #define g_strdup(x) g_strdup_inline(x)
    gchar*   (*g_strdup_printf)(const gchar*, ...);
    void     (*g_error_free)(GError*);
    gboolean (*g_main_context_iteration)(GMainContext*, gboolean);
    gboolean (*g_main_context_pending)(GMainContext*);
    // Log writer — loaded from system GLib (via WebKit handle) so we can
    // suppress GDK/GTK warnings on that GLib instance separately from the
    // engine's statically-linked GLib.
    void          (*g_log_set_writer_func)(GLogWriterFunc, gpointer, GDestroyNotify);
    guint         (*g_log_set_handler)(const gchar*, GLogLevelFlags, GLogFunc, gpointer);
    void          (*g_log_default_handler)(const gchar*, GLogLevelFlags, const gchar*, gpointer);
    const gchar * (*g_type_name)(GType);

    // ---- GTK ----
    GtkWidget* (*gtk_window_new)(GtkWindowType);
    GdkWindow* (*gtk_widget_get_window)(GtkWidget*);
    void (*gtk_container_add)(GtkContainer*, GtkWidget*);
    void (*gtk_widget_show)(GtkWidget*);
    void (*gtk_widget_show_all)(GtkWidget*);
    void (*gtk_widget_realize)(GtkWidget*);
    void (*gtk_widget_destroy)(GtkWidget*);
    void (*gtk_widget_set_size_request)(GtkWidget*, gint, gint);
    void (*gtk_widget_set_sensitive)(GtkWidget*, gboolean);
    gboolean (*gtk_widget_is_sensitive)(GtkWidget*);

    // ---- GTK window management ----
    void (*gtk_window_move)(GtkWindow*, gint, gint);

    // ---- GtkMenu (custom <select> popup) ----
    GtkWidget* (*gtk_menu_new)(void);
    GtkWidget* (*gtk_menu_item_new_with_label)(const gchar*);
    void       (*gtk_menu_shell_append)(GtkMenuShell*, GtkWidget*);
    GtkWidget* (*gtk_separator_menu_item_new)(void);
    void       (*gtk_menu_popup_at_rect)(GtkMenu*, GdkWindow*, const GdkRectangle*, GdkGravity, GdkGravity, const GdkEvent*);

    // ---- GtkPlug (XEMBED client) ----
    GtkWidget* (*gtk_plug_new)(Window);

    // ---- GDK/X11 ----
    Window (*gdk_x11_window_get_xid)(GdkWindow*);

    // ---- is4_1: true when evaluate_javascript resolved ----
    bool is4_1;

} wk = {0};

static bool s_webkit_loaded = false;

////////////////////////////////////////////////////////////////////////////////
// Library loading

static bool GetExeDir(char *r_dir, size_t p_size)
{
    char t_exe[PATH_MAX];
    ssize_t t_len = readlink("/proc/self/exe", t_exe, sizeof(t_exe) - 1);
    if (t_len <= 0)
        return false;
    t_exe[t_len] = '\0';
    char *t_slash = strrchr(t_exe, '/');
    if (t_slash == nil)
        return false;
    *t_slash = '\0';
    snprintf(r_dir, p_size, "%s", t_exe);
    return true;
}

static void *LoadBundled(const char *p_exedir, const char *p_name)
{
    char t_path[PATH_MAX];
    snprintf(t_path, sizeof(t_path), "%s/Externals/%s", p_exedir, p_name);
    return dlopen(t_path, RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND);
}

// Suppress known-benign GLib/GDK warnings that arise from our offscreen-window
// architecture and from WebKit's web-process lifecycle.
//
// Two interception points are used because GLib routes messages differently
// depending on API version and whether a custom writer is installed:
//
//  hxt_domain_log_handler — installed via g_log_set_handler() on the system
//  GLib instance (loaded through WebKit's dependency chain).  GDK uses old-
//  style g_warning()/g_critical() which go through g_logv() → domain handlers
//  before ever reaching the writer function.  This is the primary filter for
//  GDK messages on all GLib versions.
//
//  hxt_log_writer — installed via g_log_set_writer_func() and catches new-
//  style structured log messages (g_log_structured / g_log_structured_array).
//  Also acts as a secondary filter on configurations where the engine's GLib
//  and the system GLib are the same instance.

// Helper used by both handlers below.
static bool hxt_is_suppressed_message(const char *msg)
{
    if (!msg) return false;
    if (strstr(msg, "drawable is not a native X11 window"))      return true;
    if (strstr(msg, "waitid(") && strstr(msg, "No child processes")) return true;
    if (strstr(msg, "gdk_window_get_origin") &&
        strstr(msg, "GDK_IS_WINDOW"))                            return true;
    if (strstr(msg, "setting-changed") &&
        strstr(msg, "GdkX11Screen"))                             return true;
    if (strstr(msg, "Overriding existing handler for signal"))    return true;
    return false;
}

// Old-style domain handler (g_log_set_handler): intercepts g_warning() /
// g_critical() calls from GDK and GLib-GObject.  Uses g_log_default_handler
// directly (not via wk) so it works regardless of which GLib instance this
// handler is registered on.
static void hxt_domain_log_handler(const gchar    *log_domain,
                                    GLogLevelFlags  log_level,
                                    const gchar    *message,
                                    gpointer       /*user_data*/)
{
    if (hxt_is_suppressed_message(message))
        return;
    g_log_default_handler(log_domain, log_level, message, NULL);
}

// New-style writer (g_log_set_writer_func): intercepts g_log_structured()
// messages and acts as a secondary filter for old-style messages on
// configurations where g_logv() routes through the writer.
static GLogWriterOutput hxt_log_writer(GLogLevelFlags log_level,
                                        const GLogField *fields,
                                        gsize n_fields,
                                        gpointer /*user_data*/)
{
    for (gsize i = 0; i < n_fields; i++)
    {
        if (fields[i].key && strcmp(fields[i].key, "MESSAGE") == 0 &&
            fields[i].value)
        {
            if (hxt_is_suppressed_message(static_cast<const char*>(fields[i].value)))
                return G_LOG_WRITER_HANDLED;
        }
    }
    return g_log_writer_default(log_level, fields, n_fields, NULL);
}

#define LOAD_SYM(lib, name) \
    wk.name = (__typeof__(wk.name))dlsym(lib, #name)

// ─── stderr filter ─────────────────────────────────────────────────────────
//
// All GLib log paths — old-style g_warning/g_critical, new-style
// g_log_structured, and even WebKit's dataLog — ultimately write to fd 2
// (stderr).  We redirect fd 2 through a pipe and drain it on every engine
// runloop tick from MCLinuxWebKitGTKRunloopAction (which already calls
// wk.g_main_context_iteration to pump the system GLib).
//
// Critically, the drain is done with raw POSIX read() — no GLib IO watches.
// A GLib IO watch on the engine's static GLib context would never fire because
// only the SYSTEM GLib's context (via wk.g_main_context_iteration) is iterated
// by our runloop action.  By draining the pipe directly in the runloop we avoid
// any dependency on which GLib instance is which.
//
// Both pipe ends are O_NONBLOCK:
//  • writes to fd 2 never block (messages are silently dropped only if the
//    64 KB pipe buffer fills, which cannot happen at the rates we see), and
//  • reads in the drain function never stall the engine tick.

struct HXTStderrFilter
{
    int  read_fd;
    int  orig_stderr_fd;
    char line[8192];
    int  line_len;
    bool overflow;
    bool last_suppressed; // true if the previous line was dropped; used to
                          // swallow the blank line GLib emits after each warning
};

// Accessed from hxt_install_stderr_filter() and hxt_drain_stderr_pipe().
static HXTStderrFilter *s_stderr_filter = nullptr;

// Called from MCLinuxWebKitGTKRunloopAction on every engine tick.
static void hxt_drain_stderr_pipe(void)
{
    HXTStderrFilter *f = s_stderr_filter;
    if (!f)
        return;

    char buf[4096];
    ssize_t n;
    while ((n = read(f->read_fd, buf, sizeof(buf))) > 0)
    {
        for (ssize_t i = 0; i < n; i++)
        {
            char c = buf[i];
            if (c == '\n')
            {
                f->line[f->line_len] = '\0';
                // Suppress the matched messages, plus any bare blank lines.
                // GLib surrounds its warnings with blank lines (before and
                // after), so we suppress all empty lines from the pipe rather
                // than trying to do lookahead/lookbehind.  No legitimate
                // HyperXTalk stderr output is a bare newline.
                bool suppress = (f->line_len == 0) ||
                                (!f->overflow && hxt_is_suppressed_message(f->line));
                if (!suppress)
                {
                    if (f->line_len)
                        (void)write(f->orig_stderr_fd, f->line, f->line_len);
                    (void)write(f->orig_stderr_fd, "\n", 1);
                }
                f->last_suppressed = suppress;
                f->line_len = 0;
                f->overflow = false;
            }
            else
            {
                if (f->line_len < (int)(sizeof(f->line) - 1))
                    f->line[f->line_len++] = c;
                else
                    f->overflow = true;
            }
        }
    }
}

static void hxt_install_stderr_filter(void)
{
    if (s_stderr_filter)
        return; // already installed

    int pipefd[2];
    if (pipe(pipefd) != 0)
        return;

    // Non-blocking on both ends.
    fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
    fcntl(pipefd[1], F_SETFL, O_NONBLOCK);

    // Preserve the real stderr fd; the drain function writes passing lines here.
    int orig = dup(STDERR_FILENO);
    if (orig < 0)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    // Redirect stderr → write end of pipe.
    if (dup2(pipefd[1], STDERR_FILENO) < 0)
    {
        close(orig);
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }
    close(pipefd[1]); // STDERR_FILENO is now the write end; close the dup.

    HXTStderrFilter *filter = new HXTStderrFilter{};
    filter->read_fd         = pipefd[0];
    filter->orig_stderr_fd  = orig;
    filter->line_len        = 0;
    filter->overflow        = false;

    // Publish atomically (single pointer assignment is safe on all our targets).
    s_stderr_filter = filter;
}
// ─────────────────────────────────────────────────────────────────────────────
// Library constructor: install the stderr pipe filter at the very moment
// libbrowser.so is loaded (via dlopen), BEFORE any GTK/GDK code has run.
// The engine's native-layer-x11 code generates GDK warnings from GTK socket
// operations independently of WebKit, so we must be in place before that.
__attribute__((constructor))
static void hxt_library_init(void)
{
    hxt_install_stderr_filter();
}
// ─────────────────────────────────────────────────────────────────────────────

static bool LoadWebKit(void)
{
    if (s_webkit_loaded)
        return true;

    // The stderr filter is already installed by hxt_library_init() above.
    // Call again here as a no-op guard in case the constructor didn't fire
    // (e.g. static linking scenarios).
    hxt_install_stderr_filter();

    // Install the log writer on the engine's statically-linked GLib before any
    // WebKit initialisation so that warnings during startup are filtered.
    // GLib 2.82+ aborts if g_log_set_writer_func() is called more than once, so
    // guard with a static flag — LoadWebKit() may be called again after a failed
    // attempt (e.g. first browser creation fails, user tries a second time).
    {
        static bool s_log_writer_installed = false;
        if (!s_log_writer_installed)
        {
            g_log_set_writer_func(hxt_log_writer, NULL, NULL);
            s_log_writer_installed = true;
        }
    }

    // Install old-style domain handlers on whatever GLib instance the direct
    // symbol resolves to (may be the engine's statically-linked copy or the
    // system shared library depending on the build).  This is a best-effort
    // early installation; the definitive installation via the system
    // libglib-2.0.so.0 handle happens later, after WebKit is loaded.
    {
        static bool s_domain_handlers_direct = false;
        if (!s_domain_handlers_direct)
        {
            GLogLevelFlags t_levels = (GLogLevelFlags)(
                G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL |
                G_LOG_FLAG_RECURSION | G_LOG_FLAG_FATAL);
            g_log_set_handler("Gdk",          t_levels, hxt_domain_log_handler, NULL);
            g_log_set_handler("GLib-GObject", t_levels, hxt_domain_log_handler, NULL);
            g_log_set_handler("GLib",         t_levels, hxt_domain_log_handler, NULL);
            g_log_set_handler(NULL,           t_levels, hxt_domain_log_handler, NULL);
            s_domain_handlers_direct = true;
        }
    }

    // Disable AT-SPI bridge to prevent crashes on systems where the D-Bus/ATK
    // bridge is incompatible with the dynamically loaded WebKit.
    setenv("NO_AT_BRIDGE", "1", 0);

    // Disable the bubblewrap sandbox.  When WebKit is loaded via dlopen inside
    // another application's process (rather than as a direct dependency), the
    // sandbox's seccomp filter frequently prevents the web process from starting,
    // leaving the view permanently blank.
    setenv("WEBKIT_DISABLE_SANDBOX_THIS_IS_DANGEROUS", "1", 0);

    // AppImage detection: when running from an AppImage, the bundled WebKit's
    // subprocess helpers inherit LD_LIBRARY_PATH pointing at the AppImage mount,
    // which causes them to pick up the wrong EGL/GL libraries and crash.
    // Force X11 backend and software rendering so WebKit subprocesses stay safe.
    // These env vars must NOT be set on a normal dev-binary run because they
    // prevent WebKit from using hardware acceleration on X11 desktops.
    {
        const char *t_ai = getenv("APPIMAGE");
        const char *t_lp = getenv("LD_LIBRARY_PATH");
        bool t_in_appimage = (t_ai && t_ai[0]) ||
                             (t_lp && strstr(t_lp, "/tmp/.mount_"));
        if (t_in_appimage)
        {
            unsetenv("WAYLAND_DISPLAY");
            setenv("GDK_BACKEND", "x11", 1);
            setenv("WEBKIT_DISABLE_COMPOSITING_MODE", "1", 1);
            setenv("WEBKIT_DISABLE_DMABUF_RENDERER", "1", 1);
            // EGL_PLATFORM must be forced (overwrite=1) — on a Wayland session
            // this variable may already be "wayland", causing eglGetDisplay() to
            // receive an X11 Display* for a Wayland EGL, which returns EGL_BAD_PARAMETER
            // and crashes WebKitWebProcess.
            setenv("EGL_PLATFORM", "x11", 1);
            setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1);
        }
    }

    // Disable DMA-BUF renderer.  WebKit2GTK ≥ 2.40 uses DMA-BUF for tile IPC
    // between the web process and the UI process.  DMA-BUF tiles are GPU textures
    // and are not accessible as plain cairo surfaces, so gtk_widget_draw() would
    // produce a blank result even with WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER.
    // Disabling the DMA-BUF renderer forces WebKit to use shared-memory (SHM)
    // tiles, which are plain cairo image surfaces and are correctly blitted by
    // WebKit's "draw" signal handler into our cairo_t in doPaint().
    setenv("WEBKIT_DISABLE_DMABUF_RENDERER", "1", 0);

    // Disable GVFS FUSE mount so WebKit doesn't try to contact gvfsd.
    setenv("GVFS_DISABLE_FUSE", "1", 1);
    // Use the local (non-GVFS) VFS implementation.
    setenv("GIO_USE_VFS", "local", 1);

    char t_exedir[PATH_MAX];
    bool t_have_exedir = GetExeDir(t_exedir, sizeof(t_exedir));

    // CRITICAL: try system WebKit FIRST.
    //
    // The engine statically links GLib/GTK.  Loading a bundled WebKit that
    // brings its own shared libglib-2.0 / libgtk-3 creates two GObject type
    // tables in the same process, crashing with "cannot register existing type".
    // System WebKit links against the same system GLib/GTK already loaded by
    // the engine, so the dynamic linker resolves those symbols against the
    // engine's already-resident copies — no duplication.

    const char *t_wk_names[] = {
        "libwebkit2gtk-4.1.so.0",   // GTK3, API 4.1 (Ubuntu 22.04+, Fedora 37+)
        "libwebkit2gtk-4.0.so.37",  // GTK3, API 4.0 (older distros)
        "libwebkit2gtk-4.0.so",     // unversioned symlink fallback (some Arch setups)
        nil
    };
    const char *t_jsc_names[] = {
        "libjavascriptcoregtk-4.1.so.0",
        "libjavascriptcoregtk-4.0.so.18",
        "libjavascriptcoregtk-4.0.so",
        nil
    };

    // Reset SIGUSR1 to SIG_DFL before dlopen()ing WebKit.  WebKit's JSC installs
    // its GC signal handler via a global constructor that runs at dlopen time.
    // If it finds an existing handler it prints "Overriding existing handler for
    // signal 10" directly to stderr, bypassing GLib's log system.  WebKit
    // replaces whatever was there regardless; we just pre-empt the noise by
    // returning SIGUSR1 to its default first.
    {
        struct sigaction t_sa;
        memset(&t_sa, 0, sizeof(t_sa));
        t_sa.sa_handler = SIG_DFL;
        sigaction(SIGUSR1, &t_sa, NULL);
    }

    bool t_system = false;
    for (int i = 0; t_wk_names[i]; i++)
    {
        wk.libwebkit = dlopen(t_wk_names[i], RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND);
        if (wk.libwebkit)
        {
            t_system = true;
            break;
        }
    }

    if (!t_system && t_have_exedir)
    {
        // Fall back to bundled copies.  Pre-load bundled GLib/GTK dependencies
        // with RTLD_DEEPBIND so they don't stomp on the engine's static copies.
        static const char *t_glib_deps[] = {
            "libglib-2.0.so.0", "libgobject-2.0.so.0",
            "libgio-2.0.so.0", "libgdk-3.so.0",
            "libgtk-3.so.0", nil
        };
        for (const char **p = t_glib_deps; *p; p++)
            LoadBundled(t_exedir, *p);

        for (int i = 0; t_wk_names[i]; i++)
        {
            wk.libwebkit = LoadBundled(t_exedir, t_wk_names[i]);
            if (wk.libwebkit)
                break;
        }

        if (wk.libwebkit)
        {
            // Tell child WebKit processes where to find bundled libs.
            char t_extdir[PATH_MAX];
            snprintf(t_extdir, sizeof(t_extdir), "%s/Externals", t_exedir);
            char t_giomod[PATH_MAX];
            snprintf(t_giomod, sizeof(t_giomod), "%s/gio/modules", t_extdir);
            struct stat t_st;
            if (stat(t_giomod, &t_st) == 0 && S_ISDIR(t_st.st_mode))
                setenv("GIO_MODULE_DIR", t_giomod, 1);

            const char *t_existing = getenv("LD_LIBRARY_PATH");
            char t_ldpath[PATH_MAX * 2];
            if (t_existing && t_existing[0])
                snprintf(t_ldpath, sizeof(t_ldpath), "%s:%s", t_extdir, t_existing);
            else
                snprintf(t_ldpath, sizeof(t_ldpath), "%s", t_extdir);
            setenv("LD_LIBRARY_PATH", t_ldpath, 1);

        }
    }

    if (!wk.libwebkit)
        return false;

    // Load matching JavaScriptCore.
    for (int i = 0; t_jsc_names[i]; i++)
    {
        if (t_system)
            wk.libjavascriptcore = dlopen(t_jsc_names[i], RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND);
        else if (t_have_exedir)
            wk.libjavascriptcore = LoadBundled(t_exedir, t_jsc_names[i]);
        if (wk.libjavascriptcore)
            break;
    }
    // JSC is optional — evaluate_javascript (4.1) returns a JSCValue from the
    // WebKit library itself; run_javascript_finish (4.0) needs JSC for
    // webkit_javascript_result_get_js_value.

    void *t_wk  = wk.libwebkit;
    void *t_jsc = wk.libjavascriptcore ? wk.libjavascriptcore : wk.libwebkit;

    // WebKitWebView
    LOAD_SYM(t_wk, webkit_web_view_new_with_user_content_manager);
    LOAD_SYM(t_wk, webkit_web_view_new_with_settings);
    LOAD_SYM(t_wk, webkit_web_view_new_with_related_view);
    LOAD_SYM(t_wk, webkit_web_view_load_uri);
    LOAD_SYM(t_wk, webkit_web_view_load_html);
    LOAD_SYM(t_wk, webkit_web_view_can_go_back);
    LOAD_SYM(t_wk, webkit_web_view_can_go_forward);
    LOAD_SYM(t_wk, webkit_web_view_go_back);
    LOAD_SYM(t_wk, webkit_web_view_go_forward);
    LOAD_SYM(t_wk, webkit_web_view_reload);
    LOAD_SYM(t_wk, webkit_web_view_stop_loading);
    LOAD_SYM(t_wk, webkit_web_view_get_uri);
    LOAD_SYM(t_wk, webkit_web_view_get_estimated_load_progress);
    LOAD_SYM(t_wk, webkit_web_view_get_settings);
    LOAD_SYM(t_wk, webkit_web_view_get_user_content_manager);
    LOAD_SYM(t_wk, webkit_web_view_get_tls_info);
    // 4.1 JS eval API
    LOAD_SYM(t_wk, webkit_web_view_evaluate_javascript);
    LOAD_SYM(t_wk, webkit_web_view_evaluate_javascript_finish);
    // 4.0 JS eval API (fallback)
    LOAD_SYM(t_wk, webkit_web_view_run_javascript);
    LOAD_SYM(t_wk, webkit_web_view_run_javascript_finish);
    // Snapshot API (WebKit 2.8+, optional — NULL-checked at call site)
    LOAD_SYM(t_wk, webkit_web_view_get_snapshot);
    LOAD_SYM(t_wk, webkit_web_view_get_snapshot_finish);

    wk.is4_1 = (wk.webkit_web_view_evaluate_javascript != nil);

    // Settings
    LOAD_SYM(t_wk, webkit_settings_new);
    LOAD_SYM(t_wk, webkit_settings_get_user_agent);
    LOAD_SYM(t_wk, webkit_settings_set_user_agent);
    LOAD_SYM(t_wk, webkit_settings_set_enable_javascript);
    LOAD_SYM(t_wk, webkit_settings_set_javascript_can_open_windows_automatically);
    LOAD_SYM(t_wk, webkit_settings_set_hardware_acceleration_policy);

    // UserContentManager
    LOAD_SYM(t_wk, webkit_user_content_manager_new);
    LOAD_SYM(t_wk, webkit_user_content_manager_register_script_message_handler);
    LOAD_SYM(t_wk, webkit_user_content_manager_unregister_script_message_handler);
    LOAD_SYM(t_wk, webkit_user_content_manager_add_script);
    LOAD_SYM(t_wk, webkit_user_content_manager_remove_all_scripts);
    LOAD_SYM(t_wk, webkit_user_script_new);
    LOAD_SYM(t_wk, webkit_user_script_unref);

    // Policy
    LOAD_SYM(t_wk, webkit_policy_decision_use);
    LOAD_SYM(t_wk, webkit_policy_decision_ignore);
    LOAD_SYM(t_wk, webkit_navigation_policy_decision_get_navigation_action);
    LOAD_SYM(t_wk, webkit_navigation_action_get_request);
    LOAD_SYM(t_wk, webkit_navigation_action_get_navigation_type);
    LOAD_SYM(t_wk, webkit_uri_request_get_uri);

    // 4.0 JavascriptResult → JSCValue unwrap (not present in 4.1)
    LOAD_SYM(t_wk, webkit_javascript_result_get_js_value);

    // OptionMenu (WebKit 2.18+, optional — NULL-checked at call site)
    LOAD_SYM(t_wk, webkit_option_menu_get_n_items);
    LOAD_SYM(t_wk, webkit_option_menu_get_item);
    LOAD_SYM(t_wk, webkit_option_menu_item_get_label);
    LOAD_SYM(t_wk, webkit_option_menu_item_is_enabled);
    LOAD_SYM(t_wk, webkit_option_menu_item_is_group_label);
    LOAD_SYM(t_wk, webkit_option_menu_item_is_selected);
    LOAD_SYM(t_wk, webkit_option_menu_activate_item);
    LOAD_SYM(t_wk, webkit_option_menu_close);

    // JSCValue (may be in libwebkit or libjavascriptcore depending on version)
    LOAD_SYM(t_jsc, jsc_value_is_string);
    LOAD_SYM(t_jsc, jsc_value_is_null);
    LOAD_SYM(t_jsc, jsc_value_is_undefined);
    LOAD_SYM(t_jsc, jsc_value_to_string);
    if (!wk.jsc_value_is_string)
    {
        LOAD_SYM(t_wk, jsc_value_is_string);
        LOAD_SYM(t_wk, jsc_value_is_null);
        LOAD_SYM(t_wk, jsc_value_is_undefined);
        LOAD_SYM(t_wk, jsc_value_to_string);
    }

    // GLib — resolve from webkit's loaded GLib to avoid symbol collisions
    LOAD_SYM(t_wk, g_signal_connect_data);
    LOAD_SYM(t_wk, g_signal_handler_disconnect);
    LOAD_SYM(t_wk, g_object_unref);
    LOAD_SYM(t_wk, g_object_ref);
    LOAD_SYM(t_wk, g_free);
    wk.fn_g_strdup = (gchar*(*)(const gchar*))dlsym(t_wk, "g_strdup");
    LOAD_SYM(t_wk, g_strdup_printf);
    LOAD_SYM(t_wk, g_error_free);
    LOAD_SYM(t_wk, g_main_context_iteration);
    LOAD_SYM(t_wk, g_main_context_pending);
    LOAD_SYM(t_wk, g_log_set_writer_func);
    LOAD_SYM(t_wk, g_log_set_handler);
    LOAD_SYM(t_wk, g_log_default_handler);
    LOAD_SYM(t_wk, g_type_name);

    // GTK — resolve from the already-loaded libgtk-3
    void *t_gtk = dlopen("libgtk-3.so.0", RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
    if (!t_gtk) t_gtk = dlopen("libgtk-3.so.0", RTLD_LAZY | RTLD_LOCAL);
    if (!t_gtk)
        return false;

    LOAD_SYM(t_gtk, gtk_window_new);
    LOAD_SYM(t_gtk, gtk_widget_get_window);
    LOAD_SYM(t_gtk, gtk_container_add);
    LOAD_SYM(t_gtk, gtk_widget_show);
    LOAD_SYM(t_gtk, gtk_widget_show_all);
    LOAD_SYM(t_gtk, gtk_widget_realize);
    LOAD_SYM(t_gtk, gtk_widget_destroy);
    LOAD_SYM(t_gtk, gtk_widget_set_size_request);
    LOAD_SYM(t_gtk, gtk_widget_set_sensitive);
    LOAD_SYM(t_gtk, gtk_widget_is_sensitive);
    LOAD_SYM(t_gtk, gtk_window_move);

    // GtkMenu — for custom <select> popup positioning
    LOAD_SYM(t_gtk, gtk_menu_new);
    LOAD_SYM(t_gtk, gtk_menu_item_new_with_label);
    LOAD_SYM(t_gtk, gtk_menu_shell_append);
    LOAD_SYM(t_gtk, gtk_separator_menu_item_new);
    LOAD_SYM(t_gtk, gtk_menu_popup_at_rect);

    // gtk_plug_new is in libgdk-3 / libgtk-3 as part of the XEMBED support.
    // It lives in libgtk-3 on GTK3 systems (same handle as t_gtk).
    LOAD_SYM(t_gtk, gtk_plug_new);

    // GDK/X11 — needed for GetNativeLayer()
    void *t_gdk = dlopen("libgdk-3.so.0", RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
    if (!t_gdk) t_gdk = dlopen("libgdk-3.so.0", RTLD_LAZY | RTLD_LOCAL);
    if (t_gdk)
    {
        LOAD_SYM(t_gdk, gdk_x11_window_get_xid);
    }

    if (!wk.webkit_web_view_load_uri || !wk.gtk_window_new || !wk.gdk_x11_window_get_xid)
        return false;

    // Strip AppImage mount paths from LD_LIBRARY_PATH so WebKit subprocesses
    // (WebKitWebProcess, WebKitGPUProcess) find system EGL/GL libraries instead
    // of AppImage-bundled ones.  AppImage libEGL returns EGL_BAD_PARAMETER from
    // eglGetDisplay(), aborting the GPU process and leaving WebKit unable to render.
    // Done after our own dlopen() calls so we don't break our own library loading.
    {
        const char *t_lp = getenv("LD_LIBRARY_PATH");
        if (t_lp && strstr(t_lp, "/tmp/.mount_"))
        {
            char t_filtered[4096];
            t_filtered[0] = '\0';
            char t_copy[4096];
            snprintf(t_copy, sizeof(t_copy), "%s", t_lp);
            char *t_save = nil;
            char *t_tok  = strtok_r(t_copy, ":", &t_save);
            bool t_first = true;
            while (t_tok)
            {
                if (!strstr(t_tok, "/tmp/.mount_"))
                {
                    if (!t_first)
                        strncat(t_filtered, ":", sizeof(t_filtered) - strlen(t_filtered) - 1);
                    strncat(t_filtered, t_tok, sizeof(t_filtered) - strlen(t_filtered) - 1);
                    t_first = false;
                }
                t_tok = strtok_r(nil, ":", &t_save);
            }
            if (t_first)
                unsetenv("LD_LIBRARY_PATH");
            else
                setenv("LD_LIBRARY_PATH", t_filtered, 1);
        }
    }

    // Install handlers on the system libglib-2.0.so.0 instance.
    //
    // The engine statically links its own GLib, so direct g_log_set_handler()
    // calls only reach the engine's private GLib state.  GDK (a shared library)
    // depends on and routes through system libglib-2.0.so.0 — a completely
    // separate instance with its own log-handler registry.  dlsym() via the
    // WebKit handle may also resolve to the engine's copy if the engine's static
    // symbols are visible in the global namespace.
    //
    // The only reliable way to install into the correct instance is to open
    // libglib-2.0.so.0 by name with RTLD_NOLOAD (it must already be loaded as a
    // transitive dependency of libgdk-3 or libwebkit2gtk) and dlsym from that
    // exact handle.
    {
        static bool s_syslib_handlers_installed = false;
        if (!s_syslib_handlers_installed)
        {
            void *t_glib = dlopen("libglib-2.0.so.0",
                                  RTLD_NOLOAD | RTLD_LAZY | RTLD_GLOBAL);
            if (t_glib)
            {
                typedef guint (*g_log_set_handler_t)(const gchar *,
                                                     GLogLevelFlags,
                                                     GLogFunc,
                                                     gpointer);
                typedef void  (*g_log_set_writer_func_t)(GLogWriterFunc,
                                                         gpointer,
                                                         GDestroyNotify);

                auto t_set_handler =
                    (g_log_set_handler_t)dlsym(t_glib, "g_log_set_handler");
                auto t_set_writer   =
                    (g_log_set_writer_func_t)dlsym(t_glib, "g_log_set_writer_func");

                if (t_set_handler)
                {
                    GLogLevelFlags t_levels = (GLogLevelFlags)(
                        G_LOG_LEVEL_WARNING  | G_LOG_LEVEL_CRITICAL |
                        G_LOG_FLAG_RECURSION | G_LOG_FLAG_FATAL);
                    // Named domains for the specific messages we want to drop.
                    t_set_handler("Gdk",          t_levels, hxt_domain_log_handler, NULL);
                    t_set_handler("GLib-GObject", t_levels, hxt_domain_log_handler, NULL);
                    t_set_handler("GLib",         t_levels, hxt_domain_log_handler, NULL);
                    // Catch-all: NULL domain fires for any domain that has no
                    // dedicated handler registered, so this covers any other
                    // noisy domain we haven't named explicitly.
                    t_set_handler(NULL,            t_levels, hxt_domain_log_handler, NULL);
                }

                // Also install the structured-log writer if it is a different
                // function pointer than the engine's copy (GLib 2.82+ aborts on
                // double installation of the same instance's writer).
                if (t_set_writer &&
                    (void*)t_set_writer != (void*)&g_log_set_writer_func)
                {
                    static bool s_syslib_writer_installed = false;
                    if (!s_syslib_writer_installed)
                    {
                        t_set_writer(hxt_log_writer, NULL, NULL);
                        s_syslib_writer_installed = true;
                    }
                }

                dlclose(t_glib);
            }
            s_syslib_handlers_installed = true;
        }
    }

    s_webkit_loaded = true;
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// MCWebKitGTKBrowser

class MCWebKitGTKBrowser : public MCBrowserBase
{
public:
    MCWebKitGTKBrowser();
    virtual ~MCWebKitGTKBrowser();

    bool Init(void *p_display, void *p_parent_window);

    virtual void *GetNativeLayer();
    virtual bool GetRect(MCBrowserRect &r_rect);
    virtual bool SetRect(const MCBrowserRect &p_rect);

    virtual bool GetBoolProperty(MCBrowserProperty p_property, bool &r_value);
    virtual bool SetBoolProperty(MCBrowserProperty p_property, bool p_value);
    virtual bool GetStringProperty(MCBrowserProperty p_property, char *&r_utf8_string);
    virtual bool SetStringProperty(MCBrowserProperty p_property, const char *p_utf8_string);
    virtual bool GetIntegerProperty(MCBrowserProperty p_property, int32_t &r_value);
    virtual bool SetIntegerProperty(MCBrowserProperty p_property, int32_t p_value);

    virtual bool GoBack();
    virtual bool GoForward();
    virtual bool GoToURL(const char *p_url);
    virtual bool LoadHTMLText(const char *p_htmltext, const char *p_base_url);
    virtual bool StopLoading();
    virtual bool Reload();
    virtual bool EvaluateJavaScript(const char *p_script, char *&r_result);

    void SyncJavaScriptHandlers();

    // Signal callbacks
    static void     on_load_changed(WebKitWebView*, int, gpointer);
    static int      on_load_failed(WebKitWebView*, int, char*, GError*, gpointer);
    static int      on_decide_policy(WebKitWebView*, WebKitPolicyDecision*, int, gpointer);
    static GtkWidget* on_create(WebKitWebView*, WebKitNavigationAction*, gpointer);
    static int      on_context_menu(WebKitWebView*, gpointer, gpointer, gpointer, gpointer);
    static void     on_progress_changed(GObject*, GParamSpec*, gpointer);
    static void     on_script_message(WebKitUserContentManager*, gpointer, gpointer);
    // Navigation via injected click-interceptor script → hxtNav message handler
    static void     on_nav_message(WebKitUserContentManager*, gpointer, gpointer);
    // JS eval callbacks
    static void     on_js_finished_40(GObject*, GAsyncResult*, gpointer);
    static void     on_js_finished_41(GObject*, GAsyncResult*, gpointer);

    // show-option-menu: intercept native <select> popup so we can position it
    // correctly (GtkOffscreenWindow has no real screen position → GTK places the
    // native menu at (0,0)).  We build a GtkMenu from the WebKitOptionMenu items
    // using screen coordinates stored on the WebView via g_object_set_data by
    // native-layer-x11::doSetGeometry.
    static gboolean on_show_option_menu(WebKitWebView*, WebKitOptionMenu*,
                                         GdkEvent*, GdkRectangle*, gpointer);
    // Helpers for on_show_option_menu
    static void on_option_menu_item_activate(GtkMenuItem*, gpointer);
    static void on_option_menu_hidden(GtkWidget*, gpointer);

    // Click simulation — called by native-layer-x11 via g_object_get_data
    // when a left-button release is detected over the browser rect.
    // Uses JavaScript hit-testing to navigate to whatever link or element is
    // under the cursor, bypassing GDK event routing which doesn't work
    // reliably for off-screen / popup-hosted WebKitWebView instances.
    static void SimulateClick(void *ctx, int x, int y);

    // Snapshot fallback for WebKit 2.44+ (Fedora) where gtk_widget_draw() is a
    // compositor no-op.  doPaint() calls RequestSnapshot (via hxt-snap-fn bridge)
    // when it detects a blank surface.  on_snapshot_done() stores the resulting
    // cairo_surface_t as "hxt-snapshot" and triggers Redraw() via hxt-repaint-fn.
    static void RequestSnapshot(void *ctx);
    static void on_snapshot_done(GObject *source, GAsyncResult *result, gpointer user_data);

private:
    GtkWidget                *m_plug;
    WebKitWebView            *m_web_view;
    WebKitUserContentManager *m_content_manager;

    char *m_js_handlers;
    bool  m_allow_new_windows;
    bool  m_enable_context_menu;
    bool  m_snapshot_pending;

    gulong m_load_changed_id;
    gulong m_load_failed_id;
    gulong m_decide_policy_id;
    gulong m_create_id;
    gulong m_context_menu_id;
    gulong m_progress_id;
    gulong m_script_message_id;
    gulong m_nav_message_id;
    gulong m_show_option_menu_id;

    // Async JS evaluation state
    bool   m_js_finished;
    char  *m_js_result;

    friend void JSFinishWithValue(MCWebKitGTKBrowser*, JSCValue*, GError*);

    // Helpers
    bool GetUrl(char *&r_url);
    bool GetHTMLText(char *&r_htmltext);
    bool GetUserAgent(char *&r_useragent);
    bool SetUserAgent(const char *p_useragent);
    bool GetIsSecure(bool &r_value);
};

MCWebKitGTKBrowser::MCWebKitGTKBrowser()
    : m_plug(nil),
      m_web_view(nil),
      m_content_manager(nil),
      m_js_handlers(nil),
      m_allow_new_windows(false),
      m_enable_context_menu(true),
      m_load_changed_id(0),
      m_load_failed_id(0),
      m_decide_policy_id(0),
      m_create_id(0),
      m_context_menu_id(0),
      m_progress_id(0),
      m_script_message_id(0),
      m_nav_message_id(0),
      m_show_option_menu_id(0),
      m_snapshot_pending(false),
      m_js_finished(false),
      m_js_result(nil)
{
}

MCWebKitGTKBrowser::~MCWebKitGTKBrowser()
{
    // Guard every g_signal_handler_disconnect with G_IS_OBJECT(): when HXT
    // quits with a browser open, the native layer destroys m_child_window
    // first, which finalizes m_web_view (and its content manager) as child
    // widgets.  Our destructor then runs and must not touch dead GObjects.
    if (m_content_manager != nil && G_IS_OBJECT(m_content_manager))
    {
        if (m_script_message_id)
            wk.g_signal_handler_disconnect(m_content_manager, m_script_message_id);
        if (m_nav_message_id)
            wk.g_signal_handler_disconnect(m_content_manager, m_nav_message_id);
        if (wk.webkit_user_content_manager_unregister_script_message_handler)
        {
            wk.webkit_user_content_manager_unregister_script_message_handler(m_content_manager, "liveCode");
            wk.webkit_user_content_manager_unregister_script_message_handler(m_content_manager, "hxtNav");
        }
    }

    if (m_web_view != nil && G_IS_OBJECT(m_web_view))
    {
        if (m_load_changed_id)       wk.g_signal_handler_disconnect(m_web_view, m_load_changed_id);
        if (m_load_failed_id)        wk.g_signal_handler_disconnect(m_web_view, m_load_failed_id);
        if (m_decide_policy_id)      wk.g_signal_handler_disconnect(m_web_view, m_decide_policy_id);
        if (m_create_id)             wk.g_signal_handler_disconnect(m_web_view, m_create_id);
        if (m_context_menu_id)       wk.g_signal_handler_disconnect(m_web_view, m_context_menu_id);
        if (m_progress_id)           wk.g_signal_handler_disconnect(m_web_view, m_progress_id);
        if (m_show_option_menu_id)   wk.g_signal_handler_disconnect(m_web_view, m_show_option_menu_id);

        // Free any cached snapshot surface.
        cairo_surface_t *t_snap =
            (cairo_surface_t*)g_object_get_data(G_OBJECT(m_web_view), "hxt-snapshot");
        if (t_snap)
        {
            g_object_set_data(G_OBJECT(m_web_view), "hxt-snapshot", nil);
            cairo_surface_destroy(t_snap);
        }
    }

    // m_web_view is owned by m_child_window in native-layer-x11; no destroy here.

    if (m_js_handlers != nil) wk.g_free(m_js_handlers);
    if (m_js_result   != nil) wk.g_free(m_js_result);
}

bool MCWebKitGTKBrowser::Init(void *p_display, void *p_parent_window)
{
    if (!wk.webkit_user_content_manager_new)
        return false;

    // --- UserContentManager for JS→engine callbacks ---
    m_content_manager = wk.webkit_user_content_manager_new();
    if (m_content_manager == nil)
        return false;

    if (wk.webkit_user_content_manager_register_script_message_handler)
    {
        wk.webkit_user_content_manager_register_script_message_handler(m_content_manager, "liveCode");
        wk.webkit_user_content_manager_register_script_message_handler(m_content_manager, "hxtNav");
    }

    // Inject the click-interceptor at document start so it runs before any
    // page script.  Capture phase (true) means we see the event before the
    // page's own handlers; preventDefault() stops the page from doing its own
    // navigation (which may fail in the off-screen context).
    {
        const char *t_nav_js =
            "(function(){"
              "document.addEventListener('click',function(e){"
                "var n=e.target;"
                "while(n&&n.nodeName!=='A')n=n.parentElement;"
                "if(n&&n.href&&n.href.indexOf('javascript:')!==0){"
                  "try{"
                    "e.preventDefault();"
                    "e.stopPropagation();"
                    "window.webkit.messageHandlers.hxtNav.postMessage(n.href);"
                  "}catch(ex){}"
                "}"
              "},true);"
            "})();";
        WebKitUserScript *t_uscript = wk.webkit_user_script_new(
            t_nav_js,
            WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
            WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
            NULL, NULL);
        if (t_uscript)
        {
            wk.webkit_user_content_manager_add_script(m_content_manager, t_uscript);
            wk.webkit_user_script_unref(t_uscript);
        }
    }

    // --- WebKitWebView ---
    m_web_view = (WebKitWebView*)wk.webkit_web_view_new_with_user_content_manager(m_content_manager);
    if (m_web_view == nil)
        return false;

    {
        WebKitSettings *t_settings = wk.webkit_web_view_get_settings(m_web_view);
        if (t_settings)
        {
            // Disable new-window navigation by default (controlled by property).
            if (wk.webkit_settings_set_javascript_can_open_windows_automatically)
                wk.webkit_settings_set_javascript_can_open_windows_automatically(t_settings, FALSE);

            // Force software compositing (WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER = 2).
            //
            // native-layer-x11 now uses GtkOffscreenWindow: the browser renders
            // into an internal cairo_surface_t, which doPaint() reads and blits
            // into HXT's MCGContext.  GtkOffscreenWindow captures widget drawing
            // via Cairo; if WebKit uses GL/EGL compositing the result bypasses
            // Cairo entirely and the offscreen surface stays blank.
            //
            // With POLICY_NEVER, WebKit falls back to fully software-based Cairo
            // rendering, which GtkOffscreenWindow captures correctly.
            //
            // Added in WebKit 2.16; guarded by NULL-check so older versions
            // gracefully skip this (offscreen rendering may show blank content
            // on those versions, but that's better than a hard link failure).
            if (wk.webkit_settings_set_hardware_acceleration_policy)
                wk.webkit_settings_set_hardware_acceleration_policy(t_settings, 2 /* NEVER */);
        }
    }

    // --- Container window ---
    // We do NOT create a container here.  native-layer-x11::doAttach() adds
    // m_web_view directly as a GTK child of its GtkOffscreenWindow (m_child_window).
    // GetNativeLayer() returns (void*)m_web_view so native-layer-x11 can receive
    // the widget pointer.

    // Publish click-simulation trampoline so native-layer-x11 can call it.
    // WebKit is loaded RTLD_LOCAL so its symbols aren't findable via dlsym from
    // engine code; we bridge via g_object_data instead.
    g_object_set_data(G_OBJECT(m_web_view), "hxt-sim-fn",
        (gpointer)(void(*)(void*, int, int))&MCWebKitGTKBrowser::SimulateClick);
    g_object_set_data(G_OBJECT(m_web_view), "hxt-sim-ctx", (gpointer)this);

    // Publish snapshot trampoline for the doPaint() blank-surface fallback
    // (WebKit 2.44+, Fedora).  doPaint() looks these up when gtk_widget_draw()
    // produces only the white background.
    if (wk.webkit_web_view_get_snapshot && wk.webkit_web_view_get_snapshot_finish)
    {
        g_object_set_data(G_OBJECT(m_web_view), "hxt-snap-fn",
            (gpointer)(void(*)(void*))&MCWebKitGTKBrowser::RequestSnapshot);
        g_object_set_data(G_OBJECT(m_web_view), "hxt-snap-ctx", (gpointer)this);
    }

    // --- Connect signals ---
    m_load_changed_id = wk.g_signal_connect_data(m_web_view, "load-changed",
        G_CALLBACK(on_load_changed), this, nil, (GConnectFlags)0);
    m_load_failed_id = wk.g_signal_connect_data(m_web_view, "load-failed",
        G_CALLBACK(on_load_failed), this, nil, (GConnectFlags)0);
    m_decide_policy_id = wk.g_signal_connect_data(m_web_view, "decide-policy",
        G_CALLBACK(on_decide_policy), this, nil, (GConnectFlags)0);
    // The "create" signal fires when WebKit wants to open a new window (including
    // when off-screen click events arrive as NEW_WINDOW_ACTION in decide-policy).
    // We intercept it to navigate in the current view instead.
    m_create_id = wk.g_signal_connect_data(m_web_view, "create",
        G_CALLBACK(on_create), this, nil, (GConnectFlags)0);
    m_context_menu_id = wk.g_signal_connect_data(m_web_view, "context-menu",
        G_CALLBACK(on_context_menu), this, nil, (GConnectFlags)0);
    m_progress_id = wk.g_signal_connect_data(m_web_view, "notify::estimated-load-progress",
        G_CALLBACK(on_progress_changed), this, nil, (GConnectFlags)0);

    m_script_message_id = wk.g_signal_connect_data(m_content_manager,
        "script-message-received::liveCode",
        G_CALLBACK(on_script_message), this, nil, (GConnectFlags)0);

    m_nav_message_id = wk.g_signal_connect_data(m_content_manager,
        "script-message-received::hxtNav",
        G_CALLBACK(on_nav_message), this, nil, (GConnectFlags)0);

    // Intercept native <select> popup.  The signal is optional (WebKit 2.18+);
    // if the symbol wasn't loaded the connect is a no-op (g_signal_connect_data
    // returns 0 for unknown signals, which we safely ignore).
    m_show_option_menu_id = wk.g_signal_connect_data(m_web_view, "show-option-menu",
        G_CALLBACK(on_show_option_menu), this, nil, (GConnectFlags)0);

    return true;
}

void *MCWebKitGTKBrowser::GetNativeLayer()
{
    // Return the WebKitWebView widget pointer directly.  native-layer-x11
    // receives it as void* and casts to GtkWidget* to add it to its container.
    return (void*)m_web_view;
}

bool MCWebKitGTKBrowser::GetRect(MCBrowserRect &r_rect)
{
    if (m_web_view == nil)
        return false;
    GtkAllocation t_alloc;
    gtk_widget_get_allocation((GtkWidget*)m_web_view, &t_alloc);
    r_rect.left   = t_alloc.x;
    r_rect.top    = t_alloc.y;
    r_rect.right  = t_alloc.x + t_alloc.width;
    r_rect.bottom = t_alloc.y + t_alloc.height;
    return true;
}

bool MCWebKitGTKBrowser::SetRect(const MCBrowserRect &p_rect)
{
    if (m_web_view == nil)
        return false;
    int t_w = p_rect.right  - p_rect.left;
    int t_h = p_rect.bottom - p_rect.top;
    if (t_w <= 0) t_w = 1;
    if (t_h <= 0) t_h = 1;
    wk.gtk_widget_set_size_request((GtkWidget*)m_web_view, t_w, t_h);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Navigation

bool MCWebKitGTKBrowser::GoToURL(const char *p_url)
{
    if (m_web_view == nil || p_url == nil)
        return false;
    if (*p_url == '\0')
        wk.webkit_web_view_load_html(m_web_view, "<html><body></body></html>", nil);
    else
        wk.webkit_web_view_load_uri(m_web_view, p_url);
    return true;
}

bool MCWebKitGTKBrowser::LoadHTMLText(const char *p_htmltext, const char *p_base_url)
{
    if (m_web_view == nil)
        return false;
    wk.webkit_web_view_load_html(m_web_view, p_htmltext,
                                  p_base_url && *p_base_url ? p_base_url : nil);
    return true;
}

bool MCWebKitGTKBrowser::GoBack()
{
    if (m_web_view == nil) return false;
    wk.webkit_web_view_go_back(m_web_view);
    return true;
}

bool MCWebKitGTKBrowser::GoForward()
{
    if (m_web_view == nil) return false;
    wk.webkit_web_view_go_forward(m_web_view);
    return true;
}

bool MCWebKitGTKBrowser::StopLoading()
{
    if (m_web_view == nil) return false;
    wk.webkit_web_view_stop_loading(m_web_view);
    return true;
}

bool MCWebKitGTKBrowser::Reload()
{
    if (m_web_view == nil) return false;
    wk.webkit_web_view_reload(m_web_view);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// JavaScript evaluation

// Shared finish helper: extracts string result from a JSCValue*, stores it in
// the browser's m_js_result, signals completion.
void JSFinishWithValue(MCWebKitGTKBrowser *t_browser, JSCValue *t_value, GError *t_error)
{
    if (t_error != nil)
    {
        wk.g_error_free(t_error);
    }
    else if (t_value != nil && wk.jsc_value_is_string && wk.jsc_value_to_string)
    {
        if (wk.jsc_value_is_string(t_value) &&
            !wk.jsc_value_is_null(t_value) &&
            !wk.jsc_value_is_undefined(t_value))
        {
            gchar *t_str = wk.jsc_value_to_string(t_value);
            if (t_str)
            {
                if (t_browser->m_js_result)
                    wk.g_free(t_browser->m_js_result);
                t_browser->m_js_result = t_str; // caller owns
            }
        }
        if (t_value && !wk.is4_1)
        {
            // In 4.0, JSCValue is ref-counted separately
            // (caller already unreffed WebKitJavascriptResult if needed)
        }
    }
    t_browser->m_js_finished = true;
    MCBrowserRunloopBreakWait();
}

void MCWebKitGTKBrowser::on_js_finished_41(GObject *p_source, GAsyncResult *p_result, gpointer p_data)
{
    MCWebKitGTKBrowser *t_browser = (MCWebKitGTKBrowser*)p_data;
    GError *t_error = nil;
    void *t_value = wk.webkit_web_view_evaluate_javascript_finish(
        (WebKitWebView*)p_source, p_result, &t_error);
    JSFinishWithValue(t_browser, (JSCValue*)t_value, t_error);
    if (t_value) wk.g_object_unref(t_value);
}

void MCWebKitGTKBrowser::on_js_finished_40(GObject *p_source, GAsyncResult *p_result, gpointer p_data)
{
    MCWebKitGTKBrowser *t_browser = (MCWebKitGTKBrowser*)p_data;
    GError *t_error = nil;
    WebKitJavascriptResult *t_result = wk.webkit_web_view_run_javascript_finish(
        (WebKitWebView*)p_source, p_result, &t_error);
    JSCValue *t_value = nil;
    if (t_result && wk.webkit_javascript_result_get_js_value)
        t_value = wk.webkit_javascript_result_get_js_value(t_result);
    JSFinishWithValue(t_browser, t_value, t_error);
    // WebKitJavascriptResult is not a GObject; no unref needed in 4.0.
}

bool MCWebKitGTKBrowser::EvaluateJavaScript(const char *p_script, char *&r_result)
{
    if (m_web_view == nil || p_script == nil)
        return false;

    m_js_finished = false;
    if (m_js_result) { wk.g_free(m_js_result); m_js_result = nil; }

    if (wk.is4_1 && wk.webkit_web_view_evaluate_javascript)
    {
        wk.webkit_web_view_evaluate_javascript(m_web_view, p_script, -1,
            nil, nil, nil, (GAsyncReadyCallback)on_js_finished_41, this);
    }
    else if (wk.webkit_web_view_run_javascript)
    {
        wk.webkit_web_view_run_javascript(m_web_view, p_script, nil,
            (GAsyncReadyCallback)on_js_finished_40, this);
    }
    else
    {
        return false;
    }

    // Pump the runloop until the async callback fires.  MCBrowserRunloopWait()
    // processes pending GTK/engine events so WebKit can make progress.
    // The 5000-iteration guard prevents a permanent hang if the web process
    // crashes or never responds.
    int t_guard = 5000;
    while (!m_js_finished && t_guard-- > 0)
        MCBrowserRunloopWait();

    if (m_js_finished && m_js_result != nil)
    {
        // Transfer ownership to caller (MCCStringClone makes a LiveCode-heap copy)
        MCCStringClone(m_js_result, r_result);
        wk.g_free(m_js_result);
        m_js_result = nil;
        return true;
    }

    // Evaluation produced no string result (void expression, null, etc.)
    MCCStringClone("", r_result);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// JavaScript handler injection

void MCWebKitGTKBrowser::SyncJavaScriptHandlers()
{
    if (m_web_view == nil || m_content_manager == nil)
        return;
    if (!wk.webkit_user_content_manager_remove_all_scripts ||
        !wk.webkit_user_script_new || !wk.webkit_user_content_manager_add_script)
        return;

    wk.webkit_user_content_manager_remove_all_scripts(m_content_manager);

    if (MCCStringIsEmpty(m_js_handlers))
        return;

    // Build the injection script.
    // For each newline-delimited handler name, inject:
    //   window.liveCode.<name> = function() {
    //     window.webkit.messageHandlers.liveCode.postMessage(
    //       JSON.stringify(['<name>', Array.prototype.slice.call(arguments)]));
    //   };
    // The on_script_message callback receives the JSON string and parses it.
    size_t t_buf_size = 256;
    char *t_script = nil;
    MCBrowserMemoryAllocate(t_buf_size, (void *&)t_script);
    if (t_script == nil)
        return;

    strcpy(t_script, "window.liveCode = window.liveCode || {};");
    size_t t_pos = strlen(t_script);

    const char *p = m_js_handlers;
    while (*p)
    {
        while (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t') p++;
        if (!*p) break;
        const char *t_name_start = p;
        while (*p && *p != '\n' && *p != '\r') p++;
        size_t t_name_len = p - t_name_start;
        if (t_name_len == 0) continue;

        size_t t_needed = t_pos + t_name_len * 3 + 256;
        if (t_needed > t_buf_size)
        {
            t_buf_size = t_needed * 2;
            MCBrowserMemoryReallocate(t_script, t_buf_size, (void *&)t_script);
            if (t_script == nil) return;
        }

        t_pos += snprintf(t_script + t_pos, t_buf_size - t_pos,
            "window.liveCode.%.*s=function(){"
            "window.webkit.messageHandlers.liveCode.postMessage("
            "JSON.stringify(['%.*s',Array.prototype.slice.call(arguments)]));};",
            (int)t_name_len, t_name_start,
            (int)t_name_len, t_name_start);
    }

    WebKitUserScript *t_user_script = wk.webkit_user_script_new(
        t_script,
        WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
        nil, nil);

    if (t_user_script != nil)
    {
        wk.webkit_user_content_manager_add_script(m_content_manager, t_user_script);
        wk.webkit_user_script_unref(t_user_script);
    }

    MCBrowserMemoryDeallocate(t_script);
}

////////////////////////////////////////////////////////////////////////////////
// Signal handlers

// Runtime GObject type check: returns true if p is a live GObject whose GType
// name contains "JSCValue".  This guards calls to jsc_value_is_string() etc.
// against mismatched API versions where the signal may pass a different pointer
// type (e.g. WebKitJavascriptResult* when is4_1 is incorrectly detected true).
static bool IsJSCValue(void *p)
{
    if (!p)
        return false;
    GTypeInstance *inst = (GTypeInstance *)p;
    if (!inst->g_class)
        return false;
    if (!wk.g_type_name)
        return true; // can't verify; assume caller knows what it's doing
    const gchar *tn = wk.g_type_name(inst->g_class->g_type);
    return tn && strstr(tn, "JSCValue");
}

void MCWebKitGTKBrowser::on_load_changed(WebKitWebView *p_view, int p_event, gpointer p_data)
{
    MCWebKitGTKBrowser *t_browser = (MCWebKitGTKBrowser*)p_data;
    const char *t_uri = wk.webkit_web_view_get_uri(p_view);
    if (t_uri == nil) t_uri = "";

    switch (p_event)
    {
        case WEBKIT_LOAD_STARTED:
            t_browser->OnNavigationBegin(false, t_uri);
            break;
        case WEBKIT_LOAD_COMMITTED:
            t_browser->OnDocumentLoadBegin(false, t_uri);
            // Sync handlers after each navigation so they are active for the new page.
            t_browser->SyncJavaScriptHandlers();
            break;
        case WEBKIT_LOAD_FINISHED:
            t_browser->OnDocumentLoadComplete(false, t_uri);
            t_browser->OnNavigationComplete(false, t_uri);
            break;
    }
}

int MCWebKitGTKBrowser::on_load_failed(WebKitWebView *p_view, int /*p_event*/,
                                         char *p_failing_uri, GError *p_error, gpointer p_data)
{
    MCWebKitGTKBrowser *t_browser = (MCWebKitGTKBrowser*)p_data;
    const char *t_msg = (p_error && p_error->message) ? p_error->message : "unknown error";
    t_browser->OnDocumentLoadFailed(false, p_failing_uri ? p_failing_uri : "", t_msg);
    t_browser->OnNavigationFailed(false, p_failing_uri ? p_failing_uri : "", t_msg);
    return 0; // FALSE — let WebKit show error page
}

static bool UriIsNavigable(const char *p_uri)
{
    if (p_uri == nil) return false;
    static const char *s_nav[] = {
        "http://", "https://", "file://", "about:", "data:", "blob:", "javascript:",
    };
    for (size_t i = 0; i < sizeof(s_nav)/sizeof(s_nav[0]); i++)
        if (strncasecmp(p_uri, s_nav[i], strlen(s_nav[i])) == 0)
            return true;
    return false;
}

int MCWebKitGTKBrowser::on_decide_policy(WebKitWebView * /*p_view*/,
                                           WebKitPolicyDecision *p_decision,
                                           int p_type, gpointer p_data)
{
    MCWebKitGTKBrowser *t_browser = (MCWebKitGTKBrowser*)p_data;

    if (p_type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION)
    {
        // Off-screen WebKitWebView instances often dispatch link clicks as
        // NEW_WINDOW_ACTION rather than NAVIGATION_ACTION.  Calling
        // webkit_policy_decision_use() tells WebKit to proceed with the
        // new-window request, which causes it to emit the "create" signal.
        // Our on_create handler intercepts that signal, loads the URL in the
        // current view, and returns NULL to prevent an actual new window.
        wk.webkit_policy_decision_use(p_decision);
        return 1; // TRUE — handled
    }

    if (p_type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
    {
        WebKitNavigationAction *t_action =
            wk.webkit_navigation_policy_decision_get_navigation_action(
                (WebKitNavigationPolicyDecision*)p_decision);
        WebKitURIRequest *t_req = wk.webkit_navigation_action_get_request(t_action);
        const char *t_uri = wk.webkit_uri_request_get_uri(t_req);

        if (!UriIsNavigable(t_uri))
        {
            t_browser->OnNavigationRequestUnhandled(false, t_uri ? t_uri : "");
            wk.webkit_policy_decision_ignore(p_decision);
            return 1; // TRUE — handled
        }

        // Explicitly allow the navigation.  Do not rely on WebKit's default
        // class handler calling webkit_policy_decision_use() — in some builds
        // it is not called when the view has no visible on-screen context.
        wk.webkit_policy_decision_use(p_decision);
        return 1; // TRUE — handled
    }

    // Any other decision type (e.g. RESPONSE_POLICY): allow by default.
    wk.webkit_policy_decision_use(p_decision);
    return 1;
}

// static
GtkWidget* MCWebKitGTKBrowser::on_create(WebKitWebView * /*p_view*/,
                                           WebKitNavigationAction *p_action,
                                           gpointer p_data)
{
    MCWebKitGTKBrowser *t_browser = (MCWebKitGTKBrowser*)p_data;

    // WebKit emits "create" when it wants to open a new window.  For off-screen
    // WebKitWebView instances (hosted in GTK_WINDOW_POPUP at an off-screen
    // position), link clicks frequently arrive as NEW_WINDOW_ACTION in
    // decide-policy, which then triggers this signal.
    //
    // Strategy: redirect the navigation into the current view by calling
    // webkit_web_view_load_uri(), then return NULL to suppress the actual
    // new window.  Returning NULL is safe — WebKit treats it as "new window
    // creation declined" and does not crash.
    if (p_action && wk.webkit_navigation_action_get_request)
    {
        WebKitURIRequest *t_req = wk.webkit_navigation_action_get_request(p_action);
        if (t_req && wk.webkit_uri_request_get_uri)
        {
            const char *t_uri = wk.webkit_uri_request_get_uri(t_req);
            if (t_uri && t_uri[0] && wk.webkit_web_view_load_uri)
                wk.webkit_web_view_load_uri(t_browser->m_web_view, t_uri);
        }
    }

    // Return NULL — no actual new window.
    return NULL;
}

// ---- SimulateClick helpers -----------------------------------------------
// HXTNavCtx bridges the JS-evaluation callback back to the WebView pointer.
struct HXTNavCtx { WebKitWebView *view; };

// WebKit 4.1 callback: evaluate_javascript_finish → load_uri
static void hxt_nav_done_41(GObject *source, GAsyncResult *result, gpointer user_data)
{
    HXTNavCtx *ctx = (HXTNavCtx*)user_data;
    GError *err = NULL;
    JSCValue *val = (JSCValue*)wk.webkit_web_view_evaluate_javascript_finish(
        (WebKitWebView*)source, result, &err);
    if (val)
    {
        if (wk.jsc_value_is_string && wk.jsc_value_to_string &&
            wk.jsc_value_is_string(val))
        {
            gchar *url = wk.jsc_value_to_string(val);
            // "HXT:..." strings are internal SimulateClick return values —
            // do not treat as a URL to navigate to.
            if (url && url[0] && strncmp(url, "HXT:", 4) != 0 &&
                wk.webkit_web_view_load_uri)
                wk.webkit_web_view_load_uri(ctx->view, url);
            if (url) wk.g_free(url);
        }
        wk.g_object_unref(val);
    }
    if (err) wk.g_error_free(err);
    delete ctx;
}

// WebKit 4.0 callback: run_javascript_finish → load_uri
static void hxt_nav_done_40(GObject *source, GAsyncResult *result, gpointer user_data)
{
    HXTNavCtx *ctx = (HXTNavCtx*)user_data;
    GError *err = NULL;
    WebKitJavascriptResult *res = wk.webkit_web_view_run_javascript_finish(
        (WebKitWebView*)source, result, &err);
    if (res && wk.webkit_javascript_result_get_js_value)
    {
        JSCValue *val = wk.webkit_javascript_result_get_js_value(res);
        if (val && wk.jsc_value_is_string && wk.jsc_value_to_string &&
            wk.jsc_value_is_string(val))
        {
            gchar *url = wk.jsc_value_to_string(val);
            if (url && url[0] && strncmp(url, "HXT:", 4) != 0 &&
                wk.webkit_web_view_load_uri)
                wk.webkit_web_view_load_uri(ctx->view, url);
            if (url) wk.g_free(url);
        }
        // WebKitJavascriptResult is not a GObject; no unref in 4.0.
    }
    if (err) wk.g_error_free(err);
    delete ctx;
}
// --------------------------------------------------------------------------

// static
void MCWebKitGTKBrowser::SimulateClick(void *ctx, int x, int y)
{
    MCWebKitGTKBrowser *b = static_cast<MCWebKitGTKBrowser*>(ctx);
    if (!b || !b->m_web_view) return;

    // Run JS that finds the nearest anchor to (x,y) and returns its absolute
    // href as a string.  We convert device pixels → CSS pixels via
    // devicePixelRatio so HiDPI displays are handled correctly.
    // The callback (hxt_nav_done_41/40) receives the string and calls
    // webkit_web_view_load_uri() directly — no message-handler round-trip.
    // Buffer sized for the JS below (~1300 chars + two ints).
    char js[2048];
    snprintf(js, sizeof(js),
        "(function(x,y){"
          // doPaint() renders at physical resolution so HXT pixel == CSS pixel.
          // Use x/y directly; no dpr division.
          "var cx=x,cy=y;"
          "var el=document.elementFromPoint(cx,cy);"
          "if(!el)return'HXT:null';"
          // isField: INPUT/TEXTAREA/SELECT or any contenteditable element.
          // Check the attribute directly as a fallback for old WebKit where
          // isContentEditable may not recognise plaintext-only.
          "function F(e){"
            "var t=e.nodeName,ce=e.getAttribute?e.getAttribute('contenteditable'):null;"
            "return t==='INPUT'||t==='TEXTAREA'||t==='SELECT'"
              "||e.isContentEditable||(ce&&ce!=='false');"
          "}"
          // Walk up from e until a focusable element is found.
          "function W(e){while(e&&!F(e))e=e.parentElement;return e;}"
          "var f=null;"
          // elementsFromPoint (plural) returns ALL elements at (cx,cy) in
          // z-order — lets us find an INPUT that lies underneath an IFRAME
          // or other overlay without needing to pierce iframes.
          "var cs=document.elementsFromPoint"
            "?document.elementsFromPoint(cx,cy):[el];"
          "for(var i=0;!f&&i<cs.length;i++)"
            "f=F(cs[i])?cs[i]:W(cs[i].parentElement);"
          // If the topmost element is a same-origin IFRAME and we still
          // haven't found a field, recurse into the iframe document.
          "if(!f&&el.nodeName==='IFRAME'){"
            "try{"
              "var r=el.getBoundingClientRect(),id=el.contentDocument;"
              "if(id)f=W(id.elementFromPoint(cx-r.left,cy-r.top));"
            "}catch(e2){}"
          "}"
          "if(f){"
            // For form fields we only call .focus() — no synthetic mouse events.
            // Dispatching a JS click on an INPUT moves WebKit's sequential focus
            // navigation starting point (SFNSP) to the NEXT element, so the first
            // TAB keypress would land two elements ahead instead of one.  Calling
            // .focus() alone focuses the element and resets the SFNSP to f itself.
            "f.focus();"
            "return'HXT:field';"
          "}"
          // Walk up for an anchor — return the href so the C++ callback can
          // navigate via webkit_web_view_load_uri().
          "var a=el;"
          "while(a&&a.nodeName!=='A')a=a.parentElement;"
          "if(a&&a.href&&a.href.indexOf('javascript:')!==0)return a.href;"
          // For all other elements (buttons, divs, custom components, …) the
          // real GDK button-press + button-release events have already been
          // forwarded by OnMouseUp via forwardPointerEvent(), which causes
          // WebKit to fire trusted mousedown / mouseup / click events.  Those
          // are sufficient to activate the element.  Dispatching an additional
          // synthetic MouseEvent('click') here causes JS menus that open on
          // click to immediately close (their document-level dismiss listener
          // fires on the extra synthetic click) — appearing as if nothing
          // happened.  Do nothing; let the native events do the work.
          "return'HXT:click';"
        "})(%d,%d)", x, y);

    HXTNavCtx *nav_ctx = new HXTNavCtx;
    nav_ctx->view = b->m_web_view;

    if (wk.webkit_web_view_evaluate_javascript)
        wk.webkit_web_view_evaluate_javascript(
            b->m_web_view, js, (gssize)-1, NULL, NULL, NULL,
            (GAsyncReadyCallback)hxt_nav_done_41, nav_ctx);
    else if (wk.webkit_web_view_run_javascript)
        wk.webkit_web_view_run_javascript(
            b->m_web_view, js, NULL,
            (GAsyncReadyCallback)hxt_nav_done_40, nav_ctx);
    else
        delete nav_ctx;
}

// ---- show-option-menu callbacks ------------------------------------------

// Called when the user activates a menu item in our custom <select> popup.
// The item's option index and WebKitOptionMenu pointer are stored as GObject
// data so we don't need heap-allocated per-item context structs.
void MCWebKitGTKBrowser::on_option_menu_item_activate(GtkMenuItem *p_item, gpointer /*p_data*/)
{
    WebKitOptionMenu *t_menu =
        (WebKitOptionMenu*)g_object_get_data(G_OBJECT(p_item), "hxt-opt-menu");
    guint t_idx =
        (guint)GPOINTER_TO_INT(g_object_get_data(G_OBJECT(p_item), "hxt-opt-idx"));
    if (t_menu && wk.webkit_option_menu_activate_item)
        wk.webkit_option_menu_activate_item(t_menu, t_idx);
}

// Called when our custom GtkMenu is hidden (user picked an item or dismissed).
// Tells WebKit we're done with the option menu and destroys the GtkMenu.
// NOTE: only used as a fallback.  The primary path calls webkit_option_menu_close
// and gtk_widget_destroy directly after gtk_main() returns.
void MCWebKitGTKBrowser::on_option_menu_hidden(GtkWidget *p_widget, gpointer p_data)
{
    WebKitOptionMenu *t_menu = (WebKitOptionMenu*)p_data;
    if (t_menu && wk.webkit_option_menu_close)
        wk.webkit_option_menu_close(t_menu);
    wk.gtk_widget_destroy(p_widget);
}

// Called when the GtkMenu shell deactivates (user picked an item or dismissed).
// Quits the nested gtk_main() loop started by on_show_option_menu so that
// HXT's event loop can resume.
static void hxt_option_menu_deactivate(GtkMenuShell * /*shell*/, gpointer /*data*/)
{
    gtk_main_quit();
}

// static
// Called by native-layer-x11::doPaint() (via hxt-snap-fn bridge) when
// gtk_widget_draw() produces a blank surface — happens on WebKit 2.44+ (Fedora)
// where the compositor bypasses Cairo.
void MCWebKitGTKBrowser::RequestSnapshot(void *ctx)
{
    MCWebKitGTKBrowser *b = static_cast<MCWebKitGTKBrowser*>(ctx);
    if (!b || !b->m_web_view) return;
    if (b->m_snapshot_pending) return;
    if (!wk.webkit_web_view_get_snapshot) return;

    b->m_snapshot_pending = true;
    // WEBKIT_SNAPSHOT_REGION_VISIBLE=0, WEBKIT_SNAPSHOT_OPTIONS_NONE=0
    wk.webkit_web_view_get_snapshot(b->m_web_view, 0, 0, nil,
        (GAsyncReadyCallback)on_snapshot_done, b);
}

// static
// Async completion for webkit_web_view_get_snapshot().
// Stores the surface as "hxt-snapshot" on the web view, then calls
// hxt-repaint-fn to schedule an engine Redraw() so doPaint() blits it.
void MCWebKitGTKBrowser::on_snapshot_done(GObject *source, GAsyncResult *result,
                                           gpointer user_data)
{
    MCWebKitGTKBrowser *b = static_cast<MCWebKitGTKBrowser*>(user_data);
    if (!b || !b->m_web_view) return;
    b->m_snapshot_pending = false;

    GError *t_error = nil;
    cairo_surface_t *t_surf =
        wk.webkit_web_view_get_snapshot_finish((WebKitWebView*)source, result, &t_error);
    if (t_error) { wk.g_error_free(t_error); return; }
    if (!t_surf) return;

    // Replace any previously cached snapshot.
    cairo_surface_t *t_old =
        (cairo_surface_t*)g_object_get_data(G_OBJECT(b->m_web_view), "hxt-snapshot");
    if (t_old)
        cairo_surface_destroy(t_old);
    g_object_set_data(G_OBJECT(b->m_web_view), "hxt-snapshot", (gpointer)t_surf);

    // Wake the engine paint loop via the bridge registered by doAttach().
    typedef void (*RepaintFn)(void*);
    RepaintFn fn    = (RepaintFn)g_object_get_data(G_OBJECT(b->m_web_view), "hxt-repaint-fn");
    void     *fn_ctx =           g_object_get_data(G_OBJECT(b->m_web_view), "hxt-repaint-ctx");
    if (fn && fn_ctx)
        fn(fn_ctx);
}

// static
// Signal: "show-option-menu" on the WebKitWebView.
// Fired when WebKit wants to display a native <select> dropdown.
// p_rect is the bounding rect of the <select> element in WebView widget coords.
// Returns TRUE to tell WebKit we handled the popup (suppresses the default
// native GTK menu that would appear at screen position 0,0 because the
// GtkOffscreenWindow has no real screen position).
gboolean MCWebKitGTKBrowser::on_show_option_menu(WebKitWebView *p_view,
                                                   WebKitOptionMenu *p_menu,
                                                   GdkEvent *p_event,
                                                   GdkRectangle *p_rect,
                                                   gpointer /*p_data*/)
{
    if (!wk.webkit_option_menu_get_n_items || !wk.gtk_menu_new ||
        !wk.gtk_menu_item_new_with_label || !wk.gtk_menu_shell_append ||
        !wk.gtk_menu_popup_at_rect)
    {
        return FALSE;
    }

    // Retrieve the widget's absolute screen position stored by
    // native-layer-x11::doSetGeometry via g_object_set_data.
    int t_screen_x = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(p_view), "hxt-screen-x"));
    int t_screen_y = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(p_view), "hxt-screen-y"));
    GdkWindow *t_stack_win = (GdkWindow*)g_object_get_data(G_OBJECT(p_view), "hxt-stack-win");

    // Build GtkMenu from WebKitOptionMenu items.
    GtkWidget *t_gtk_menu = wk.gtk_menu_new();
    if (!t_gtk_menu)
        return FALSE;

    guint t_n = wk.webkit_option_menu_get_n_items(p_menu);
    for (guint i = 0; i < t_n; i++)
    {
        WebKitOptionMenuItem *t_item = wk.webkit_option_menu_get_item(p_menu, i);
        if (!t_item) continue;

        GtkWidget *t_menu_item = NULL;
        bool t_is_group = wk.webkit_option_menu_item_is_group_label &&
                          wk.webkit_option_menu_item_is_group_label(t_item);

        if (t_is_group)
        {
            t_menu_item = wk.gtk_separator_menu_item_new
                        ? wk.gtk_separator_menu_item_new() : NULL;
        }
        else
        {
            const gchar *t_label = wk.webkit_option_menu_item_get_label
                                 ? wk.webkit_option_menu_item_get_label(t_item) : "";
            t_menu_item = wk.gtk_menu_item_new_with_label(t_label ? t_label : "");
            if (t_menu_item)
            {
                // Grey out disabled items.
                if (wk.webkit_option_menu_item_is_enabled &&
                    !wk.webkit_option_menu_item_is_enabled(t_item))
                    wk.gtk_widget_set_sensitive(t_menu_item, FALSE);

                // Store index + menu on the item so the activate callback can
                // find them without a separate heap allocation.
                g_object_set_data(G_OBJECT(t_menu_item), "hxt-opt-idx",
                    GINT_TO_POINTER((gint)i));
                g_object_set_data(G_OBJECT(t_menu_item), "hxt-opt-menu",
                    (gpointer)p_menu);
                wk.g_signal_connect_data(t_menu_item, "activate",
                    G_CALLBACK(on_option_menu_item_activate),
                    NULL, NULL, (GConnectFlags)0);
            }
        }

        if (t_menu_item)
        {
            wk.gtk_menu_shell_append((GtkMenuShell*)t_gtk_menu, t_menu_item);
            wk.gtk_widget_show(t_menu_item);
        }
    }

    // Connect "deactivate" to quit the nested main loop when the menu closes.
    // "deactivate" fires before "hide" so gtk_main_quit() is called while the
    // menu widget is still valid.
    wk.g_signal_connect_data(t_gtk_menu, "deactivate",
        G_CALLBACK(hxt_option_menu_deactivate), NULL, NULL, (GConnectFlags)0);

    // Compute the anchor rectangle in the coordinate space of t_stack_win.
    GdkRectangle t_anchor = {
        t_screen_x + (p_rect ? p_rect->x : 0),
        t_screen_y + (p_rect ? p_rect->y : 0),
        p_rect ? p_rect->width  : 1,
        p_rect ? p_rect->height : 1
    };

    GdkWindow *t_anchor_win = t_stack_win;
    if (t_anchor_win && GDK_IS_WINDOW(t_anchor_win))
    {
        int t_origin_x = 0, t_origin_y = 0;
        gdk_window_get_origin(t_anchor_win, &t_origin_x, &t_origin_y);
        t_anchor.x = (t_screen_x - t_origin_x) + (p_rect ? p_rect->x : 0);
        t_anchor.y = (t_screen_y - t_origin_y) + (p_rect ? p_rect->y : 0);
        t_anchor.width  = p_rect ? p_rect->width  : 1;
        t_anchor.height = p_rect ? p_rect->height : 1;
    }
    else
    {
        t_anchor_win = gdk_get_default_root_window();
    }

    wk.gtk_menu_popup_at_rect(
        (GtkMenu*)t_gtk_menu,
        t_anchor_win,
        &t_anchor,
        GDK_GRAVITY_SOUTH_WEST,
        GDK_GRAVITY_NORTH_WEST,
        p_event);

    // Run a nested GTK main loop for the duration of the popup.
    //
    // GtkMenu's hover tracking, pointer grab, and CSS state (prelight/active)
    // all depend on GTK's own event dispatch, not HXT's custom event loop.
    // Calling gtk_main() here hands full event control to GTK until the menu
    // is dismissed.  hxt_option_menu_deactivate (above) calls gtk_main_quit()
    // when the "deactivate" signal fires, returning control here.
    //
    // Nested gtk_main() calls are explicitly supported by GTK and are the
    // standard mechanism for modal popups and combo-box dropdowns.
    gtk_main();

    // Notify WebKit the option menu is closed and destroy the GtkMenu widget.
    if (wk.webkit_option_menu_close)
        wk.webkit_option_menu_close(p_menu);
    wk.gtk_widget_destroy(t_gtk_menu);

    return TRUE; // we handled it; suppress WebKit's default popup
}

// --------------------------------------------------------------------------

int MCWebKitGTKBrowser::on_context_menu(WebKitWebView * /*p_view*/, gpointer /*p_menu*/,
                                          gpointer /*p_event*/, gpointer /*p_hit_test*/,
                                          gpointer p_data)
{
    MCWebKitGTKBrowser *t_browser = (MCWebKitGTKBrowser*)p_data;
    return t_browser->m_enable_context_menu ? 0 : 1;
}

void MCWebKitGTKBrowser::on_progress_changed(GObject *p_obj, GParamSpec * /*p_pspec*/, gpointer p_data)
{
    MCWebKitGTKBrowser *t_browser = (MCWebKitGTKBrowser*)p_data;
    double t_progress = wk.webkit_web_view_get_estimated_load_progress((WebKitWebView*)p_obj);
    const char *t_uri = wk.webkit_web_view_get_uri((WebKitWebView*)p_obj);
    t_browser->OnProgressChanged(t_uri ? t_uri : "", (uint32_t)(t_progress * 100));
}

// static
// Called when the injected click-interceptor script posts an anchor's href
// via window.webkit.messageHandlers.hxtNav.postMessage(url).
// We navigate the current view directly with webkit_web_view_load_uri(),
// bypassing decide-policy and all GDK event routing.
void MCWebKitGTKBrowser::on_nav_message(WebKitUserContentManager * /*p_mgr*/,
                                          gpointer p_js_result, gpointer p_data)
{
    MCWebKitGTKBrowser *t_browser = (MCWebKitGTKBrowser*)p_data;

    // In webkit2gtk 4.1 the signal passes JSCValue* directly.
    // In 4.0 it passes WebKitJavascriptResult*; unwrap to JSCValue*.
    // Guard with IsJSCValue() in case API version detection is wrong on a
    // given system — fall back to the 4.0 unwrap path rather than crashing.
    JSCValue *t_value = nil;
    if (wk.is4_1 && IsJSCValue(p_js_result))
        t_value = (JSCValue*)p_js_result;
    else if (wk.webkit_javascript_result_get_js_value && !IsJSCValue(p_js_result))
        t_value = wk.webkit_javascript_result_get_js_value((WebKitJavascriptResult*)p_js_result);

    if (t_value == nil || !wk.jsc_value_is_string || !wk.jsc_value_to_string)
        return;
    if (!wk.jsc_value_is_string(t_value))
        return;

    gchar *t_url = wk.jsc_value_to_string(t_value);
    if (t_url && t_url[0] && wk.webkit_web_view_load_uri)
        wk.webkit_web_view_load_uri(t_browser->m_web_view, t_url);
    if (t_url)
        wk.g_free(t_url);
}

void MCWebKitGTKBrowser::on_script_message(WebKitUserContentManager * /*p_mgr*/,
                                             gpointer p_js_result, gpointer p_data)
{
    MCWebKitGTKBrowser *t_browser = (MCWebKitGTKBrowser*)p_data;

    // In webkit2gtk 4.1 the signal passes JSCValue* directly.
    // In 4.0 it passes WebKitJavascriptResult*; unwrap to JSCValue*.
    // Guard with IsJSCValue() so a mis-detected API version doesn't crash.
    JSCValue *t_value = nil;
    if (IsJSCValue(p_js_result))
        t_value = (JSCValue*)p_js_result;
    else if (wk.webkit_javascript_result_get_js_value)
        t_value = wk.webkit_javascript_result_get_js_value((WebKitJavascriptResult*)p_js_result);

    if (t_value == nil || !wk.jsc_value_is_string || !wk.jsc_value_to_string)
        return;
    if (!wk.jsc_value_is_string(t_value))
        return;

    gchar *t_json = wk.jsc_value_to_string(t_value);
    if (t_json == nil) return;

    // Parse JSON array: ['handlerName', [arg1, arg2, ...]]
    // Extract handler name (first quoted string in the outer array).
    char *p = t_json;
    while (*p && *p != '[') p++;
    if (!*p) { wk.g_free(t_json); return; }
    p++; // skip '['
    while (*p && (*p == ' ' || *p == '\t')) p++;
    if (*p != '"') { wk.g_free(t_json); return; }
    p++; // skip opening quote

    char *t_name_start = p;
    while (*p && *p != '"') p++;
    size_t t_name_len = p - t_name_start;

    char *t_handler_name = nil;
    if (!MCCStringCloneSubstring(t_name_start, t_name_len, t_handler_name))
    {
        wk.g_free(t_json);
        return;
    }

    // Pass an empty argument list for now.
    // Full JSON array parsing of the args can be added here if needed.
    MCBrowserListRef t_args = nil;
    MCBrowserListCreate(t_args, 0);
    t_browser->OnJavaScriptCall(t_handler_name, t_args);
    MCBrowserListRelease(t_args);
    MCCStringFree(t_handler_name);
    wk.g_free(t_json);
}

////////////////////////////////////////////////////////////////////////////////
// Property helpers

bool MCWebKitGTKBrowser::GetUrl(char *&r_url)
{
    if (m_web_view == nil) return false;
    const char *t_uri = wk.webkit_web_view_get_uri(m_web_view);
    return MCCStringClone(t_uri ? t_uri : "", r_url);
}

bool MCWebKitGTKBrowser::GetHTMLText(char *&r_htmltext)
{
    return EvaluateJavaScript("document.documentElement.outerHTML", r_htmltext);
}

bool MCWebKitGTKBrowser::GetUserAgent(char *&r_useragent)
{
    if (m_web_view == nil) return false;
    WebKitSettings *t_settings = wk.webkit_web_view_get_settings(m_web_view);
    if (t_settings == nil) return false;
    const char *t_ua = wk.webkit_settings_get_user_agent(t_settings);
    return MCCStringClone(t_ua ? t_ua : "", r_useragent);
}

bool MCWebKitGTKBrowser::SetUserAgent(const char *p_useragent)
{
    if (m_web_view == nil) return false;
    if (!wk.webkit_settings_set_user_agent) return true; // no-op
    WebKitSettings *t_settings = wk.webkit_web_view_get_settings(m_web_view);
    if (t_settings == nil) return false;
    wk.webkit_settings_set_user_agent(t_settings, p_useragent);
    return true;
}

bool MCWebKitGTKBrowser::GetIsSecure(bool &r_value)
{
    if (m_web_view == nil) return false;
    if (!wk.webkit_web_view_get_tls_info)
    {
        // Fallback: check URI prefix
        const char *t_uri = wk.webkit_web_view_get_uri(m_web_view);
        r_value = t_uri && strncmp(t_uri, "https:", 6) == 0;
        return true;
    }
    guint t_errors = 0;
    if (!wk.webkit_web_view_get_tls_info(m_web_view, nil, &t_errors))
    {
        r_value = false;
        return true;
    }
    r_value = (t_errors == 0);
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// Property dispatch

bool MCWebKitGTKBrowser::GetBoolProperty(MCBrowserProperty p_property, bool &r_value)
{
    switch (p_property)
    {
    case kMCBrowserCanGoBack:
        r_value = m_web_view && wk.webkit_web_view_can_go_back(m_web_view);
        return true;
    case kMCBrowserCanGoForward:
        r_value = m_web_view && wk.webkit_web_view_can_go_forward(m_web_view);
        return true;
    case kMCBrowserIsSecure:
        return GetIsSecure(r_value);
    case kMCBrowserAllowNewWindows:
        r_value = m_allow_new_windows;
        return true;
    case kMCBrowserEnableContextMenu:
        r_value = m_enable_context_menu;
        return true;
    case kMCBrowserAllowUserInteraction:
        r_value = m_web_view && wk.gtk_widget_is_sensitive &&
                  wk.gtk_widget_is_sensitive((GtkWidget*)m_web_view);
        return true;
    case kMCBrowserVerticalScrollbarEnabled:
    case kMCBrowserHorizontalScrollbarEnabled:
    case kMCBrowserScrollEnabled:
        r_value = true;
        return true;
    case kMCBrowserScrollCanBounce:
        r_value = false;
        return true;
    default:
        return true;
    }
}

bool MCWebKitGTKBrowser::SetBoolProperty(MCBrowserProperty p_property, bool p_value)
{
    switch (p_property)
    {
    case kMCBrowserAllowNewWindows:
        m_allow_new_windows = p_value;
        return true;
    case kMCBrowserEnableContextMenu:
        m_enable_context_menu = p_value;
        return true;
    case kMCBrowserAllowUserInteraction:
        if (m_web_view && wk.gtk_widget_set_sensitive)
            wk.gtk_widget_set_sensitive((GtkWidget*)m_web_view, p_value ? TRUE : FALSE);
        return true;
    case kMCBrowserVerticalScrollbarEnabled:
    case kMCBrowserHorizontalScrollbarEnabled:
    {
        // Implement via CSS on the document body
        const char *t_axis  = (p_property == kMCBrowserVerticalScrollbarEnabled)
                              ? "overflowY" : "overflowX";
        const char *t_value = p_value ? "auto" : "hidden";
        char t_script[128];
        snprintf(t_script, sizeof(t_script),
                 "document.body.style.%s='%s'", t_axis, t_value);
        char *t_result = nil;
        EvaluateJavaScript(t_script, t_result);
        if (t_result) MCCStringFree(t_result);
        return true;
    }
    default:
        return true;
    }
}

bool MCWebKitGTKBrowser::GetStringProperty(MCBrowserProperty p_property, char *&r_utf8_string)
{
    switch (p_property)
    {
    case kMCBrowserURL:            return GetUrl(r_utf8_string);
    case kMCBrowserHTMLText:       return GetHTMLText(r_utf8_string);
    case kMCBrowserUserAgent:      return GetUserAgent(r_utf8_string);
    case kMCBrowserJavaScriptHandlers:
        return MCCStringClone(m_js_handlers ? m_js_handlers : "", r_utf8_string);
    default:
        return true;
    }
}

bool MCWebKitGTKBrowser::SetStringProperty(MCBrowserProperty p_property, const char *p_utf8_string)
{
    switch (p_property)
    {
    case kMCBrowserURL:       return GoToURL(p_utf8_string);
    case kMCBrowserHTMLText:  return LoadHTMLText(p_utf8_string, nil);
    case kMCBrowserUserAgent: return SetUserAgent(p_utf8_string);
    case kMCBrowserJavaScriptHandlers:
    {
        if (m_js_handlers) { wk.g_free(m_js_handlers); m_js_handlers = nil; }
        if (!MCCStringIsEmpty(p_utf8_string))
            m_js_handlers = wk.fn_g_strdup ? wk.fn_g_strdup(p_utf8_string) : strdup(p_utf8_string);
        SyncJavaScriptHandlers();
        return true;
    }
    default:
        return true;
    }
}

bool MCWebKitGTKBrowser::GetIntegerProperty(MCBrowserProperty p_property, int32_t &r_value)
{
    switch (p_property)
    {
    case kMCBrowserDataDetectorTypes:
        r_value = 0;
        return true;
    default:
        return true;
    }
}

bool MCWebKitGTKBrowser::SetIntegerProperty(MCBrowserProperty /*p_property*/, int32_t /*p_value*/)
{
    return true;
}

////////////////////////////////////////////////////////////////////////////////
// GTK main context pumping
//
// WebKit2GTK drives rendering, network, and IPC through GLib's main context.
// Without regular iterations, the web process stalls and nothing renders.
// We hook into the browser runloop so this happens on every engine tick.

static void MCLinuxWebKitGTKRunloopAction(void * /*p_context*/)
{
    // Always drain the pipe — even if WebKit symbols aren't ready yet.
    // hxt_drain_stderr_pipe() is a no-op when s_stderr_filter is NULL.
    hxt_drain_stderr_pipe();

    if (!s_webkit_loaded || !wk.g_main_context_pending || !wk.g_main_context_iteration)
        return;
    while (wk.g_main_context_pending(nil))
        wk.g_main_context_iteration(nil, FALSE);
    // Drain again after the GLib iteration: any messages written to fd 2
    // during the iteration are now in the pipe buffer.
    hxt_drain_stderr_pipe();
}

static bool    s_runloop_added = false;

static void MCWebKitGTKLibraryInitialize()
{
    if (!s_runloop_added)
    {
        MCBrowserAddRunloopAction(MCLinuxWebKitGTKRunloopAction, nil);
        s_runloop_added = true;
    }
}

static void MCWebKitGTKLibraryFinalize()
{
    if (s_runloop_added)
    {
        MCBrowserRemoveRunloopAction(MCLinuxWebKitGTKRunloopAction, nil);
        s_runloop_added = false;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Factory

class MCWebKitGTKBrowserFactory : public MCBrowserFactory
{
public:
    MCWebKitGTKBrowserFactory() {}
    virtual ~MCWebKitGTKBrowserFactory()
    {
        MCWebKitGTKLibraryFinalize();
    }

    virtual bool CreateBrowser(void *p_display, void *p_parent_view,
                               MCBrowser *&r_browser)
    {
        MCWebKitGTKLibraryInitialize();

        MCWebKitGTKBrowser *t_browser = new (nothrow) MCWebKitGTKBrowser();
        if (t_browser == nil)
            return false;

        if (!t_browser->Init(p_display, p_parent_view))
        {
            delete t_browser;
            return false;
        }

        r_browser = t_browser;
        return true;
    }
};

bool MCWebKitGTKBrowserFactoryCreate(MCBrowserFactoryRef &r_factory)
{
    if (!LoadWebKit())
        return false;

    MCWebKitGTKBrowserFactory *t_factory = new (nothrow) MCWebKitGTKBrowserFactory();
    if (t_factory == nil)
        return false;

    r_factory = (MCBrowserFactoryRef)t_factory;
    return true;
}
