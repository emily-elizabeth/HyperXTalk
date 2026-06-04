/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

// exec-credentials.cpp
//
// Platform-agnostic exec layer for the credential storage functions:
//   storeCredential(service, account, secret)
//   retrieveCredential(service, account)
//   deleteCredential(service, account)
//
// Each function delegates to a platform-specific MCCredential* entry point.
// On failure the result is set to an error string; the return value is set
// to false (store/delete) or empty (retrieve).

#include "prefix.h"

#include "exec.h"
#include "exec-credentials.h"

////////////////////////////////////////////////////////////////////////////////

void MCCredentialsEvalStoreCredential(MCExecContext &ctxt,
                                      MCStringRef p_service,
                                      MCStringRef p_account,
                                      MCStringRef p_secret,
                                      bool &r_result)
{
    if (!MCCredentialStore(p_service, p_account, p_secret))
    {
        ctxt.SetTheResultToCString("storeCredential: could not store credential");
        r_result = false;
        return;
    }
    r_result = true;
}

void MCCredentialsEvalRetrieveCredential(MCExecContext &ctxt,
                                         MCStringRef p_service,
                                         MCStringRef p_account,
                                         MCStringRef &r_secret)
{
    MCStringRef t_secret = nil;
    if (!MCCredentialRetrieve(p_service, p_account, t_secret))
    {
        ctxt.SetTheResultToCString("retrieveCredential: credential not found");
        r_secret = MCValueRetain(kMCEmptyString);
        return;
    }
    r_secret = t_secret;
}

void MCCredentialsEvalDeleteCredential(MCExecContext &ctxt,
                                       MCStringRef p_service,
                                       MCStringRef p_account,
                                       bool &r_result)
{
    if (!MCCredentialDelete(p_service, p_account))
    {
        ctxt.SetTheResultToCString("deleteCredential: could not delete credential");
        r_result = false;
        return;
    }
    r_result = true;
}
