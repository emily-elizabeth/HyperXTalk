/* Copyright (C) 2003-2015 LiveCode Ltd.

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

//
// Linux notification backend.
//
// Implements the platform notification entry points via the D-Bus
// org.freedesktop.Notifications interface (the cross-desktop standard).
//
// Spec: https://specifications.freedesktop.org/notification-spec/
//
// Notification click callbacks are not delivered here because the
// freedesktop spec does not guarantee that ActionInvoked signals are
// emitted by all notification daemons (GNOME, KDE, XFCE all behave
// differently).  For consistency with other platforms we fire
// notificationClicked only if the daemon actually emits the signal.
//
// Requires: libdbus-1.  Linked via pkg-config: dbus-1.
//

#include "prefix.h"
#include "mcstring.h"
#include "notification.h"

#include <dbus/dbus.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstdint>

////////////////////////////////////////////////////////////////////////////////
// D-Bus well-known names

static const char * const kFDNBus    = "org.freedesktop.Notifications";
static const char * const kFDNPath   = "/org/freedesktop/Notifications";
static const char * const kFDNIface  = "org.freedesktop.Notifications";

////////////////////////////////////////////////////////////////////////////////
// Helpers: MCStringRef → std::string (UTF-8)

static std::string _mcstr_to_utf8(MCStringRef p_str)
{
    if (p_str == nil || MCStringIsEmpty(p_str))
        return "";
    char *t_buf = nil;
    if (!MCStringConvertToUTF8String(p_str, t_buf))
        return "";
    std::string t_result(t_buf);
    MCMemoryDeallocate(t_buf);
    return t_result;
}

////////////////////////////////////////////////////////////////////////////////
// Shared session bus connection (lazily initialised, never closed)

static DBusConnection *s_dbus = nullptr;
static std::mutex      s_dbus_mutex;

static DBusConnection *_get_connection()
{
    std::lock_guard<std::mutex> guard(s_dbus_mutex);
    if (s_dbus)
        return s_dbus;

    DBusError err;
    dbus_error_init(&err);
    s_dbus = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err))
        dbus_error_free(&err);
    return s_dbus;
}

////////////////////////////////////////////////////////////////////////////////
// Notification ID map: tag → uint32 D-Bus notification ID
// Used to replace / cancel existing notifications.

static std::mutex                              s_id_mutex;
static std::unordered_map<std::string, uint32_t> s_id_map;

static uint32_t _lookup_id(const std::string& p_tag)
{
    std::lock_guard<std::mutex> guard(s_id_mutex);
    auto it = s_id_map.find(p_tag);
    return (it != s_id_map.end()) ? it->second : 0;
}

static void _store_id(const std::string& p_tag, uint32_t p_id)
{
    std::lock_guard<std::mutex> guard(s_id_mutex);
    s_id_map[p_tag] = p_id;
}

static void _remove_id(const std::string& p_tag)
{
    std::lock_guard<std::mutex> guard(s_id_mutex);
    s_id_map.erase(p_tag);
}

static std::vector<uint32_t> _all_ids()
{
    std::lock_guard<std::mutex> guard(s_id_mutex);
    std::vector<uint32_t> t_ids;
    t_ids.reserve(s_id_map.size());
    for (auto& kv : s_id_map)
        t_ids.push_back(kv.second);
    s_id_map.clear();
    return t_ids;
}

////////////////////////////////////////////////////////////////////////////////
// Call org.freedesktop.Notifications.Notify
//
//   Notify(app_name, replaces_id, app_icon, summary, body,
//          actions, hints, expire_timeout) → id
//
// Returns the notification ID on success, 0 on failure.

static uint32_t _fdn_notify(const std::string& p_title,
                             const std::string& p_body,
                             uint32_t           p_replaces_id)
{
    DBusConnection *t_conn = _get_connection();
    if (!t_conn)
        return 0;

    DBusMessage *t_msg = dbus_message_new_method_call(
        kFDNBus, kFDNPath, kFDNIface, "Notify");
    if (!t_msg)
        return 0;

    // Detect the calling application name from /proc/self/comm.
    char t_comm[256] = "HyperXTalk";
    FILE *f = fopen("/proc/self/comm", "r");
    if (f) { fscanf(f, "%255s", t_comm); fclose(f); }

    const char *t_app_name    = t_comm;
    dbus_uint32_t t_replaces  = (dbus_uint32_t)p_replaces_id;
    const char *t_icon        = "";   // empty = let the daemon choose
    const char *t_summary     = p_title.c_str();
    const char *t_body        = p_body.c_str();
    dbus_int32_t t_timeout    = -1;   // daemon default

    DBusMessageIter t_args;
    dbus_message_iter_init_append(t_msg, &t_args);

    dbus_message_iter_append_basic(&t_args, DBUS_TYPE_STRING, &t_app_name);
    dbus_message_iter_append_basic(&t_args, DBUS_TYPE_UINT32, &t_replaces);
    dbus_message_iter_append_basic(&t_args, DBUS_TYPE_STRING, &t_icon);
    dbus_message_iter_append_basic(&t_args, DBUS_TYPE_STRING, &t_summary);
    dbus_message_iter_append_basic(&t_args, DBUS_TYPE_STRING, &t_body);

    // actions: empty array as/
    DBusMessageIter t_actions_iter;
    dbus_message_iter_open_container(&t_args, DBUS_TYPE_ARRAY, "s", &t_actions_iter);
    dbus_message_iter_close_container(&t_args, &t_actions_iter);

    // hints: empty dict a{sv}
    DBusMessageIter t_hints_iter;
    dbus_message_iter_open_container(&t_args, DBUS_TYPE_ARRAY, "{sv}", &t_hints_iter);
    dbus_message_iter_close_container(&t_args, &t_hints_iter);

    dbus_message_iter_append_basic(&t_args, DBUS_TYPE_INT32, &t_timeout);

    // Send and wait for the reply (brief timeout — 500 ms).
    DBusError t_err;
    dbus_error_init(&t_err);
    DBusMessage *t_reply = dbus_connection_send_with_reply_and_block(
        t_conn, t_msg, 500, &t_err);
    dbus_message_unref(t_msg);

    if (dbus_error_is_set(&t_err))
    {
        dbus_error_free(&t_err);
        return 0;
    }
    if (!t_reply)
        return 0;

    dbus_uint32_t t_id = 0;
    DBusMessageIter t_reply_iter;
    if (dbus_message_iter_init(t_reply, &t_reply_iter) &&
        dbus_message_iter_get_arg_type(&t_reply_iter) == DBUS_TYPE_UINT32)
    {
        dbus_message_iter_get_basic(&t_reply_iter, &t_id);
    }

    dbus_message_unref(t_reply);
    return (uint32_t)t_id;
}

////////////////////////////////////////////////////////////////////////////////
// Call org.freedesktop.Notifications.CloseNotification(id)

static void _fdn_close(uint32_t p_id)
{
    if (p_id == 0)
        return;

    DBusConnection *t_conn = _get_connection();
    if (!t_conn)
        return;

    DBusMessage *t_msg = dbus_message_new_method_call(
        kFDNBus, kFDNPath, kFDNIface, "CloseNotification");
    if (!t_msg)
        return;

    dbus_uint32_t t_id = (dbus_uint32_t)p_id;
    dbus_message_append_args(t_msg,
                             DBUS_TYPE_UINT32, &t_id,
                             DBUS_TYPE_INVALID);

    // Fire-and-forget — we don't need to wait for a reply.
    dbus_uint32_t t_serial;
    dbus_connection_send(t_conn, t_msg, &t_serial);
    dbus_connection_flush(t_conn);
    dbus_message_unref(t_msg);
}

////////////////////////////////////////////////////////////////////////////////
// Tag: if p_tag is empty we generate one from the D-Bus id after the fact.
// We keep a synthetic tag->id map so all cancel operations work uniformly.

static std::string _synth_tag(uint32_t p_id)
{
    return "_id_" + std::to_string(p_id);
}

////////////////////////////////////////////////////////////////////////////////
// Platform entry points

void MCPlatformRequestNotificationPermission()
{
    // Linux (freedesktop) has no permission concept — always granted.
    MCNotificationDispatchPermissionGranted();
}

void MCPlatformShowNotification(MCStringRef p_title, MCStringRef p_body, MCStringRef p_tag)
{
    std::string t_title = _mcstr_to_utf8(p_title);
    std::string t_body  = _mcstr_to_utf8(p_body);
    std::string t_tag;
    bool t_has_tag = (p_tag != nil && !MCStringIsEmpty(p_tag));
    if (t_has_tag)
        t_tag = _mcstr_to_utf8(p_tag);

    // If the same tag was used before, replace that notification (replaces_id).
    uint32_t t_replaces = t_has_tag ? _lookup_id(t_tag) : 0;

    uint32_t t_id = _fdn_notify(t_title, t_body, t_replaces);
    if (t_id == 0)
        return;  // daemon unavailable — silently ignore

    if (t_has_tag)
        _store_id(t_tag, t_id);
    else
        _store_id(_synth_tag(t_id), t_id);
}

void MCPlatformCancelNotification(MCStringRef p_tag)
{
    std::string t_tag = _mcstr_to_utf8(p_tag);
    uint32_t t_id = _lookup_id(t_tag);
    if (t_id == 0)
        return;
    _remove_id(t_tag);
    _fdn_close(t_id);
}

void MCPlatformCancelAllNotifications()
{
    std::vector<uint32_t> t_ids = _all_ids();
    for (uint32_t id : t_ids)
        _fdn_close(id);
}

void MCPlatformGetNotificationPermission(MCStringRef& r_permission)
{
    // Linux has no per-app permission system for desktop notifications.
    /* UNCHECKED */ MCStringCreateWithCString("granted", r_permission);
}
