/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

#ifndef __MC_EXEC_CREDENTIALS__
#define __MC_EXEC_CREDENTIALS__

#include "exec.h"

////////////////////////////////////////////////////////////////////////////////
// Platform entry points (implemented per-platform)

// Store a secret in the platform credential store.
// Returns false only on an unexpected platform error; on success returns true.
bool MCCredentialStore(MCStringRef p_service,
                       MCStringRef p_account,
                       MCStringRef p_secret);

// Retrieve a previously stored secret.
// Returns true and sets r_secret on success.
// Returns false if the credential does not exist or on a platform error.
bool MCCredentialRetrieve(MCStringRef p_service,
                          MCStringRef p_account,
                          MCStringRef &r_secret);

// Delete a stored credential.
// Returns true on success or if the credential did not exist.
// Returns false on a platform error.
bool MCCredentialDelete(MCStringRef p_service,
                        MCStringRef p_account);

////////////////////////////////////////////////////////////////////////////////
// Exec-layer eval functions (called by the engine function dispatch)

void MCCredentialsEvalStoreCredential(MCExecContext &ctxt,
                                      MCStringRef p_service,
                                      MCStringRef p_account,
                                      MCStringRef p_secret,
                                      bool &r_result);

void MCCredentialsEvalRetrieveCredential(MCExecContext &ctxt,
                                         MCStringRef p_service,
                                         MCStringRef p_account,
                                         MCStringRef &r_secret);

void MCCredentialsEvalDeleteCredential(MCExecContext &ctxt,
                                       MCStringRef p_service,
                                       MCStringRef p_account,
                                       bool &r_result);

#endif // __MC_EXEC_CREDENTIALS__
