/* Copyright (C) 2024 HyperXTalk contributors.
   GPL v3 — see lnx-hotkey.cpp for full notice. */

#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int  lnx_hotkey_x11_init(int write_fd);       // open display, start thread; idempotent
int  lnx_hotkey_x11_display_open(void);       // 1 if display is open

int  lnx_hotkey_x11_parse(const char *p_key,
                           unsigned   *r_modifiers,
                           unsigned   *r_keycode,
                           char       *r_error,
                           size_t      p_error_len);

int  lnx_hotkey_x11_grab  (unsigned modifiers, unsigned keycode);
void lnx_hotkey_x11_ungrab(unsigned modifiers, unsigned keycode);
void lnx_hotkey_x11_flush (void);

int  lnx_hotkey_x11_store            (int32_t engine_id, unsigned keycode, unsigned modifiers);
int  lnx_hotkey_x11_remove           (int32_t engine_id, unsigned *r_keycode, unsigned *r_modifiers);
void lnx_hotkey_x11_remove_all_and_ungrab(void);

#ifdef __cplusplus
}
#endif
