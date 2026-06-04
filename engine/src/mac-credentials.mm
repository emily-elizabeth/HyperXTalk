/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

// mac-credentials.mm
//
// macOS implementation of MCCredentialStore / MCCredentialRetrieve /
// MCCredentialDelete using the Security framework Keychain Services API
// (SecItem.h).
//
// Credentials are stored as kSecClassGenericPassword items keyed by
// service (kSecAttrService) and account (kSecAttrAccount).  They are
// accessible when the device is unlocked and are stored in the user's
// login keychain.
//
// No entitlement is required for the login keychain on macOS.

#include "prefix.h"

#include "exec-credentials.h"

#include <Security/Security.h>

////////////////////////////////////////////////////////////////////////////////
// Helpers

static CFStringRef s_cf_string(MCStringRef p_str)
{
    CFStringRef t_result = nil;
    MCStringConvertToCFStringRef(p_str, t_result);
    return t_result;    // caller must CFRelease
}

////////////////////////////////////////////////////////////////////////////////

bool MCCredentialStore(MCStringRef p_service,
                       MCStringRef p_account,
                       MCStringRef p_secret)
{
    CFStringRef t_service = s_cf_string(p_service);
    CFStringRef t_account = s_cf_string(p_account);
    if (t_service == nil || t_account == nil)
    {
        if (t_service) CFRelease(t_service);
        if (t_account) CFRelease(t_account);
        return false;
    }

    // Convert secret to UTF-8 bytes.
    MCAutoStringRefAsCFString t_cf_secret;
    if (!t_cf_secret.Lock(p_secret))
    {
        CFRelease(t_service);
        CFRelease(t_account);
        return false;
    }
    CFDataRef t_data = CFStringCreateExternalRepresentation(
        kCFAllocatorDefault, *t_cf_secret, kCFStringEncodingUTF8, 0);
    if (t_data == nil)
    {
        CFRelease(t_service);
        CFRelease(t_account);
        return false;
    }

    // Build the base query used for both lookup and update.
    CFMutableDictionaryRef t_query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 4,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(t_query, kSecClass,        kSecClassGenericPassword);
    CFDictionarySetValue(t_query, kSecAttrService,  t_service);
    CFDictionarySetValue(t_query, kSecAttrAccount,  t_account);

    // Attributes to set / update.
    CFMutableDictionaryRef t_attrs = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 2,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(t_attrs, kSecValueData, t_data);
    CFDictionarySetValue(t_attrs, kSecAttrAccessible,
                         kSecAttrAccessibleWhenUnlocked);

    // Try update first; if the item doesn't exist, add it.
    OSStatus t_status = SecItemUpdate(t_query, t_attrs);
    if (t_status == errSecItemNotFound)
    {
        // Merge query + attrs into a single add dict.
        CFDictionarySetValue(t_query, kSecValueData, t_data);
        CFDictionarySetValue(t_query, kSecAttrAccessible,
                             kSecAttrAccessibleWhenUnlocked);
        t_status = SecItemAdd(t_query, nil);
    }

    CFRelease(t_query);
    CFRelease(t_attrs);
    CFRelease(t_data);
    CFRelease(t_service);
    CFRelease(t_account);

    return t_status == errSecSuccess;
}

bool MCCredentialRetrieve(MCStringRef p_service,
                          MCStringRef p_account,
                          MCStringRef &r_secret)
{
    CFStringRef t_service = s_cf_string(p_service);
    CFStringRef t_account = s_cf_string(p_account);
    if (t_service == nil || t_account == nil)
    {
        if (t_service) CFRelease(t_service);
        if (t_account) CFRelease(t_account);
        return false;
    }

    CFMutableDictionaryRef t_query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 5,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(t_query, kSecClass,            kSecClassGenericPassword);
    CFDictionarySetValue(t_query, kSecAttrService,      t_service);
    CFDictionarySetValue(t_query, kSecAttrAccount,      t_account);
    CFDictionarySetValue(t_query, kSecReturnData,       kCFBooleanTrue);
    CFDictionarySetValue(t_query, kSecMatchLimit,       kSecMatchLimitOne);

    CFTypeRef t_result = nil;
    OSStatus t_status = SecItemCopyMatching(t_query, &t_result);

    CFRelease(t_query);
    CFRelease(t_service);
    CFRelease(t_account);

    if (t_status != errSecSuccess || t_result == nil)
        return false;

    // The result is CFDataRef containing the UTF-8 secret.
    CFDataRef t_data = (CFDataRef)t_result;
    CFStringRef t_cf_secret = CFStringCreateFromExternalRepresentation(
        kCFAllocatorDefault, t_data, kCFStringEncodingUTF8);
    CFRelease(t_data);

    if (t_cf_secret == nil)
        return false;

    bool t_ok = MCStringCreateWithCFStringRef(t_cf_secret, r_secret);
    CFRelease(t_cf_secret);
    return t_ok;
}

bool MCCredentialDelete(MCStringRef p_service,
                        MCStringRef p_account)
{
    CFStringRef t_service = s_cf_string(p_service);
    CFStringRef t_account = s_cf_string(p_account);
    if (t_service == nil || t_account == nil)
    {
        if (t_service) CFRelease(t_service);
        if (t_account) CFRelease(t_account);
        return false;
    }

    CFMutableDictionaryRef t_query = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 3,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(t_query, kSecClass,        kSecClassGenericPassword);
    CFDictionarySetValue(t_query, kSecAttrService,  t_service);
    CFDictionarySetValue(t_query, kSecAttrAccount,  t_account);

    OSStatus t_status = SecItemDelete(t_query);

    CFRelease(t_query);
    CFRelease(t_service);
    CFRelease(t_account);

    // errSecItemNotFound is not an error for delete — the credential is gone.
    return t_status == errSecSuccess || t_status == errSecItemNotFound;
}
