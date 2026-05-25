/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

// mac-battery.mm
//
// macOS implementation of MCBatteryGetLevel / MCBatteryGetPowerSource using
// the IOKit Power Sources API (IOPowerSources.h).
//
// No entitlement or permission is required to read power source information.

#include "prefix.h"

#include "exec-battery.h"

#include <IOKit/ps/IOPowerSources.h>
#include <IOKit/ps/IOPSKeys.h>

////////////////////////////////////////////////////////////////////////////////

// Returns the first battery power source dictionary, or NULL if there is none.
// The caller is responsible for releasing the returned CFDictionaryRef.
static CFDictionaryRef s_copy_battery_dict(CFTypeRef p_info)
{
    CFArrayRef t_list = IOPSCopyPowerSourcesList(p_info);
    if (t_list == NULL)
        return NULL;

    CFDictionaryRef t_result = NULL;
    CFIndex t_count = CFArrayGetCount(t_list);

    for (CFIndex i = 0; i < t_count; i++)
    {
        CFTypeRef t_source = CFArrayGetValueAtIndex(t_list, i);
        CFDictionaryRef t_desc = IOPSGetPowerSourceDescription(p_info, t_source);
        if (t_desc == NULL)
            continue;

        CFStringRef t_type = (CFStringRef)CFDictionaryGetValue(t_desc,
                                                CFSTR(kIOPSTypeKey));
        if (t_type != NULL &&
            CFStringCompare(t_type, CFSTR(kIOPSInternalBatteryType),
                            0) == kCFCompareEqualTo)
        {
            // Retain so we can release the list safely.
            t_result = (CFDictionaryRef)CFRetain(t_desc);
            break;
        }
    }

    CFRelease(t_list);
    return t_result;
}

////////////////////////////////////////////////////////////////////////////////

bool MCBatteryGetLevel(integer_t &r_level)
{
    CFTypeRef t_info = IOPSCopyPowerSourcesInfo();
    if (t_info == NULL)
        return false;

    CFDictionaryRef t_batt = s_copy_battery_dict(t_info);
    CFRelease(t_info);

    if (t_batt == NULL)
    {
        // No internal battery — desktop or undetectable.
        r_level = -1;
        return true;
    }

    // Current capacity and max capacity are both in percent units on macOS.
    CFNumberRef t_current = (CFNumberRef)CFDictionaryGetValue(t_batt,
                                             CFSTR(kIOPSCurrentCapacityKey));
    CFNumberRef t_max     = (CFNumberRef)CFDictionaryGetValue(t_batt,
                                             CFSTR(kIOPSMaxCapacityKey));
    CFRelease(t_batt);

    if (t_current == NULL || t_max == NULL)
    {
        r_level = -1;
        return true;
    }

    int t_cur_val = 0, t_max_val = 0;
    CFNumberGetValue(t_current, kCFNumberIntType, &t_cur_val);
    CFNumberGetValue(t_max,     kCFNumberIntType, &t_max_val);

    if (t_max_val <= 0)
    {
        r_level = -1;
        return true;
    }

    // Clamp to [0, 100].
    r_level = (integer_t)((t_cur_val * 100 + t_max_val / 2) / t_max_val);
    if (r_level < 0)   r_level = 0;
    if (r_level > 100) r_level = 100;
    return true;
}

bool MCBatteryGetPowerSource(MCStringRef &r_source)
{
    CFTypeRef t_info = IOPSCopyPowerSourcesInfo();
    if (t_info == NULL)
        return false;

    // IOPSGetProvidingPowerSourceType returns one of:
    //   kIOPSACPowerValue, kIOPSBatteryPowerValue, or kIOPSOffLineValue.
    CFStringRef t_type = IOPSGetProvidingPowerSourceType(t_info);
    CFRelease(t_info);

    if (t_type == NULL)
        return MCStringCreateWithCString("unknown", r_source);

    if (CFStringCompare(t_type, CFSTR(kIOPSBatteryPowerValue), 0) == kCFCompareEqualTo)
        return MCStringCreateWithCString("battery", r_source);

    if (CFStringCompare(t_type, CFSTR(kIOPSACPowerValue), 0) == kCFCompareEqualTo)
        return MCStringCreateWithCString("ac", r_source);

    return MCStringCreateWithCString("unknown", r_source);
}
