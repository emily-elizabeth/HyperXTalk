/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

// lnx-credentials.cpp
//
// Linux implementation of MCCredentialStore / MCCredentialRetrieve /
// MCCredentialDelete using the D-Bus Secret Service API
// (org.freedesktop.secrets, libdbus-1).
//
// The Secret Service protocol is implemented directly over D-Bus without
// libsecret to avoid adding a runtime library dependency.  A "plain"
// (unencrypted) transport session is used; the D-Bus socket itself is a
// local AF_UNIX connection and is not exposed to the network.
//
// If no Secret Service daemon (GNOME Keyring or KWallet) is running on the
// session bus, all three functions return false and the exec layer sets
// the result to an appropriate error string.

#include "prefix.h"

#include "exec-credentials.h"

#include <dbus/dbus.h>
#include <string.h>
#include <string>

////////////////////////////////////////////////////////////////////////////////
// D-Bus helpers

#define SS_SERVICE    "org.freedesktop.secrets"
#define SS_PATH       "/org/freedesktop/secrets"
#define SS_IFACE      "org.freedesktop.Secret.Service"
#define SS_COLL_IFACE "org.freedesktop.Secret.Collection"
#define SS_ITEM_IFACE "org.freedesktop.Secret.Item"
#define SS_SESSION    "org.freedesktop.Secret.Session"
#define DEFAULT_COLL  "/org/freedesktop/secrets/aliases/default"

// RAII wrapper for DBusMessage.
struct DBusMsgGuard
{
    DBusMessage *msg;
    DBusMsgGuard(DBusMessage *m = nullptr) : msg(m) {}
    ~DBusMsgGuard() { if (msg) dbus_message_unref(msg); }
    DBusMessage *operator->() { return msg; }
    operator bool() const { return msg != nullptr; }
};

// RAII wrapper for DBusConnection.
struct DBusConnGuard
{
    DBusConnection *conn;
    DBusConnGuard(DBusConnection *c = nullptr) : conn(c) {}
    ~DBusConnGuard() { if (conn) dbus_connection_unref(conn); }
    operator DBusConnection*() { return conn; }
    operator bool() const { return conn != nullptr; }
};

// Convert MCStringRef to std::string (UTF-8).
static std::string s_utf8(MCStringRef p_str)
{
    MCAutoPointer<char> t_buf;
    uindex_t t_len;
    if (!MCStringConvertToUTF8(p_str, &t_buf, t_len))
        return {};
    return std::string(*t_buf, t_len);
}

// Send a method call and return the reply, or nullptr on error/timeout.
static DBusMessage *s_call(DBusConnection *p_conn,
                           const char *p_dest, const char *p_path,
                           const char *p_iface, const char *p_method,
                           DBusMessage *p_msg)
{
    DBusError t_err;
    dbus_error_init(&t_err);
    DBusMessage *t_reply = dbus_connection_send_with_reply_and_block(
        p_conn, p_msg, 5000, &t_err);
    if (dbus_error_is_set(&t_err))
        dbus_error_free(&t_err);
    return t_reply;
}

////////////////////////////////////////////////////////////////////////////////
// Session management
//
// We open a "plain" session (no DH encryption) per-call.  Opening a session
// is cheap and avoids needing to cache state across script invocations.

// Opens a plain session and returns its object path, or "" on failure.
static std::string s_open_session(DBusConnection *p_conn)
{
    DBusMsgGuard t_msg(dbus_message_new_method_call(
        SS_SERVICE, SS_PATH, SS_IFACE, "OpenSession"));
    if (!t_msg)
        return {};

    const char *t_algorithm = "plain";
    DBusMessageIter t_args, t_variant;
    dbus_message_iter_init_append(t_msg.msg, &t_args);
    dbus_message_iter_append_basic(&t_args, DBUS_TYPE_STRING, &t_algorithm);

    // input: variant containing empty string
    dbus_message_iter_open_container(&t_args, DBUS_TYPE_VARIANT, "s", &t_variant);
    const char *t_empty = "";
    dbus_message_iter_append_basic(&t_variant, DBUS_TYPE_STRING, &t_empty);
    dbus_message_iter_close_container(&t_args, &t_variant);

    DBusMsgGuard t_reply(s_call(p_conn, nullptr, nullptr, nullptr, nullptr,
                                t_msg.msg));
    if (!t_reply)
        return {};

    // Reply: (variant output, object_path session_path)
    DBusMessageIter t_iter;
    if (!dbus_message_iter_init(t_reply.msg, &t_iter))
        return {};

    // Skip the output variant.
    dbus_message_iter_next(&t_iter);

    const char *t_path = nullptr;
    if (dbus_message_iter_get_arg_type(&t_iter) != DBUS_TYPE_OBJECT_PATH)
        return {};
    dbus_message_iter_get_basic(&t_iter, &t_path);
    return t_path ? std::string(t_path) : std::string();
}

////////////////////////////////////////////////////////////////////////////////
// Secret struct helper
//
// org.freedesktop.Secret.Secret = (o session, ay parameters, ay value,
//                                   s content_type)

static void s_append_secret(DBusMessageIter *p_iter,
                             const std::string &p_session_path,
                             const std::string &p_secret_utf8)
{
    DBusMessageIter t_struct;
    dbus_message_iter_open_container(p_iter, DBUS_TYPE_STRUCT, nullptr,
                                     &t_struct);

    // session path
    const char *t_spath = p_session_path.c_str();
    dbus_message_iter_append_basic(&t_struct, DBUS_TYPE_OBJECT_PATH, &t_spath);

    // parameters (empty byte array)
    DBusMessageIter t_params;
    dbus_message_iter_open_container(&t_struct, DBUS_TYPE_ARRAY, "y", &t_params);
    dbus_message_iter_close_container(&t_struct, &t_params);

    // value bytes
    DBusMessageIter t_value;
    dbus_message_iter_open_container(&t_struct, DBUS_TYPE_ARRAY, "y", &t_value);
    const uint8_t *t_bytes =
        reinterpret_cast<const uint8_t *>(p_secret_utf8.data());
    int t_len = (int)p_secret_utf8.size();
    dbus_message_iter_append_fixed_array(&t_value, DBUS_TYPE_BYTE,
                                         &t_bytes, t_len);
    dbus_message_iter_close_container(&t_struct, &t_value);

    // content_type
    const char *t_ct = "text/plain; charset=utf8";
    dbus_message_iter_append_basic(&t_struct, DBUS_TYPE_STRING, &t_ct);

    dbus_message_iter_close_container(p_iter, &t_struct);
}

////////////////////////////////////////////////////////////////////////////////
// Attribute helpers
//
// Items are identified by attributes: {"service": service, "account": account}

static void s_append_attributes(DBusMessageIter *p_iter,
                                 const std::string &p_service,
                                 const std::string &p_account)
{
    DBusMessageIter t_array, t_dict;
    dbus_message_iter_open_container(p_iter, DBUS_TYPE_ARRAY, "{ss}", &t_array);

    // "service" key
    dbus_message_iter_open_container(&t_array, DBUS_TYPE_DICT_ENTRY,
                                     nullptr, &t_dict);
    const char *t_k1 = "service";
    dbus_message_iter_append_basic(&t_dict, DBUS_TYPE_STRING, &t_k1);
    const char *t_v1 = p_service.c_str();
    dbus_message_iter_append_basic(&t_dict, DBUS_TYPE_STRING, &t_v1);
    dbus_message_iter_close_container(&t_array, &t_dict);

    // "account" key
    dbus_message_iter_open_container(&t_array, DBUS_TYPE_DICT_ENTRY,
                                     nullptr, &t_dict);
    const char *t_k2 = "account";
    dbus_message_iter_append_basic(&t_dict, DBUS_TYPE_STRING, &t_k2);
    const char *t_v2 = p_account.c_str();
    dbus_message_iter_append_basic(&t_dict, DBUS_TYPE_STRING, &t_v2);
    dbus_message_iter_close_container(&t_array, &t_dict);

    dbus_message_iter_close_container(p_iter, &t_array);
}

// Search the default collection for items matching service + account.
// Returns the first matching item's object path, or "" if none found.
static std::string s_find_item(DBusConnection *p_conn,
                                const std::string &p_service,
                                const std::string &p_account)
{
    DBusMsgGuard t_msg(dbus_message_new_method_call(
        SS_SERVICE, DEFAULT_COLL, SS_COLL_IFACE, "SearchItems"));
    if (!t_msg)
        return {};

    DBusMessageIter t_args;
    dbus_message_iter_init_append(t_msg.msg, &t_args);
    s_append_attributes(&t_args, p_service, p_account);

    DBusMsgGuard t_reply(s_call(p_conn, nullptr, nullptr, nullptr, nullptr,
                                t_msg.msg));
    if (!t_reply)
        return {};

    // Reply is an array of object paths.
    DBusMessageIter t_iter, t_arr;
    if (!dbus_message_iter_init(t_reply.msg, &t_iter))
        return {};
    if (dbus_message_iter_get_arg_type(&t_iter) != DBUS_TYPE_ARRAY)
        return {};

    dbus_message_iter_recurse(&t_iter, &t_arr);
    if (dbus_message_iter_get_arg_type(&t_arr) != DBUS_TYPE_OBJECT_PATH)
        return {};

    const char *t_path = nullptr;
    dbus_message_iter_get_basic(&t_arr, &t_path);
    return t_path ? std::string(t_path) : std::string();
}

////////////////////////////////////////////////////////////////////////////////

bool MCCredentialStore(MCStringRef p_service,
                       MCStringRef p_account,
                       MCStringRef p_secret)
{
    DBusError t_err;
    dbus_error_init(&t_err);
    DBusConnGuard t_conn(dbus_bus_get(DBUS_BUS_SESSION, &t_err));
    if (dbus_error_is_set(&t_err))
        dbus_error_free(&t_err);
    if (!t_conn)
        return false;

    // Check service is available.
    if (!dbus_bus_name_has_owner(t_conn, SS_SERVICE, &t_err))
    {
        if (dbus_error_is_set(&t_err))
            dbus_error_free(&t_err);
        return false;
    }

    std::string t_session = s_open_session(t_conn);
    if (t_session.empty())
        return false;

    std::string t_svc    = s_utf8(p_service);
    std::string t_acc    = s_utf8(p_account);
    std::string t_secret = s_utf8(p_secret);

    DBusMsgGuard t_msg(dbus_message_new_method_call(
        SS_SERVICE, DEFAULT_COLL, SS_COLL_IFACE, "CreateItem"));
    if (!t_msg)
        return false;

    DBusMessageIter t_args, t_props, t_dict;
    dbus_message_iter_init_append(t_msg.msg, &t_args);

    // properties dict: label + attributes
    dbus_message_iter_open_container(&t_args, DBUS_TYPE_ARRAY, "{sv}", &t_props);

    // org.freedesktop.Secret.Item.Label
    dbus_message_iter_open_container(&t_props, DBUS_TYPE_DICT_ENTRY,
                                     nullptr, &t_dict);
    const char *t_label_key = "org.freedesktop.Secret.Item.Label";
    dbus_message_iter_append_basic(&t_dict, DBUS_TYPE_STRING, &t_label_key);
    DBusMessageIter t_label_var;
    dbus_message_iter_open_container(&t_dict, DBUS_TYPE_VARIANT, "s",
                                     &t_label_var);
    const char *t_label_val = t_svc.c_str();
    dbus_message_iter_append_basic(&t_label_var, DBUS_TYPE_STRING, &t_label_val);
    dbus_message_iter_close_container(&t_dict, &t_label_var);
    dbus_message_iter_close_container(&t_props, &t_dict);

    // org.freedesktop.Secret.Item.Attributes
    dbus_message_iter_open_container(&t_props, DBUS_TYPE_DICT_ENTRY,
                                     nullptr, &t_dict);
    const char *t_attr_key = "org.freedesktop.Secret.Item.Attributes";
    dbus_message_iter_append_basic(&t_dict, DBUS_TYPE_STRING, &t_attr_key);
    DBusMessageIter t_attr_var;
    dbus_message_iter_open_container(&t_dict, DBUS_TYPE_VARIANT, "a{ss}",
                                     &t_attr_var);
    s_append_attributes(&t_attr_var, t_svc, t_acc);
    dbus_message_iter_close_container(&t_dict, &t_attr_var);
    dbus_message_iter_close_container(&t_props, &t_dict);

    dbus_message_iter_close_container(&t_args, &t_props);

    // secret struct
    s_append_secret(&t_args, t_session, t_secret);

    // replace = true
    dbus_bool_t t_replace = TRUE;
    dbus_message_iter_append_basic(&t_args, DBUS_TYPE_BOOLEAN, &t_replace);

    DBusMsgGuard t_reply(s_call(t_conn, nullptr, nullptr, nullptr, nullptr,
                                t_msg.msg));
    return t_reply && dbus_message_get_type(t_reply.msg) != DBUS_MESSAGE_TYPE_ERROR;
}

bool MCCredentialRetrieve(MCStringRef p_service,
                          MCStringRef p_account,
                          MCStringRef &r_secret)
{
    DBusError t_err;
    dbus_error_init(&t_err);
    DBusConnGuard t_conn(dbus_bus_get(DBUS_BUS_SESSION, &t_err));
    if (dbus_error_is_set(&t_err))
        dbus_error_free(&t_err);
    if (!t_conn)
        return false;

    if (!dbus_bus_name_has_owner(t_conn, SS_SERVICE, &t_err))
    {
        if (dbus_error_is_set(&t_err))
            dbus_error_free(&t_err);
        return false;
    }

    std::string t_svc  = s_utf8(p_service);
    std::string t_acc  = s_utf8(p_account);

    std::string t_item_path = s_find_item(t_conn, t_svc, t_acc);
    if (t_item_path.empty())
        return false;

    std::string t_session = s_open_session(t_conn);
    if (t_session.empty())
        return false;

    // Call GetSecret on the item.
    DBusMsgGuard t_msg(dbus_message_new_method_call(
        SS_SERVICE, t_item_path.c_str(), SS_ITEM_IFACE, "GetSecret"));
    if (!t_msg)
        return false;

    const char *t_spath = t_session.c_str();
    dbus_message_append_args(t_msg.msg,
        DBUS_TYPE_OBJECT_PATH, &t_spath,
        DBUS_TYPE_INVALID);

    DBusMsgGuard t_reply(s_call(t_conn, nullptr, nullptr, nullptr, nullptr,
                                t_msg.msg));
    if (!t_reply || dbus_message_get_type(t_reply.msg) == DBUS_MESSAGE_TYPE_ERROR)
        return false;

    // Reply is a Secret struct: (o session, ay params, ay value, s content_type)
    DBusMessageIter t_iter, t_struct, t_arr;
    if (!dbus_message_iter_init(t_reply.msg, &t_iter))
        return false;
    if (dbus_message_iter_get_arg_type(&t_iter) != DBUS_TYPE_STRUCT)
        return false;

    dbus_message_iter_recurse(&t_iter, &t_struct);
    // skip session path
    dbus_message_iter_next(&t_struct);
    // skip params array
    dbus_message_iter_next(&t_struct);
    // read value array (bytes)
    if (dbus_message_iter_get_arg_type(&t_struct) != DBUS_TYPE_ARRAY)
        return false;

    dbus_message_iter_recurse(&t_struct, &t_arr);
    const uint8_t *t_bytes = nullptr;
    int t_len = 0;
    dbus_message_iter_get_fixed_array(&t_arr,
        reinterpret_cast<const void **>(&t_bytes), &t_len);

    if (t_bytes == nullptr || t_len < 0)
        return false;

    return MCStringCreateWithBytes(t_bytes, (uindex_t)t_len,
                                   kMCStringEncodingUTF8, false, r_secret);
}

bool MCCredentialDelete(MCStringRef p_service,
                        MCStringRef p_account)
{
    DBusError t_err;
    dbus_error_init(&t_err);
    DBusConnGuard t_conn(dbus_bus_get(DBUS_BUS_SESSION, &t_err));
    if (dbus_error_is_set(&t_err))
        dbus_error_free(&t_err);
    if (!t_conn)
        return false;

    if (!dbus_bus_name_has_owner(t_conn, SS_SERVICE, &t_err))
    {
        if (dbus_error_is_set(&t_err))
            dbus_error_free(&t_err);
        return false;
    }

    std::string t_svc = s_utf8(p_service);
    std::string t_acc = s_utf8(p_account);

    std::string t_item_path = s_find_item(t_conn, t_svc, t_acc);
    if (t_item_path.empty())
        return true;    // Already gone — not an error.

    DBusMsgGuard t_msg(dbus_message_new_method_call(
        SS_SERVICE, t_item_path.c_str(), SS_ITEM_IFACE, "Delete"));
    if (!t_msg)
        return false;

    DBusMsgGuard t_reply(s_call(t_conn, nullptr, nullptr, nullptr, nullptr,
                                t_msg.msg));
    return t_reply && dbus_message_get_type(t_reply.msg) != DBUS_MESSAGE_TYPE_ERROR;
}
