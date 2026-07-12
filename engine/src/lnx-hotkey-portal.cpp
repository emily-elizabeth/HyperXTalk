/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.  */

//
// XDG GlobalShortcuts portal backend for Wayland global hotkeys.
//
// This TU intentionally does NOT include prefix.h or any engine headers.
// All libdbus-1 types are forward-declared below so that neither the
// bundled GTK/GLib headers nor the system dbus headers are required at
// compile time; the actual symbols are loaded at runtime via the stub
// system (linux.stubs → linux.stubs.cpp).
//
// Protocol flow:
//   1. lnx_hotkey_portal_init():
//        CreateSession  → wait for Request.Response → store session_handle
//   2. lnx_hotkey_portal_register():
//        BindShortcuts (fire-and-forget; portal handles the key assignment)
//   3. Background thread:
//        poll for GlobalShortcuts.Activated signal → write engine_id to pipe
//

#include "lnx-hotkey-portal.h"

#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>

////////////////////////////////////////////////////////////////////////////////
// Desktop-file registration
//
// xdg-desktop-portal identifies callers by matching the process executable
// against installed .desktop files.  Without a match, GNOME's GlobalShortcuts
// backend rejects BindShortcuts with code=2.  We write a minimal entry to
// ~/.local/share/applications/ and refresh the database so the portal can
// identify HyperXTalk on the very first call.

static void _ensure_desktop_file(void)
{
    // Resolve the absolute path of our own executable.
    char exe_path[512];
    ssize_t r = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (r <= 0)
    {
        fprintf(stderr, "lnx-hotkey-portal: readlink /proc/self/exe failed\n");
        return;
    }
    exe_path[r] = '\0';

    const char *home = getenv("HOME");
    if (!home)
        return;

    // Ensure the target directory exists.
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.local/share/applications", home);
    {
        char cmd[600];
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s' 2>/dev/null", dir);
        (void)system(cmd);
    }

    // Write the desktop file.
    char path[768];
    snprintf(path, sizeof(path), "%s/hyperxtalk.desktop", dir);

    FILE *f = fopen(path, "w");
    if (!f)
    {
        fprintf(stderr, "lnx-hotkey-portal: could not write %s\n", path);
        return;
    }
    fprintf(f,
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Version=1.0\n"
        "Name=HyperXTalk\n"
        "Exec=%s %%U\n"
        "Categories=Development;IDE;\n"
        "StartupWMClass=HyperXTalk\n"
        "NoDisplay=true\n",
        exe_path);
    fclose(f);

    // Rebuild the GLib desktop database so xdg-desktop-portal sees the entry.
    {
        char cmd[800];
        snprintf(cmd, sizeof(cmd),
                 "update-desktop-database '%s' 2>/dev/null", dir);
        (void)system(cmd);
    }

    // xdg-desktop-portal identifies host apps via sd_pid_get_user_unit()
    // (systemd cgroup unit name), not via GIO_LAUNCHED_DESKTOP_FILE.
    // These env vars are set for completeness but have no effect on xdp 1.18+.
    setenv("GIO_LAUNCHED_DESKTOP_FILE", path, 1);
    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", (int)getpid());
    setenv("GIO_LAUNCHED_DESKTOP_FILE_PID", pid_str, 1);
}

////////////////////////////////////////////////////////////////////////////////
// libdbus-1 ABI declarations
// These are stable since dbus 1.0 and match the binary ABI exactly.

typedef unsigned int   dbus_uint32_t;
typedef int            dbus_bool_t;
typedef unsigned long long dbus_uint64_t;

// Opaque connection / message handles — we only ever hold pointers.
typedef struct DBusConnection_ DBusConnection;
typedef struct DBusMessage_    DBusMessage;

// DBusMessageIter has a stable layout (14 fields, same since 1.0).
typedef struct
{
    void         *dummy1;
    void         *dummy2;
    dbus_uint32_t dummy3;
    int           dummy4, dummy5, dummy6, dummy7, dummy8,
                  dummy9, dummy10, dummy11, dummy12, dummy13;
    void         *dummy14;
} DBusMessageIter;

// DBusError has a stable layout too.
typedef struct
{
    const char   *name;
    const char   *message;
    unsigned int  dummy1 : 1, dummy2 : 1, dummy3 : 1,
                  dummy4 : 1, dummy5 : 1;
    void         *padding1;
} DBusError;

// Bus type enum value for the session bus.
#define DBUS_BUS_SESSION 0

// Type codes (single ASCII chars used as integers).
#define DBUS_TYPE_INVALID    '\0'
#define DBUS_TYPE_BOOLEAN    'b'
#define DBUS_TYPE_UINT32     'u'
#define DBUS_TYPE_UINT64     't'
#define DBUS_TYPE_STRING     's'
#define DBUS_TYPE_OBJECT_PATH 'o'
#define DBUS_TYPE_ARRAY      'a'
#define DBUS_TYPE_VARIANT    'v'
#define DBUS_TYPE_DICT_ENTRY 'e'
#define DBUS_TYPE_STRUCT     'r'

// Stub function pointers — filled by initialise_weak_link_dbus().
extern "C" int initialise_weak_link_dbus(void);

extern "C"
{
    DBusConnection *dbus_bus_get(int bus_type, DBusError *error);
    void            dbus_error_init(DBusError *error);
    void            dbus_error_free(DBusError *error);
    dbus_bool_t     dbus_error_is_set(const DBusError *error);

    DBusMessage    *dbus_message_new_method_call(const char *destination,
                                                 const char *path,
                                                 const char *iface,
                                                 const char *method);
    void            dbus_message_unref(DBusMessage *msg);
    dbus_bool_t     dbus_message_is_signal(DBusMessage *msg,
                                           const char *iface,
                                           const char *signal_name);
    const char     *dbus_message_get_path(DBusMessage *msg);
    const char     *dbus_message_get_interface(DBusMessage *msg);
    const char     *dbus_message_get_member(DBusMessage *msg);
    int             dbus_message_get_type(DBusMessage *msg);

    void            dbus_message_iter_init_append(DBusMessage *msg,
                                                  DBusMessageIter *iter);
    dbus_bool_t     dbus_message_iter_append_basic(DBusMessageIter *iter,
                                                   int type,
                                                   const void *value);
    dbus_bool_t     dbus_message_iter_open_container(DBusMessageIter *iter,
                                                     int type,
                                                     const char *contained_sig,
                                                     DBusMessageIter *sub);
    dbus_bool_t     dbus_message_iter_close_container(DBusMessageIter *iter,
                                                      DBusMessageIter *sub);
    dbus_bool_t     dbus_message_iter_init(DBusMessage *msg,
                                           DBusMessageIter *iter);
    int             dbus_message_iter_get_arg_type(DBusMessageIter *iter);
    void            dbus_message_iter_get_basic(DBusMessageIter *iter,
                                                void *value);
    dbus_bool_t     dbus_message_iter_next(DBusMessageIter *iter);
    void            dbus_message_iter_recurse(DBusMessageIter *iter,
                                              DBusMessageIter *sub);

    DBusMessage    *dbus_connection_send_with_reply_and_block(
                        DBusConnection *conn, DBusMessage *msg,
                        int timeout_ms, DBusError *error);
    dbus_bool_t     dbus_connection_send(DBusConnection *conn,
                                         DBusMessage *msg,
                                         dbus_uint32_t *serial);
    dbus_bool_t     dbus_connection_read_write(DBusConnection *conn,
                                               int timeout_ms);
    DBusMessage    *dbus_connection_pop_message(DBusConnection *conn);
    void            dbus_connection_flush(DBusConnection *conn);

    void            dbus_bus_add_match(DBusConnection *conn,
                                       const char *rule,
                                       DBusError *error);
    void            dbus_free(void *ptr);
}

////////////////////////////////////////////////////////////////////////////////
// Portal constants

#define PORTAL_DEST    "org.freedesktop.portal.Desktop"
#define PORTAL_PATH    "/org/freedesktop/portal/desktop"
#define PORTAL_IFACE   "org.freedesktop.portal.GlobalShortcuts"
#define REQUEST_IFACE  "org.freedesktop.portal.Request"

////////////////////////////////////////////////////////////////////////////////
// Entry storage

typedef struct
{
    int32_t engine_id;
    char    shortcut_id[64];   // "hxt-<sanitised-key>", e.g. "hxt-ctrl-shift-p"
} PortalEntry;

static PortalEntry *s_entries     = NULL;
static size_t       s_entry_count = 0;
static size_t       s_entry_cap   = 0;
static pthread_mutex_t s_mutex    = PTHREAD_MUTEX_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////
// D-Bus / thread state

static DBusConnection *s_conn           = NULL;  // main-thread connection
static DBusConnection *s_thread_conn    = NULL;  // background-thread connection (separate)
static char            s_session_handle[256] = {};
static int             s_pipe_write_fd  = -1;
static pthread_t       s_thread;
static int             s_thread_running = 0;
static int             s_initialised    = 0;

////////////////////////////////////////////////////////////////////////////////
// Key string → portal trigger format
// "Ctrl+Shift+P"  →  "<Control><Shift>p"
// "Alt+F4"        →  "<Alt>F4"

static void _key_to_trigger(const char *p_key, char *out, size_t out_len)
{
    char buf[256];
    strncpy(buf, p_key, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tokens[16];
    int   count = 0;
    char *p     = buf;
    while (*p && count < 15)
    {
        char *start = p;
        while (*p && *p != '+') p++;
        if (*p == '+')
        {
            *p++ = '\0';
            tokens[count++] = start;
        }
        else
        {
            tokens[count++] = start;
            break;
        }
    }

    out[0]  = '\0';
    size_t pos = 0;

    // All tokens except the last are modifier names.
    for (int i = 0; i < count - 1; i++)
    {
        const char *m = tokens[i];
        const char *mapped = NULL;
        if      (strcasecmp(m, "ctrl")    == 0 || strcasecmp(m, "control") == 0)
            mapped = "<Control>";
        else if (strcasecmp(m, "shift")   == 0)
            mapped = "<Shift>";
        else if (strcasecmp(m, "alt")     == 0 || strcasecmp(m, "option")  == 0)
            mapped = "<Alt>";
        else if (strcasecmp(m, "win")     == 0 || strcasecmp(m, "cmd")     == 0 ||
                 strcasecmp(m, "command") == 0)
            mapped = "<Super>";
        if (mapped)
        {
            size_t len = strlen(mapped);
            if (pos + len < out_len - 1)
            {
                memcpy(out + pos, mapped, len);
                pos += len;
                out[pos] = '\0';
            }
        }
    }

    // Last token is the key name.
    if (count > 0)
    {
        const char *k = tokens[count - 1];
        const char *mapped = NULL;

        if      (strcasecmp(k, "space")     == 0) mapped = "space";
        else if (strcasecmp(k, "tab")       == 0) mapped = "Tab";
        else if (strcasecmp(k, "return")    == 0 ||
                 strcasecmp(k, "enter")     == 0) mapped = "Return";
        else if (strcasecmp(k, "escape")    == 0 ||
                 strcasecmp(k, "esc")       == 0) mapped = "Escape";
        else if (strcasecmp(k, "delete")    == 0) mapped = "Delete";
        else if (strcasecmp(k, "backspace") == 0) mapped = "BackSpace";
        else if (strcasecmp(k, "home")      == 0) mapped = "Home";
        else if (strcasecmp(k, "end")       == 0) mapped = "End";
        else if (strcasecmp(k, "pageup")    == 0) mapped = "Page_Up";
        else if (strcasecmp(k, "pagedown")  == 0) mapped = "Page_Down";
        else if (strcasecmp(k, "left")      == 0) mapped = "Left";
        else if (strcasecmp(k, "right")     == 0) mapped = "Right";
        else if (strcasecmp(k, "up")        == 0) mapped = "Up";
        else if (strcasecmp(k, "down")      == 0) mapped = "Down";
        else if (strcasecmp(k, "insert")    == 0) mapped = "Insert";

        if (mapped)
        {
            strncat(out + pos, mapped, out_len - pos - 1);
        }
        else if ((k[0] == 'f' || k[0] == 'F') && k[1] != '\0')
        {
            // F1–F12
            int fnum = atoi(k + 1);
            if (fnum >= 1 && fnum <= 12)
            {
                char fn[5];
                snprintf(fn, sizeof(fn), "F%d", fnum);
                strncat(out + pos, fn, out_len - pos - 1);
            }
        }
        else if (k[0] != '\0' && k[1] == '\0')
        {
            // Single printable character — lowercase.
            char lc[2] = { (char)tolower((unsigned char)k[0]), '\0' };
            strncat(out + pos, lc, out_len - pos - 1);
        }
        else
        {
            // Unknown — pass through as-is.
            strncat(out + pos, k, out_len - pos - 1);
        }
    }

    out[out_len - 1] = '\0';
}

////////////////////////////////////////////////////////////////////////////////
// D-Bus message builder helpers

// Append a string-type value (DBUS_TYPE_STRING or OBJECT_PATH) to an iter.
static dbus_bool_t _append_str(DBusMessageIter *it, int type, const char *s)
{
    return dbus_message_iter_append_basic(it, type, &s);
}

// Open an a{sv} (string → variant dict) container.
static dbus_bool_t _open_dict(DBusMessageIter *parent, DBusMessageIter *sub)
{
    return dbus_message_iter_open_container(parent, DBUS_TYPE_ARRAY,
                                            "{sv}", sub);
}

// Append one {sv} entry where the value is a string variant.
static dbus_bool_t _dict_append_str(DBusMessageIter *dict,
                                     const char *key, const char *value)
{
    DBusMessageIter entry, variant;
    if (!dbus_message_iter_open_container(dict, DBUS_TYPE_DICT_ENTRY,
                                          NULL, &entry))
        return 0;
    if (!_append_str(&entry, DBUS_TYPE_STRING, key))
        return 0;
    if (!dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT,
                                          "s", &variant))
        return 0;
    if (!_append_str(&variant, DBUS_TYPE_STRING, value))
        return 0;
    if (!dbus_message_iter_close_container(&entry, &variant))
        return 0;
    return dbus_message_iter_close_container(dict, &entry);
}


////////////////////////////////////////////////////////////////////////////////
// Wait for a org.freedesktop.portal.Request.Response signal on r_path.
// Blocks (polling the connection) up to timeout_ms.
// Returns 1 and fills r_results_iter with the results a{sv} iterator,
// or returns 0 on timeout / error.
// Caller must not unref the message before reading r_results_iter.

static int _wait_for_response(const char *r_path,
                               int         timeout_ms,
                               DBusMessage **r_msg_out,
                               DBusMessageIter *r_results_iter)
{
    // Add a match so messages for this specific request reach us.
    char match[512];
    snprintf(match, sizeof(match),
             "type='signal',"
             "interface='" REQUEST_IFACE "',"
             "member='Response',"
             "path='%s'",
             r_path);
    DBusError err;
    dbus_error_init(&err);
    dbus_bus_add_match(s_conn, match, &err);
    if (dbus_error_is_set(&err))
    {
        dbus_error_free(&err);
        return 0;
    }

    int waited = 0;
    const int step = 50;   // poll every 50 ms

    while (waited < timeout_ms)
    {
        dbus_connection_read_write(s_conn, step);
        waited += step;

        DBusMessage *msg;
        while ((msg = dbus_connection_pop_message(s_conn)) != NULL)
        {
            if (dbus_message_is_signal(msg, REQUEST_IFACE, "Response") &&
                strcmp(dbus_message_get_path(msg), r_path) == 0)
            {
                // Parse: u response, a{sv} results
                DBusMessageIter it;
                if (!dbus_message_iter_init(msg, &it))
                {
                    dbus_message_unref(msg);
                    return 0;
                }
                // Read the response code (uint32).
                if (dbus_message_iter_get_arg_type(&it) != DBUS_TYPE_UINT32)
                {
                    dbus_message_unref(msg);
                    return 0;
                }
                dbus_uint32_t resp_code = 0;
                dbus_message_iter_get_basic(&it, &resp_code);
                if (resp_code != 0)
                {
                    dbus_message_unref(msg);
                    return 0;   // user cancelled or error
                }
                if (!dbus_message_iter_next(&it))
                {
                    dbus_message_unref(msg);
                    return 0;
                }
                // Caller reads the a{sv} via r_results_iter.
                dbus_message_iter_recurse(&it, r_results_iter);
                *r_msg_out = msg;   // caller owns; must unref after use
                return 1;
            }
            dbus_message_unref(msg);
        }
    }
    return 0;  // timed out
}

// Extract the value of key "session_handle" (object path) from an a{sv} iter.
static int _extract_session_handle(DBusMessageIter *dict_iter, char *out, size_t out_len)
{
    while (dbus_message_iter_get_arg_type(dict_iter) == DBUS_TYPE_DICT_ENTRY)
    {
        DBusMessageIter entry, value;
        dbus_message_iter_recurse(dict_iter, &entry);

        const char *k = NULL;
        dbus_message_iter_get_basic(&entry, &k);
        dbus_message_iter_next(&entry);

        // value is a variant — recurse into it to get the actual value type
        dbus_message_iter_recurse(&entry, &value);
        int vtype = dbus_message_iter_get_arg_type(&value);

        if (k && strcmp(k, "session_handle") == 0 &&
            (vtype == DBUS_TYPE_OBJECT_PATH || vtype == DBUS_TYPE_STRING))
        {
            const char *v = NULL;
            dbus_message_iter_get_basic(&value, &v);
            if (v)
            {
                strncpy(out, v, out_len - 1);
                out[out_len - 1] = '\0';
                return 1;
            }
        }
        dbus_message_iter_next(dict_iter);
    }
    return 0;
}

////////////////////////////////////////////////////////////////////////////////
// Background thread — watches for Activated signals

static void _dispatch_activated(DBusMessage *msg)
{
    // Signature: o session_handle, s shortcut_id, t timestamp, a{sv} options
    DBusMessageIter it;
    if (!dbus_message_iter_init(msg, &it))
        return;

    // Verify session_handle belongs to our session (o or s type).
    int sh_type = dbus_message_iter_get_arg_type(&it);
    if (sh_type != DBUS_TYPE_OBJECT_PATH && sh_type != DBUS_TYPE_STRING)
        return;
    const char *sig_session = NULL;
    dbus_message_iter_get_basic(&it, &sig_session);
    if (!sig_session || strcmp(sig_session, s_session_handle) != 0)
        return;
    dbus_message_iter_next(&it);

    // shortcut_id (s)
    if (dbus_message_iter_get_arg_type(&it) != DBUS_TYPE_STRING)
        return;
    const char *shortcut_id = NULL;
    dbus_message_iter_get_basic(&it, &shortcut_id);
    if (!shortcut_id)
        return;

    pthread_mutex_lock(&s_mutex);
    for (size_t i = 0; i < s_entry_count; i++)
    {
        if (strcmp(s_entries[i].shortcut_id, shortcut_id) == 0)
        {
            int32_t t_id = s_entries[i].engine_id;
            pthread_mutex_unlock(&s_mutex);
            { ssize_t _wr = write(s_pipe_write_fd, &t_id, sizeof(t_id)); (void)_wr; }
            return;
        }
    }
    pthread_mutex_unlock(&s_mutex);
}

static void *_portal_thread(void *unused)
{
    (void)unused;

    // Open a dedicated connection so we never contend with the main thread.
    DBusError err;
    dbus_error_init(&err);
    s_thread_conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!s_thread_conn || dbus_error_is_set(&err))
    {
        dbus_error_free(&err);
        return NULL;
    }

    // Subscribe to Activated signals from the portal.
    //
    // The GlobalShortcuts Activated signal is emitted by the portal on its
    // own object path (/org/freedesktop/portal/desktop), not on the session
    // object path.  The session handle appears in the signal body (first arg).
    // We omit sender= because some GNOME versions route the signal through the
    // backend's own bus name rather than org.freedesktop.portal.Desktop.
    // Session-handle verification happens inside _dispatch_activated.
    char match[512];
    snprintf(match, sizeof(match),
             "type='signal',"
             "interface='" PORTAL_IFACE "',"
             "member='Activated'");
    dbus_error_init(&err);
    dbus_bus_add_match(s_thread_conn, match, &err);
    if (dbus_error_is_set(&err))
        dbus_error_free(&err);
    dbus_connection_flush(s_thread_conn);

    while (s_thread_running)
    {
        // Block up to 200 ms waiting for Activated signals on our own connection.
        // s_conn is owned exclusively by the main thread; we do not touch it here.
        dbus_connection_read_write(s_thread_conn, 200);

        DBusMessage *msg;
        while ((msg = dbus_connection_pop_message(s_thread_conn)) != NULL)
        {
            if (dbus_message_is_signal(msg, PORTAL_IFACE, "Activated"))
                _dispatch_activated(msg);
            dbus_message_unref(msg);
        }
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// Public API

int lnx_hotkey_portal_available(void)
{
    if (!initialise_weak_link_dbus())
        return 0;

    DBusError err;
    dbus_error_init(&err);
    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!conn || dbus_error_is_set(&err))
    {
        dbus_error_free(&err);
        return 0;
    }
    // Checking for the service name would require dbus_bus_name_has_owner which
    // we haven't stubbed; just return 1 if we connected — init will fail cleanly
    // if the portal isn't present.
    return 1;
}

int lnx_hotkey_portal_init(int write_fd)
{
    if (s_initialised)
        return 1;

    // Register our .desktop file so the portal can identify this process.
    // Must happen before we open the D-Bus connection.
    _ensure_desktop_file();

    if (!initialise_weak_link_dbus())
        return 0;

    DBusError err;
    dbus_error_init(&err);
    s_conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!s_conn || dbus_error_is_set(&err))
    {
        dbus_error_free(&err);
        return 0;
    }
    s_pipe_write_fd = write_fd;

    // ---- CreateSession -------------------------------------------------------
    char handle_token[32], session_token[32];
    snprintf(handle_token,  sizeof(handle_token),  "hxt%d_cs", (int)getpid());
    snprintf(session_token, sizeof(session_token), "hxt%d_s",  (int)getpid());

    DBusMessage *call = dbus_message_new_method_call(
        PORTAL_DEST, PORTAL_PATH, PORTAL_IFACE, "CreateSession");
    if (!call)
    {
        fprintf(stderr, "lnx-hotkey-portal: dbus_message_new_method_call failed (OOM)\n");
        return 0;
    }

    DBusMessageIter args, opts;
    dbus_message_iter_init_append(call, &args);
    _open_dict(&args, &opts);
    _dict_append_str(&opts, "handle_token",          handle_token);
    _dict_append_str(&opts, "session_handle_token",  session_token);
    dbus_message_iter_close_container(&args, &opts);

    dbus_error_init(&err);
    DBusMessage *reply = dbus_connection_send_with_reply_and_block(
        s_conn, call, 5000, &err);
    dbus_message_unref(call);

    if (!reply || dbus_error_is_set(&err))
    {
        dbus_error_free(&err);
        return 0;
    }

    // The reply contains the request object path (o).
    DBusMessageIter ri;
    if (!dbus_message_iter_init(reply, &ri) ||
        dbus_message_iter_get_arg_type(&ri) != DBUS_TYPE_OBJECT_PATH)
    {
        dbus_message_unref(reply);
        return 0;
    }
    const char *req_path = NULL;
    dbus_message_iter_get_basic(&ri, &req_path);
    char req_path_buf[256];
    strncpy(req_path_buf, req_path ? req_path : "", sizeof(req_path_buf) - 1);
    dbus_message_unref(reply);

    // ---- Wait for Request.Response -------------------------------------------
    DBusMessage    *resp_msg = NULL;
    DBusMessageIter results_iter;
    if (!_wait_for_response(req_path_buf, 10000, &resp_msg, &results_iter))
        return 0;

    if (!_extract_session_handle(&results_iter, s_session_handle,
                                  sizeof(s_session_handle)))
    {
        dbus_message_unref(resp_msg);
        return 0;
    }
    dbus_message_unref(resp_msg);

    // ---- Start event thread (opens its own D-Bus connection) -----------------
    s_thread_running = 1;
    if (pthread_create(&s_thread, NULL, _portal_thread, NULL) != 0)
    {
        s_thread_running = 0;
        return 0;
    }
    pthread_detach(s_thread);

    s_initialised = 1;
    return 1;
}

int lnx_hotkey_portal_register(int32_t     engine_id,
                                const char *p_key,
                                char       *p_error,
                                size_t      p_error_len)
{
    if (!s_initialised)
    {
        strncpy(p_error, "portal not initialised", p_error_len);
        p_error[p_error_len - 1] = '\0';
        return 0;
    }

    // Build shortcut ID from key string so it is stable across re-registrations.
    // "Ctrl+Shift+P" → "hxt-ctrl-shift-p".  Using a stable ID means GNOME
    // recognises the shortcut on subsequent BindShortcuts calls and does not
    // show the assignment dialog again.
    char shortcut_id[64];
    {
        char tmp[64] = "hxt-";
        size_t out = 4;
        for (size_t i = 0; p_key[i] && out < sizeof(tmp) - 1; i++)
        {
            char c = (char)tolower((unsigned char)p_key[i]);
            if (isalnum((unsigned char)c))
                tmp[out++] = c;
            else if (out > 4 && tmp[out - 1] != '-')
                tmp[out++] = '-';
        }
        while (out > 4 && tmp[out - 1] == '-')
            out--;
        tmp[out] = '\0';
        strncpy(shortcut_id, tmp, sizeof(shortcut_id) - 1);
        shortcut_id[sizeof(shortcut_id) - 1] = '\0';
    }

    char trigger[128];
    _key_to_trigger(p_key, trigger, sizeof(trigger));

    // Generate a handle token — must match [A-Za-z0-9_] only (used to build
    // a D-Bus object path).  Use engine_id which is always a plain integer.
    char handle_token[48];
    snprintf(handle_token, sizeof(handle_token), "hxt%d_b%d",
             (int)getpid(), (int)engine_id);

    // ---- BindShortcuts (blocking: lets us see whether the portal accepts it) --
    // Signature: o session_handle, a(sa{sv}) shortcuts, s parent_window, a{sv} options
    DBusMessage *bind_msg = dbus_message_new_method_call(
        PORTAL_DEST, PORTAL_PATH, PORTAL_IFACE, "BindShortcuts");
    if (!bind_msg)
    {
        strncpy(p_error, "out of memory", p_error_len);
        p_error[p_error_len - 1] = '\0';
        return 0;
    }

    DBusMessageIter args;
    dbus_message_iter_init_append(bind_msg, &args);

    // session_handle (o)
    _append_str(&args, DBUS_TYPE_OBJECT_PATH, s_session_handle);

    // shortcuts: a(sa{sv})
    DBusMessageIter shortcuts_arr, shortcut_struct, shortcut_opts;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "(sa{sv})",
                                     &shortcuts_arr);
    dbus_message_iter_open_container(&shortcuts_arr, DBUS_TYPE_STRUCT, NULL,
                                     &shortcut_struct);
    _append_str(&shortcut_struct, DBUS_TYPE_STRING, shortcut_id);
    _open_dict(&shortcut_struct, &shortcut_opts);
    _dict_append_str(&shortcut_opts, "description",         p_key);
    // preferred_trigger: GNOME's impl expects 's' (single string) here even
    // though the portal spec says 'as'.  We use the GTK accelerator format.
    if (trigger[0])
        _dict_append_str(&shortcut_opts, "preferred_trigger", trigger);
    dbus_message_iter_close_container(&shortcut_struct, &shortcut_opts);
    dbus_message_iter_close_container(&shortcuts_arr, &shortcut_struct);
    dbus_message_iter_close_container(&args, &shortcuts_arr);

    // parent_window (s) — empty
    _append_str(&args, DBUS_TYPE_STRING, "");

    // options a{sv} — just handle_token
    DBusMessageIter bind_opts;
    _open_dict(&args, &bind_opts);
    _dict_append_str(&bind_opts, "handle_token", handle_token);
    dbus_message_iter_close_container(&args, &bind_opts);

    // Send BindShortcuts fire-and-forget.
    //
    // GNOME's portal backend does not return the method reply immediately —
    // it blocks until the user interacts with a shortcut-assignment dialog.
    // Waiting for the reply here would stall the HyperXTalk main thread for
    // an indeterminate time.  Instead we send without waiting; the portal
    // will process the call asynchronously and fire Activated signals on the
    // session once the shortcut is active.
    //
    // On first run GNOME may show a keyboard-shortcut assignment dialog.
    // If no hotkeys fire, look for that dialog (it can appear behind other
    // windows) and confirm the shortcut there.
    // Fire-and-forget: send without waiting for a reply.  Do not call
    // dbus_connection_flush here — the background thread's read_write loop
    // will flush naturally, avoiding contention on the shared connection.
    dbus_bool_t sent = dbus_connection_send(s_conn, bind_msg, NULL);
    dbus_message_unref(bind_msg);
    (void)sent;

    // ---- Store entry ---------------------------------------------------------
    pthread_mutex_lock(&s_mutex);

    if (s_entry_count >= s_entry_cap)
    {
        size_t new_cap = s_entry_cap == 0 ? 8 : s_entry_cap * 2;
        PortalEntry *new_buf = (PortalEntry *)realloc(
            s_entries, new_cap * sizeof(PortalEntry));
        if (!new_buf)
        {
            pthread_mutex_unlock(&s_mutex);
            strncpy(p_error, "out of memory", p_error_len);
            p_error[p_error_len - 1] = '\0';
            return 0;
        }
        s_entries   = new_buf;
        s_entry_cap = new_cap;
    }

    s_entries[s_entry_count].engine_id = engine_id;
    strncpy(s_entries[s_entry_count].shortcut_id, shortcut_id,
            sizeof(s_entries[0].shortcut_id) - 1);
    s_entries[s_entry_count].shortcut_id[sizeof(s_entries[0].shortcut_id) - 1] = '\0';
    s_entry_count++;

    pthread_mutex_unlock(&s_mutex);
    return 1;
}

void lnx_hotkey_portal_unregister(int32_t engine_id)
{
    pthread_mutex_lock(&s_mutex);
    for (size_t i = 0; i < s_entry_count; i++)
    {
        if (s_entries[i].engine_id == engine_id)
        {
            for (size_t j = i + 1; j < s_entry_count; j++)
                s_entries[j - 1] = s_entries[j];
            s_entry_count--;
            break;
        }
    }
    pthread_mutex_unlock(&s_mutex);
    // Note: the portal has no per-shortcut unbind; the shortcut stays registered
    // in the DE's settings but will simply not fire our handler.
}

void lnx_hotkey_portal_unregister_all(void)
{
    pthread_mutex_lock(&s_mutex);
    free(s_entries);
    s_entries     = NULL;
    s_entry_count = 0;
    s_entry_cap   = 0;
    pthread_mutex_unlock(&s_mutex);
}
