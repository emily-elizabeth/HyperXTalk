/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

// w32-battery.cpp
//
// Windows implementation of MCBatteryGetLevel / MCBatteryGetPowerSource using
// GetSystemPowerStatus (kernel32, available on all Windows versions).
//
// SYSTEM_POWER_STATUS.BatteryLifePercent:
//   0–100  = charge percentage
//   255    = status unknown (no battery / reading unavailable)
//
// SYSTEM_POWER_STATUS.ACLineStatus:
//   0 = offline (battery power)
//   1 = online  (AC power)
//   255 = unknown

#include "prefix.h"

#include "exec-battery.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

////////////////////////////////////////////////////////////////////////////////

bool MCBatteryGetLevel(integer_t &r_level)
{
    SYSTEM_POWER_STATUS t_status;
    if (!GetSystemPowerStatus(&t_status))
        return false;

    if (t_status.BatteryLifePercent == 255)
    {
        // No battery present or status unavailable.
        r_level = -1;
    }
    else
    {
        r_level = (integer_t)t_status.BatteryLifePercent;
        if (r_level < 0)   r_level = 0;
        if (r_level > 100) r_level = 100;
    }
    return true;
}

bool MCBatteryGetPowerSource(MCStringRef &r_source)
{
    SYSTEM_POWER_STATUS t_status;
    if (!GetSystemPowerStatus(&t_status))
        return false;

    const char *t_str;
    switch (t_status.ACLineStatus)
    {
        case 0:   t_str = "battery"; break;
        case 1:   t_str = "ac";      break;
        default:  t_str = "unknown"; break;
    }
    return MCStringCreateWithCString(t_str, r_source);
}
