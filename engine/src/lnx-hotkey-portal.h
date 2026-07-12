/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.  */

//
// XDG GlobalShortcuts portal backend — Wayland global hotkeys.
// Pure C interface; included by both lnx-hotkey.cpp (engine glue, has
// prefix.h) and lnx-hotkey-portal.cpp (implementation, no prefix.h).
//

#ifndef LNX_HOTKEY_PORTAL_H
#define LNX_HOTKEY_PORTAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Returns 1 if the XDG GlobalShortcuts portal is reachable on the session
// D-Bus (i.e. we're on a Wayland desktop that supports it).
int lnx_hotkey_portal_available(void);

// Connect to D-Bus, create a portal session, start the event thread.
// write_fd is the write end of the self-pipe used by lnx-hotkey.cpp.
// Returns 1 on success, 0 on failure.
int lnx_hotkey_portal_init(int write_fd);

// Register a hotkey with the portal.
// p_key is in HyperXTalk format ("Ctrl+Shift+P").
// p_error / p_error_len receive a human-readable message on failure.
// Returns 1 on success, 0 on failure.
int lnx_hotkey_portal_register(int32_t engine_id,
                                const char *p_key,
                                char *p_error,
                                size_t p_error_len);

// Unregister a single hotkey by engine ID.
void lnx_hotkey_portal_unregister(int32_t engine_id);

// Unregister all hotkeys.
void lnx_hotkey_portal_unregister_all(void);

#ifdef __cplusplus
}
#endif

#endif /* LNX_HOTKEY_PORTAL_H */
