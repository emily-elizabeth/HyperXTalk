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
// Cross-platform hotkey: statement implementations, registry, and engine callbacks.
//

#include "prefix.h"

#include "globdefs.h"
#include "filedefs.h"
#include "objdefs.h"
#include "parsedef.h"

#include "scriptpt.h"
#include "globals.h"
#include "stack.h"
#include "card.h"
#include "exec.h"
#include "param.h"
#include "mcerror.h"
#include "executionerrors.h"
#include "parseerrors.h"
#include "express.h"

#include "mcstring.h"
#include "cmds.h"
#include "hotkey.h"

////////////////////////////////////////////////////////////////////////////////
// Hotkey registry
//
// Stores the mapping of engine ID → (key string, handler name) so we can
// fire the right message when a hotkey is pressed, and release resources when
// hotkeys are unregistered.

struct MCHotkeyEntry
{
    int32_t    id;          // unique engine-assigned integer
    MCStringRef key;        // normalised key string, e.g. "Ctrl+Shift+H"
    MCNameRef  handler;     // message to fire, e.g. "myHandler"
    MCStack   *owner_stack; // stack that called registerHotkey (weak ref)
};

static MCHotkeyEntry *s_entries     = nullptr;
static uindex_t       s_entry_count = 0;
static int32_t        s_next_id     = 1;

// Find an entry by key string (case-insensitive).  Returns index or -1.
static intptr_t _find_by_key(MCStringRef p_key)
{
    for (uindex_t i = 0; i < s_entry_count; i++)
    {
        if (MCStringIsEqualTo(s_entries[i].key, p_key, kMCStringOptionCompareCaseless))
            return (intptr_t)i;
    }
    return -1;
}

// Find an entry by engine ID.  Returns index or -1.
static intptr_t _find_by_id(int32_t p_id)
{
    for (uindex_t i = 0; i < s_entry_count; i++)
    {
        if (s_entries[i].id == p_id)
            return (intptr_t)i;
    }
    return -1;
}

// Remove entry at index (release strings, compact array).
static void _remove_entry(uindex_t p_index)
{
    MCValueRelease(s_entries[p_index].key);
    MCValueRelease(s_entries[p_index].handler);
    // Shift remaining entries down.
    for (uindex_t i = p_index + 1; i < s_entry_count; i++)
        s_entries[i - 1] = s_entries[i];
    s_entry_count--;
}

////////////////////////////////////////////////////////////////////////////////
// Engine-side dispatch helper

static void _dispatch_to_stack(MCStack *p_stack, MCNameRef p_msg)
{
    // Fall back to MCdefaultstackptr if no owner was captured.
    MCStack *t_stack = p_stack != nil ? p_stack : MCdefaultstackptr;
    if (t_stack == nil)
        return;
    MCCard *t_card = t_stack->getcurcard();
    if (t_card != nil)
        t_card->message(p_msg);
}

void MCHotkeyDispatchFired(int32_t p_id)
{
    intptr_t t_idx = _find_by_id(p_id);
    if (t_idx < 0)
        return;
    _dispatch_to_stack(s_entries[t_idx].owner_stack, s_entries[t_idx].handler);
}

////////////////////////////////////////////////////////////////////////////////
// registerHotkey key, handlerName

MCRegisterHotkey::~MCRegisterHotkey()
{
    delete m_key;
    delete m_handler;
}

// Helper: skip an optional comma between arguments.
static void _skip_optional_comma(MCScriptPoint& sp)
{
    Symbol_type t_type;
    if (sp.next(t_type) != PS_NORMAL || t_type != ST_SEP)
        sp.backup();
}

Parse_stat MCRegisterHotkey::parse(MCScriptPoint& sp)
{
    initpoint(sp);

    if (sp.parseexp(False, False, &m_key) != PS_NORMAL)
    {
        MCperror->add(PE_REGISTERHOTKEY_BADKEY, sp);
        return PS_ERROR;
    }

    _skip_optional_comma(sp);

    if (sp.parseexp(False, True, &m_handler) != PS_NORMAL)
    {
        MCperror->add(PE_REGISTERHOTKEY_BADHANDLER, sp);
        return PS_ERROR;
    }

    return PS_NORMAL;
}

void MCRegisterHotkey::exec_ctxt(MCExecContext& ctxt)
{
    MCAutoStringRef t_key;
    if (!ctxt.EvalExprAsStringRef(m_key, EE_REGISTERHOTKEY_BADKEY, &t_key))
        return;

    MCAutoStringRef t_handler_str;
    if (!ctxt.EvalExprAsStringRef(m_handler, EE_REGISTERHOTKEY_BADHANDLER, &t_handler_str))
        return;

    // If this key is already registered, unregister it first.
    intptr_t t_existing = _find_by_key(*t_key);
    if (t_existing >= 0)
    {
        MCPlatformUnregisterHotkey(s_entries[t_existing].id);
        _remove_entry((uindex_t)t_existing);
    }

    // Assign a new engine ID and attempt platform registration.
    int32_t t_id = s_next_id++;

    if (!MCPlatformRegisterHotkey(*t_key, t_id))
    {
        // Platform already set the result with an error string.
        return;
    }

    // Grow the registry array.
    MCHotkeyEntry *t_new_entries;
    if (!MCMemoryResizeArray(s_entry_count + 1, s_entries, s_entry_count))
    {
        MCPlatformUnregisterHotkey(t_id);
        ctxt.LegacyThrow(EE_REGISTERHOTKEY_BADKEY);
        return;
    }

    // Store the entry.  Retain both string values.
    MCHotkeyEntry& t_entry = s_entries[s_entry_count - 1];
    t_entry.id          = t_id;
    t_entry.key         = MCValueRetain(*t_key);
    t_entry.owner_stack = MCdefaultstackptr; // capture calling stack at registration time

    MCNameRef t_handler_name;
    /* UNCHECKED */ MCNameCreate(*t_handler_str, t_handler_name);
    t_entry.handler = t_handler_name;
}

////////////////////////////////////////////////////////////////////////////////
// unregisterHotkey key

MCUnregisterHotkey::~MCUnregisterHotkey()
{
    delete m_key;
}

Parse_stat MCUnregisterHotkey::parse(MCScriptPoint& sp)
{
    initpoint(sp);
    if (sp.parseexp(False, True, &m_key) != PS_NORMAL)
    {
        MCperror->add(PE_UNREGISTERHOTKEY_BADKEY, sp);
        return PS_ERROR;
    }
    return PS_NORMAL;
}

void MCUnregisterHotkey::exec_ctxt(MCExecContext& ctxt)
{
    MCAutoStringRef t_key;
    if (!ctxt.EvalExprAsStringRef(m_key, EE_UNREGISTERHOTKEY_BADKEY, &t_key))
        return;

    intptr_t t_idx = _find_by_key(*t_key);
    if (t_idx < 0)
        return;  // no-op if not registered

    MCPlatformUnregisterHotkey(s_entries[t_idx].id);
    _remove_entry((uindex_t)t_idx);
}

////////////////////////////////////////////////////////////////////////////////
// unregisterAllHotkeys

MCUnregisterAllHotkeys::~MCUnregisterAllHotkeys() {}

Parse_stat MCUnregisterAllHotkeys::parse(MCScriptPoint& sp)
{
    initpoint(sp);
    return PS_NORMAL;
}

void MCUnregisterAllHotkeys::exec_ctxt(MCExecContext& ctxt)
{
    MCPlatformUnregisterAllHotkeys();

    // Release all registry entries.
    for (uindex_t i = 0; i < s_entry_count; i++)
    {
        MCValueRelease(s_entries[i].key);
        MCValueRelease(s_entries[i].handler);
    }
    MCMemoryDeleteArray(s_entries);
    s_entries     = nullptr;
    s_entry_count = 0;
}
