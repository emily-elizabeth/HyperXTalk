/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

// exec-battery.cpp
//
// Platform-agnostic exec layer for the battery functions:
//   batteryLevel()   — charge percentage (0–100) or -1 if no battery
//   powerSource()    — "battery", "ac", or "unknown"
//
// Each function delegates to a platform-specific MCBattery* entry point.
// On an unexpected platform error the result is set to an error string and
// a safe fallback value (-1 / "unknown") is returned.

#include "prefix.h"

#include "exec.h"
#include "exec-battery.h"

////////////////////////////////////////////////////////////////////////////////

void MCBatteryEvalBatteryLevel(MCExecContext &ctxt, integer_t &r_level)
{
    integer_t t_level = -1;
    if (!MCBatteryGetLevel(t_level))
    {
        ctxt.SetTheResultToCString("batteryLevel: could not query battery");
        r_level = -1;
        return;
    }
    r_level = t_level;
}

void MCBatteryEvalPowerSource(MCExecContext &ctxt, MCStringRef &r_source)
{
    MCStringRef t_source = nil;
    if (!MCBatteryGetPowerSource(t_source))
    {
        ctxt.SetTheResultToCString("powerSource: could not query power source");
        r_source = MCValueRetain(MCSTR("unknown"));
        return;
    }
    r_source = t_source;
}
