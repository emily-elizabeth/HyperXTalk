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
// Cross-platform voice command: statement implementations, phrase registry,
// and engine dispatch callbacks.
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
#include "speech.h"

////////////////////////////////////////////////////////////////////////////////
// Dispatched message name constants
//
// Lazily initialised on first dispatch so we don't pull in MCNameCreate
// before the foundation library is ready.

static MCNameRef s_msg_voice_command          = nil; // "voiceCommand"
static MCNameRef s_msg_wake_word_detected     = nil; // "wakeWordDetected"
static MCNameRef s_msg_listen_timeout_expired = nil; // "listenTimeoutExpired"
static MCNameRef s_msg_unrecognized_input     = nil; // "unrecognizedVoiceCommand"

static bool _ensure_message_names()
{
    if (s_msg_voice_command != nil)
        return true;

    const char *k_vc  = "voiceCommand";
    const char *k_wwd = "wakeWordDetected";
    const char *k_lte = "listenTimeoutExpired";
    const char *k_uri = "unrecognizedVoiceCommand";

    return MCNameCreateWithNativeChars((const char_t *)k_vc,  strlen(k_vc),  &s_msg_voice_command) &&
           MCNameCreateWithNativeChars((const char_t *)k_wwd, strlen(k_wwd), &s_msg_wake_word_detected) &&
           MCNameCreateWithNativeChars((const char_t *)k_lte, strlen(k_lte), &s_msg_listen_timeout_expired) &&
           MCNameCreateWithNativeChars((const char_t *)k_uri, strlen(k_uri), &s_msg_unrecognized_input);
}

////////////////////////////////////////////////////////////////////////////////
// Voice command phrase registry
//
// Each registered phrase is stored with the stack that registered it so that
// voiceCommand can be dispatched to the right place.

struct MCVoiceCommandEntry
{
    MCStringRef  phrase;       // the recognised phrase, e.g. "open stack"
    MCStack     *owner_stack;  // stack that called registerVoiceCommand (weak ref)
};

static MCVoiceCommandEntry *s_entries     = nullptr;
static uindex_t             s_entry_count = 0;

// Find a phrase (case-insensitive).  Returns index or -1.
static intptr_t _find_phrase(MCStringRef p_phrase)
{
    for (uindex_t i = 0; i < s_entry_count; i++)
    {
        if (MCStringIsEqualTo(s_entries[i].phrase, p_phrase, kMCStringOptionCompareCaseless))
            return (intptr_t)i;
    }
    return -1;
}

// Remove entry at index (releases string, compacts array).
static void _remove_entry(uindex_t p_index)
{
    MCValueRelease(s_entries[p_index].phrase);
    for (uindex_t i = p_index + 1; i < s_entry_count; i++)
        s_entries[i - 1] = s_entries[i];
    s_entry_count--;
}

// Build a return-delimited MCStringRef from the current registry and push it
// to the platform.  Called any time the registry changes so the platform's
// recognition grammar stays in sync.
static void _rebuild_platform_list()
{
    if (s_entry_count == 0)
    {
        MCPlatformSetVoiceCommands(kMCEmptyString);
        return;
    }

    MCAutoStringRef t_list;
    /* UNCHECKED */ MCStringCreateMutable(0, &t_list);

    for (uindex_t i = 0; i < s_entry_count; i++)
    {
        if (i > 0)
            /* UNCHECKED */ MCStringAppendChar(*t_list, '\n');
        /* UNCHECKED */ MCStringAppend(*t_list, s_entries[i].phrase);
    }

    MCPlatformSetVoiceCommands(*t_list);
}

////////////////////////////////////////////////////////////////////////////////
// Phrase-splitting helper
//
// registerVoiceCommand accepts either a single phrase or a return-delimited
// list of phrases.  Splits on '\n' and registers each non-empty segment.

static void _register_phrases(MCStringRef p_list, MCStack *p_owner)
{
    uindex_t t_len   = MCStringGetLength(p_list);
    uindex_t t_start = 0;

    for (uindex_t i = 0; i <= t_len; i++)
    {
        bool t_boundary = (i == t_len) || (MCStringGetCharAtIndex(p_list, i) == '\n');
        if (!t_boundary)
            continue;

        if (i == t_start)
        {
            // Empty segment (consecutive or trailing newline) — skip.
            t_start = i + 1;
            continue;
        }

        // Extract phrase[t_start .. i).
        MCAutoStringRef t_phrase;
        /* UNCHECKED */ MCStringCopySubstring(p_list, MCRangeMakeMinMax(t_start, i), &t_phrase);
        t_start = i + 1;

        if (MCStringIsEmpty(*t_phrase))
            continue;

        // Duplicate phrase?  No-op for that entry.
        if (_find_phrase(*t_phrase) >= 0)
            continue;

        // Grow the registry array.
        if (!MCMemoryResizeArray(s_entry_count + 1, s_entries, s_entry_count))
            return; // out of memory — silent failure, partial registration accepted

        MCVoiceCommandEntry& t_entry = s_entries[s_entry_count - 1];
        t_entry.phrase      = MCValueRetain(*t_phrase);
        t_entry.owner_stack = p_owner;
    }
}

////////////////////////////////////////////////////////////////////////////////
// Engine-side dispatch helpers

// Dispatch:  voiceCommand pPhrase
// Targets the owning stack of the matched phrase, or the default stack as
// fallback (matches the hotkey dispatch pattern).
void MCSpeechDispatchVoiceCommand(MCStringRef p_phrase)
{
    if (!_ensure_message_names())
        return;

    intptr_t t_idx = _find_phrase(p_phrase);

    MCStack *t_stack = nil;
    if (t_idx >= 0)
        t_stack = s_entries[t_idx].owner_stack;
    if (t_stack == nil)
        t_stack = MCdefaultstackptr;
    if (t_stack == nil)
        return;

    MCCard *t_card = t_stack->getcurcard();
    if (t_card == nil)
        return;

    // Build a single-argument parameter carrying the matched phrase.
    MCParameter t_param;
    t_param.setvalueref_argument(p_phrase);

    t_card->message(s_msg_voice_command, &t_param);
}

// Dispatch:  wakeWordDetected
void MCSpeechDispatchWakeWordDetected()
{
    if (!_ensure_message_names())
        return;
    if (MCdefaultstackptr == nil)
        return;
    MCCard *t_card = MCdefaultstackptr->getcurcard();
    if (t_card != nil)
        t_card->message(s_msg_wake_word_detected);
}

// Dispatch:  listenTimeoutExpired
void MCSpeechDispatchListenTimeoutExpired()
{
    if (!_ensure_message_names())
        return;
    if (MCdefaultstackptr == nil)
        return;
    MCCard *t_card = MCdefaultstackptr->getcurcard();
    if (t_card != nil)
        t_card->message(s_msg_listen_timeout_expired);
}

// Dispatch:  unrecognizedVoiceCommand pText
void MCSpeechDispatchUnrecognizedInput(MCStringRef p_text)
{
    if (!_ensure_message_names())
        return;
    if (MCdefaultstackptr == nil)
        return;
    MCCard *t_card = MCdefaultstackptr->getcurcard();
    if (t_card == nil)
        return;

    MCParameter t_param;
    t_param.setvalueref_argument(p_text);
    t_card->message(s_msg_unrecognized_input, &t_param);
}

////////////////////////////////////////////////////////////////////////////////
// registerVoiceCommand phrases
//
//   registerVoiceCommand "open stack"
//   registerVoiceCommand "open stack" & return & "close stack" & return & "save stack"

MCRegisterVoiceCommand::~MCRegisterVoiceCommand()
{
    delete m_phrases;
}

Parse_stat MCRegisterVoiceCommand::parse(MCScriptPoint& sp)
{
    initpoint(sp);

    if (sp.parseexp(False, True, &m_phrases) != PS_NORMAL)
    {
        MCperror->add(PE_REGISTERVOICECOMMAND_BADPHRASE, sp);
        return PS_ERROR;
    }

    return PS_NORMAL;
}

void MCRegisterVoiceCommand::exec_ctxt(MCExecContext& ctxt)
{
    MCAutoStringRef t_phrases;
    if (!ctxt.EvalExprAsStringRef(m_phrases, EE_REGISTERVOICECOMMAND_BADPHRASE, &t_phrases))
        return;

    _register_phrases(*t_phrases, MCdefaultstackptr);
    _rebuild_platform_list();
}

////////////////////////////////////////////////////////////////////////////////
// unregisterVoiceCommand phrase

MCUnregisterVoiceCommand::~MCUnregisterVoiceCommand()
{
    delete m_phrase;
}

Parse_stat MCUnregisterVoiceCommand::parse(MCScriptPoint& sp)
{
    initpoint(sp);

    if (sp.parseexp(False, True, &m_phrase) != PS_NORMAL)
    {
        MCperror->add(PE_UNREGISTERVOICECOMMAND_BADPHRASE, sp);
        return PS_ERROR;
    }

    return PS_NORMAL;
}

void MCUnregisterVoiceCommand::exec_ctxt(MCExecContext& ctxt)
{
    MCAutoStringRef t_phrase;
    if (!ctxt.EvalExprAsStringRef(m_phrase, EE_UNREGISTERVOICECOMMAND_BADPHRASE, &t_phrase))
        return;

    intptr_t t_idx = _find_phrase(*t_phrase);
    if (t_idx < 0)
        return; // no-op if not registered

    _remove_entry((uindex_t)t_idx);
    _rebuild_platform_list();
}

////////////////////////////////////////////////////////////////////////////////
// unregisterAllVoiceCommands

Parse_stat MCUnregisterAllVoiceCommands::parse(MCScriptPoint& sp)
{
    initpoint(sp);
    return PS_NORMAL;
}

void MCUnregisterAllVoiceCommands::exec_ctxt(MCExecContext& ctxt)
{
    for (uindex_t i = 0; i < s_entry_count; i++)
        MCValueRelease(s_entries[i].phrase);

    MCMemoryDeleteArray(s_entries);
    s_entries     = nullptr;
    s_entry_count = 0;

    MCPlatformSetVoiceCommands(kMCEmptyString);
}

////////////////////////////////////////////////////////////////////////////////
// startListening [language]
//
//   startListening
//   startListening "fr-FR"

MCStartListening::~MCStartListening()
{
    delete m_language;
}

Parse_stat MCStartListening::parse(MCScriptPoint& sp)
{
    initpoint(sp);

    // Language is optional — peek at the next token to see if there's more.
    Symbol_type t_type;
    if (sp.next(t_type) == PS_NORMAL && t_type != ST_EOL && t_type != ST_EOF)
    {
        sp.backup();
        if (sp.parseexp(False, True, &m_language) != PS_NORMAL)
        {
            MCperror->add(PE_STARTLISTENING_BADLANGUAGE, sp);
            return PS_ERROR;
        }
    }

    return PS_NORMAL;
}

void MCStartListening::exec_ctxt(MCExecContext& ctxt)
{
    MCAutoStringRef t_language;

    if (m_language != nil)
    {
        if (!ctxt.EvalExprAsStringRef(m_language, EE_STARTLISTENING_BADLANGUAGE, &t_language))
            return;
    }

    // Pass nil → platform uses system locale.
    MCStringRef t_lang_arg = (m_language != nil && !MCStringIsEmpty(*t_language))
                                 ? *t_language
                                 : nil;

    if (!MCPlatformStartListening(t_lang_arg))
    {
        // Platform has already set the result string with a descriptive error.
    }
}

////////////////////////////////////////////////////////////////////////////////
// stopListening

Parse_stat MCStopListening::parse(MCScriptPoint& sp)
{
    initpoint(sp);
    return PS_NORMAL;
}

void MCStopListening::exec_ctxt(MCExecContext& ctxt)
{
    MCPlatformStopListening();
}

////////////////////////////////////////////////////////////////////////////////
// setWakeWord word [, timeoutMS]
//
//   setWakeWord "hey hyper"
//   setWakeWord "hey hyper", 8000
//   setWakeWord ""           -- clears the wake word

MCSetWakeWord::~MCSetWakeWord()
{
    delete m_word;
    delete m_timeout;
}

Parse_stat MCSetWakeWord::parse(MCScriptPoint& sp)
{
    initpoint(sp);

    // Required: wake word expression.
    if (sp.parseexp(False, False, &m_word) != PS_NORMAL)
    {
        MCperror->add(PE_SETWAKEWORD_BADWORD, sp);
        return PS_ERROR;
    }

    // Optional: timeout in milliseconds (may be preceded by a comma/separator).
    Symbol_type t_type;
    if (sp.next(t_type) == PS_NORMAL && t_type != ST_EOL && t_type != ST_EOF)
    {
        // If the token was not a separator/comma, put it back so parseexp can
        // consume it as the start of the timeout expression.
        if (t_type != ST_SEP)
            sp.backup();

        if (sp.parseexp(False, True, &m_timeout) != PS_NORMAL)
        {
            MCperror->add(PE_SETWAKEWORD_BADTIMEOUT, sp);
            return PS_ERROR;
        }
    }

    return PS_NORMAL;
}

void MCSetWakeWord::exec_ctxt(MCExecContext& ctxt)
{
    MCAutoStringRef t_word;
    if (!ctxt.EvalExprAsStringRef(m_word, EE_SETWAKEWORD_BADWORD, &t_word))
        return;

    uint32_t t_timeout_ms = 5000; // default 5 seconds

    if (m_timeout != nil)
    {
        integer_t t_val;
        if (!ctxt.EvalExprAsInt(m_timeout, EE_SETWAKEWORD_BADTIMEOUT, t_val))
            return;
        if (t_val > 0)
            t_timeout_ms = (uint32_t)t_val;
    }

    MCPlatformSetWakeWord(*t_word, t_timeout_ms);
}
