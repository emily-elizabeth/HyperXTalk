/* Copyright (C) 2024 HyperXTalk contributors.

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.  */

//
// Hotkey stubs for the Linux server build.
// The server has no display and therefore no global hotkey support.
// These stubs satisfy the linker; lnx-hotkey.cpp provides the real
// implementations for the desktop build.
//

#include "prefix.h"
#include "hotkey.h"

bool MCPlatformRegisterHotkey(MCStringRef /*p_key*/, int32_t /*p_id*/)
{
    return false;
}

void MCPlatformUnregisterHotkey(int32_t /*p_id*/)
{
}

void MCPlatformUnregisterAllHotkeys()
{
}
