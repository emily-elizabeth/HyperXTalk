/*
Copyright (C) 2026 HyperXTalk

This file is part of HyperXTalk.

HyperXTalk is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License v3 as published by the Free
Software Foundation.

HyperXTalk is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
HyperXTalk. If not, see <http://www.gnu.org/licenses/>.
*/

//
// Linux voice command backend — stub implementation.
//
// Speech recognition on Linux is not yet implemented.
// startListening sets the result to a "not supported" string and returns false.
// All other entry points are no-ops.
//

#include "prefix.h"
#include "mcstring.h"
#include "globals.h"
#include "speech.h"

bool MCPlatformStartListening(MCStringRef /*p_language*/)
{
    MCStringRef t_err;
    /* UNCHECKED */ MCStringCreateWithCString(
        "startListening: speech recognition is not yet supported on Linux", t_err);
    MCresult->setvalueref(t_err);
    MCValueRelease(t_err);
    return false;
}

void MCPlatformStopListening()
{
    // no-op
}

bool MCPlatformIsListening()
{
    return false;
}

void MCPlatformSetVoiceCommands(MCStringRef /*p_phrases*/)
{
    // no-op
}

void MCPlatformSetWakeWord(MCStringRef /*p_word*/, uint32_t /*p_timeout_ms*/)
{
    // no-op
}
