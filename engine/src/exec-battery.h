/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

#ifndef __MC_EXEC_BATTERY__
#define __MC_EXEC_BATTERY__

#include "exec.h"

////////////////////////////////////////////////////////////////////////////////
// Platform entry points (implemented per-platform)

// Returns the battery charge level as a percentage (0–100), or -1 if no
// battery is present (e.g. a desktop machine running on AC only).
// Returns false only on an unexpected platform error; -1 is not an error.
bool MCBatteryGetLevel(integer_t &r_level);

// Returns one of "battery", "ac", or "unknown" depending on the current
// power source.
bool MCBatteryGetPowerSource(MCStringRef &r_source);

////////////////////////////////////////////////////////////////////////////////
// Exec-layer eval functions (called by the engine function dispatch)

void MCBatteryEvalBatteryLevel(MCExecContext &ctxt, integer_t &r_level);
void MCBatteryEvalPowerSource(MCExecContext &ctxt, MCStringRef &r_source);

#endif // __MC_EXEC_BATTERY__
