/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation. */

// lnx-battery.cpp
//
// Linux implementation of MCBatteryGetLevel / MCBatteryGetPowerSource by
// reading the kernel's power_supply sysfs interface.
//
// Battery capacity: /sys/class/power_supply/<name>/capacity  (0–100)
// AC adapter state: /sys/class/power_supply/<name>/online    (0 or 1)
//
// Battery nodes are identified by their 'type' file containing "Battery".
// AC adapter nodes have a 'type' file containing "Mains".
//
// Not all systems enumerate nodes the same way; we scan all entries under
// /sys/class/power_supply/ and check the 'type' file of each.

#include "prefix.h"

#include "exec-battery.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>

////////////////////////////////////////////////////////////////////////////////

static const char *const k_ps_root = "/sys/class/power_supply/";

// Read a single-line text file into p_buf (null-terminated, trailing newline
// stripped).  Returns true on success.
static bool s_read_sysfs(const char *p_path, char *p_buf, size_t p_size)
{
    FILE *t_f = fopen(p_path, "r");
    if (t_f == NULL)
        return false;

    bool t_ok = (fgets(p_buf, (int)p_size, t_f) != NULL);
    fclose(t_f);

    if (t_ok)
    {
        size_t t_len = strlen(p_buf);
        if (t_len > 0 && p_buf[t_len - 1] == '\n')
            p_buf[t_len - 1] = '\0';
    }
    return t_ok;
}

// Read an integer from a sysfs file.  Returns true on success.
static bool s_read_int(const char *p_path, int &r_val)
{
    char t_buf[32];
    if (!s_read_sysfs(p_path, t_buf, sizeof(t_buf)))
        return false;
    return sscanf(t_buf, "%d", &r_val) == 1;
}

////////////////////////////////////////////////////////////////////////////////

bool MCBatteryGetLevel(integer_t &r_level)
{
    DIR *t_dir = opendir(k_ps_root);
    if (t_dir == NULL)
    {
        r_level = -1;
        return true;    // Not an error — just no sysfs (container / VM).
    }

    integer_t t_found = -1;
    struct dirent *t_ent;

    while ((t_ent = readdir(t_dir)) != NULL)
    {
        if (t_ent->d_name[0] == '.')
            continue;

        char t_type_path[256];
        snprintf(t_type_path, sizeof(t_type_path),
                 "%s%s/type", k_ps_root, t_ent->d_name);

        char t_type[32];
        if (!s_read_sysfs(t_type_path, t_type, sizeof(t_type)))
            continue;

        if (strncmp(t_type, "Battery", 7) != 0)
            continue;

        char t_cap_path[256];
        snprintf(t_cap_path, sizeof(t_cap_path),
                 "%s%s/capacity", k_ps_root, t_ent->d_name);

        int t_cap = 0;
        if (s_read_int(t_cap_path, t_cap))
        {
            if (t_cap < 0)   t_cap = 0;
            if (t_cap > 100) t_cap = 100;
            t_found = (integer_t)t_cap;
            break;
        }
    }

    closedir(t_dir);
    r_level = t_found;
    return true;
}

bool MCBatteryGetPowerSource(MCStringRef &r_source)
{
    DIR *t_dir = opendir(k_ps_root);
    if (t_dir == NULL)
        return MCStringCreateWithCString("unknown", r_source);

    // Default: if we find a battery node, assume it's supplying power unless
    // we find an online AC adapter.
    bool t_has_battery = false;
    bool t_ac_online   = false;
    struct dirent *t_ent;

    while ((t_ent = readdir(t_dir)) != NULL)
    {
        if (t_ent->d_name[0] == '.')
            continue;

        char t_type_path[256];
        snprintf(t_type_path, sizeof(t_type_path),
                 "%s%s/type", k_ps_root, t_ent->d_name);

        char t_type[32];
        if (!s_read_sysfs(t_type_path, t_type, sizeof(t_type)))
            continue;

        if (strncmp(t_type, "Battery", 7) == 0)
        {
            t_has_battery = true;
        }
        else if (strncmp(t_type, "Mains", 5) == 0)
        {
            char t_online_path[256];
            snprintf(t_online_path, sizeof(t_online_path),
                     "%s%s/online", k_ps_root, t_ent->d_name);
            int t_online = 0;
            if (s_read_int(t_online_path, t_online) && t_online != 0)
                t_ac_online = true;
        }
    }

    closedir(t_dir);

    const char *t_str;
    if (!t_has_battery)
        t_str = "ac";           // Desktop with no battery node.
    else if (t_ac_online)
        t_str = "ac";           // Laptop plugged in.
    else
        t_str = "battery";      // Running on battery.

    return MCStringCreateWithCString(t_str, r_source);
}
