/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

// w32-credentials.cpp
//
// Windows implementation of MCCredentialStore / MCCredentialRetrieve /
// MCCredentialDelete using the Windows Credential Manager API
// (wincred.h, advapi32).
//
// Credentials are stored as CRED_TYPE_GENERIC items.  The target name is
// formed as "<service>/<account>" to produce a unique lookup key.
// The secret is stored as the credential blob (arbitrary bytes); we store
// it as UTF-16LE so round-tripping through MCStringRef is lossless.
//
// No special privilege is required; credentials are per-user and roam
// with the user profile on domain-joined machines.

#include "prefix.h"

#include "exec-credentials.h"

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#include <windows.h>
#include <wincred.h>
#include <string>

#pragma comment(lib, "advapi32.lib")

////////////////////////////////////////////////////////////////////////////////
// Helpers

// Build the target name "<service>/<account>" as a std::wstring.
static std::wstring s_target_name(MCStringRef p_service, MCStringRef p_account)
{
    uindex_t t_svc_len = MCStringGetLength(p_service);
    uindex_t t_acc_len = MCStringGetLength(p_account);

    std::wstring t_result(t_svc_len + 1 + t_acc_len, L'\0');
    MCStringGetChars(p_service, MCRangeMake(0, t_svc_len),
                     (unichar_t *)t_result.data());
    t_result[t_svc_len] = L'/';
    MCStringGetChars(p_account, MCRangeMake(0, t_acc_len),
                     (unichar_t *)(t_result.data() + t_svc_len + 1));
    return t_result;
}

// Convert MCStringRef to a std::wstring.
static std::wstring s_to_wide(MCStringRef p_str)
{
    uindex_t t_len = MCStringGetLength(p_str);
    std::wstring t_result(t_len, L'\0');
    MCStringGetChars(p_str, MCRangeMake(0, t_len),
                     (unichar_t *)t_result.data());
    return t_result;
}

////////////////////////////////////////////////////////////////////////////////

bool MCCredentialStore(MCStringRef p_service,
                       MCStringRef p_account,
                       MCStringRef p_secret)
{
    std::wstring t_target  = s_target_name(p_service, p_account);
    std::wstring t_user    = s_to_wide(p_account);
    std::wstring t_secret  = s_to_wide(p_secret);

    CREDENTIALW t_cred    = {};
    t_cred.Type           = CRED_TYPE_GENERIC;
    t_cred.TargetName     = const_cast<LPWSTR>(t_target.c_str());
    t_cred.UserName       = const_cast<LPWSTR>(t_user.c_str());
    t_cred.CredentialBlob = reinterpret_cast<LPBYTE>(
                                const_cast<wchar_t *>(t_secret.c_str()));
    t_cred.CredentialBlobSize = (DWORD)(t_secret.size() * sizeof(wchar_t));
    t_cred.Persist        = CRED_PERSIST_LOCAL_MACHINE;

    return CredWriteW(&t_cred, 0) != FALSE;
}

bool MCCredentialRetrieve(MCStringRef p_service,
                          MCStringRef p_account,
                          MCStringRef &r_secret)
{
    std::wstring t_target = s_target_name(p_service, p_account);

    PCREDENTIALW t_cred = nullptr;
    if (!CredReadW(t_target.c_str(), CRED_TYPE_GENERIC, 0, &t_cred))
        return false;

    // The blob is a UTF-16LE wchar_t array (no null terminator stored).
    uindex_t t_char_count = t_cred->CredentialBlobSize / sizeof(wchar_t);
    bool t_ok = MCStringCreateWithChars(
        reinterpret_cast<const unichar_t *>(t_cred->CredentialBlob),
        t_char_count, r_secret);

    CredFree(t_cred);
    return t_ok;
}

bool MCCredentialDelete(MCStringRef p_service,
                        MCStringRef p_account)
{
    std::wstring t_target = s_target_name(p_service, p_account);

    BOOL t_ok = CredDeleteW(t_target.c_str(), CRED_TYPE_GENERIC, 0);
    // ERROR_NOT_FOUND is not a failure — credential is already gone.
    return t_ok || GetLastError() == ERROR_NOT_FOUND;
}
