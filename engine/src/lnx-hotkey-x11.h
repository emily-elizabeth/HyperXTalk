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

#ifndef LNX_HOTKEY_X11_H
#define LNX_HOTKEY_X11_H

//
// Plain-C bridge between the X11 hotkey backend and the engine.
//
// This header intentionally uses only <stdint.h> types so it can be included
// by both the X11 translation unit (which must not see engine typedefs) and
// the engine translation unit (which must not see X11 typedefs).  No X11
// headers and no prefix.h / engine headers are included here.
//

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

// Open a background X display and start the event thread.  Must be called
// before any grab/parse functions.  Safe to call multiple times.
bool lnx_hotkey_x11_init(void);

// Release all grabs, stop the thread, close the display.
void lnx_hotkey_x11_shutdown(void);

// ---------------------------------------------------------------------------
// Self-pipe
// ---------------------------------------------------------------------------

// Create (if needed) the self-pipe used to marshal hotkey IDs to the main
// thread.  Returns true on success.  The read fd is retrieved separately so
// the engine TU can register it with GLib without needing any X11 headers.
bool lnx_hotkey_x11_ensure_pipe(void);

// Returns the read end of the self-pipe, or -1 if not yet created.
// The engine TU passes this fd to g_io_add_watch().
int lnx_hotkey_x11_pipe_read_fd(void);

// ---------------------------------------------------------------------------
// Grab / ungrab
// ---------------------------------------------------------------------------

// Register a hotkey given an already-parsed modifier mask and X11 keycode.
// p_modifiers and p_keycode are the raw X11 values returned by
// lnx_hotkey_x11_parse_key().
bool lnx_hotkey_x11_grab(int32_t p_engine_id,
                          unsigned int p_modifiers,
                          unsigned int p_keycode);

// Unregister the hotkey with the given engine ID.
void lnx_hotkey_x11_ungrab(int32_t p_engine_id);

// Unregister all hotkeys.
void lnx_hotkey_x11_ungrab_all(void);

// ---------------------------------------------------------------------------
// Key string parsing
// ---------------------------------------------------------------------------

// Parse a "Ctrl+Shift+H"-style string into an X11 modifier mask and keycode.
// p_str      — null-terminated key string (ASCII)
// r_modifiers — receives the X11 modifier mask
// r_keycode   — receives the X11 KeyCode (unsigned int to avoid X11 typedef)
// r_error     — receives a human-readable error message on failure
// p_error_len — size of the r_error buffer
// Returns true on success.
bool lnx_hotkey_x11_parse_key(const char  *p_str,
                               unsigned int *r_modifiers,
                               unsigned int *r_keycode,
                               char         *r_error,
                               size_t        p_error_len);

#ifdef __cplusplus
}
#endif

#endif /* LNX_HOTKEY_X11_H */
